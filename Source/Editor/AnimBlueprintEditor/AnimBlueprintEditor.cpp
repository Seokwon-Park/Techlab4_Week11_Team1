#include "EnginePCH.h"
#include "AnimBlueprintEditor.h"
#include "AnimBlueprintEditorTab.h"

#include <cmath>
#include <cctype>

#include "Asset/AssetManager.h"
#include "Asset/AssetUtils.h"
#include "Render/SkeletalMesh.h"
#include "UObject/Casts.h"
#include "UObject/UObjectGlobals.h"

#include "Animation/AnimGraphInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"

#include "Animation/AnimNode_BlendSpacePlayer.h"
#include "Animation/AnimNode_TwoWayBlend.h"

#include "Components/SkeletalMeshComponent.h"

FAnimBlueprintEditor::FAnimBlueprintEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager, const FString& InPreviewMeshPath)
    : FSkeletalPreviewEditor(InEditorEngine, InEditorManager, InPreviewMeshPath + ".animblueprint-preview")
{
    BlueprintData.PreviewMeshPath = InPreviewMeshPath;
}

FAnimBlueprintEditor::~FAnimBlueprintEditor()
{
}

bool FAnimBlueprintEditor::InitializeResources()
{
    const FString& PreviewPath = BlueprintData.PreviewMeshPath;
    const FString FilePath = FAssetUtils::ResolveAssetFilePath(PreviewPath);

    FString Error;
    Mesh = Cast<USkeletalMesh>(UAssetManager::LoadFbxMesh(FilePath.empty() ? PreviewPath : FilePath, Error));

    if (!Mesh)
    {
        HTR_LOG(Warning, "AnimBlueprint preview mesh failed: {} ({})", PreviewPath, Error);
        return false;
    }

    if (!InitializePreview())
    {
        return false;
    }

    CreateInitialGraph();
    return true;
}

TUniquePtr<IEditorTab> FAnimBlueprintEditor::CreateTab()
{
    return MakeUnique<FAnimBlueprintEditorTab>(*this);
}

void FAnimBlueprintEditor::ShutdownResources()
{
    bApplyPreviewRequested = false;

    if (MeshComponent && PreviewInstance && MeshComponent->GetAnimInstance() == PreviewInstance.get())
    {
        MeshComponent->SetAnimInstance(nullptr);
    }

    PreviewInstance.reset();
    PreviewNodeIndices.Empty();
    AppliedVariables.Empty();
    PreviewMessage.clear();

    FSkeletalPreviewEditor::ShutdownResources();
}

void FAnimBlueprintEditor::RequestApplyPreview()
{
    bApplyPreviewRequested = true;
}

void FAnimBlueprintEditor::TickBeforeWorld(float DeltaTime)
{
    if (bApplyPreviewRequested)
    {
        bApplyPreviewRequested = false;
        ApplyPreview();
    }
}

bool FAnimBlueprintEditor::SetPreviewError(int32 NodeId, const FString& Message)
{
    PreviewMessage = Message;

    if (NodeId != INDEX_NONE)
    {
        PreviewMessage += " (Node " + std::to_string(NodeId) + ")";
    }

    return false;
}

bool FAnimBlueprintEditor::ValidateSequence(UAnimSequence* Sequence, int32 NodeId)
{
    if (!Sequence || !Sequence->GetDataModel())
    {
        return SetPreviewError(NodeId, "Failed to load animation.");
    }

    const UAnimDataModel* Model = Sequence->GetDataModel();
    const float Length = Model->GetPlayLength();

    if (!std::isfinite(Length) || Length <= 0.0f || Model->GetBoneAnimationTracks().IsEmpty())
    {
        return SetPreviewError(NodeId, "The animation has no playable bone tracks.");
    }

    for (const FBoneAnimationTrack& Track : Model->GetBoneAnimationTracks())
    {
        if (Mesh->FindBoneIndex(Track.Name) < 0)
        {
            return SetPreviewError(NodeId, "Animation bone does not match the preview mesh: " + Track.Name);
        }
    }

    return true;
}

bool FAnimBlueprintEditor::ValidateFloatInput(const FAnimBlueprintNode& Node)
{
    const FAnimBlueprintFloatInput& Input = Node.FloatInput;

    if (!Input.bUseVariable)
    {
        if (!std::isfinite(Input.Constant))
        {
            return SetPreviewError(Node.Id, "The input constant must be finite.");
        }

        if (Node.Type == EAnimBlueprintNodeType::TwoWayBlend && (Input.Constant < 0.0f || Input.Constant > 1.0f))
        {
            return SetPreviewError(Node.Id, "Alpha must be between zero and one.");
        }

        return true;
    }

    for (const FAnimBlueprintVariable& Variable : BlueprintData.Variables)
    {
        if (Variable.Name == Input.Variable && Variable.Type == EAnimBlueprintVariableType::Float)
        {
            return true;
        }
    }

    return SetPreviewError(Node.Id, "Select a valid Float variable for the input.");
}

int32 FAnimBlueprintEditor::BuildPreviewInput(const FAnimBlueprintGraph& Graph, int32 NodeId, int32 PinId, UAnimGraphInstance& Instance, TArray<int32>& Visiting, TMap<int32, TArray<int32>>& NodeIndices)
{
    const FAnimBlueprintLink* InputLink = nullptr;

    for (const FAnimBlueprintLink& Link : Graph.Links)
    {
        if (Link.ToNodeId == NodeId && Link.ToPinId == PinId)
        {
            if (InputLink)
            {
                SetPreviewError(NodeId, "A pose input has multiple connections.");
                return INDEX_NONE;
            }

            InputLink = &Link;
        }
    }

    if (!InputLink || PinId <= 0)
    {
        SetPreviewError(NodeId, "Connect every required pose input.");
        return INDEX_NONE;
    }

    for (const FAnimBlueprintNode& Source : Graph.Nodes)
    {
        if (Source.Id == InputLink->FromNodeId && Source.OutputPosePinId > 0 && Source.OutputPosePinId == InputLink->FromPinId)
        {
            return BuildPreviewNode(Graph, Source.Id, Instance, Visiting, NodeIndices);
        }
    }

    SetPreviewError(NodeId, "The connected pose output is invalid.");
    return INDEX_NONE;
}

int32 FAnimBlueprintEditor::BuildPreviewNode(const FAnimBlueprintGraph& Graph, int32 NodeId, UAnimGraphInstance& Instance, TArray<int32>& Visiting, TMap<int32, TArray<int32>>& NodeIndices)
{
    if (Visiting.Find(NodeId) != INDEX_NONE)
    {
        SetPreviewError(NodeId, "A pose graph cannot contain a cycle.");
        return INDEX_NONE;
    }

    const FAnimBlueprintNode* Node = nullptr;

    for (const FAnimBlueprintNode& Candidate : Graph.Nodes)
    {
        if (Candidate.Id == NodeId)
        {
            Node = &Candidate;
            break;
        }
    }

    if (!Node)
    {
        SetPreviewError(NodeId, "The connected node no longer exists.");
        return INDEX_NONE;
    }

    Visiting.Add(NodeId);
    int32 RuntimeIndex = INDEX_NONE;

    if (Node->Type == EAnimBlueprintNodeType::SequencePlayer || Node->Type == EAnimBlueprintNodeType::BlendSpacePlayer)
    {
        if (!std::isfinite(Node->PlayRate) || Node->PlayRate < 0.0f)
        {
            SetPreviewError(NodeId, "Play Rate must be finite and non-negative.");
            return INDEX_NONE;
        }
    }

    switch (Node->Type)
    {
    case EAnimBlueprintNodeType::OutputPose:
        RuntimeIndex = BuildPreviewInput(Graph, NodeId, Node->InputPosePinId, Instance, Visiting, NodeIndices);
        break;

    case EAnimBlueprintNodeType::SequencePlayer:
    {
        if (Node->SequencePath.empty())
        {
            SetPreviewError(NodeId, "Select an animation for the Sequence Player.");
            return INDEX_NONE;
        }

        UAnimSequence* Sequence = Instance.LoadSequence(Node->SequencePath);

        if (!ValidateSequence(Sequence, NodeId))
        {
            return INDEX_NONE;
        }

        RuntimeIndex = Instance.AddSequencePlayer(Sequence, Node->bLooping, Node->PlayRate);
        break;
    }

    case EAnimBlueprintNodeType::BlendSpacePlayer:
    {
        if (!ValidateFloatInput(*Node))
        {
            return INDEX_NONE;
        }

        if (Node->BlendSpacePath.empty())
        {
            SetPreviewError(NodeId, "Select a BlendSpace for the BlendSpace Player.");
            return INDEX_NONE;
        }

        UBlendSpace1D* BlendSpace = Instance.LoadBlendSpace(Node->BlendSpacePath);

        if (!BlendSpace || BlendSpace->GetSampleData().IsEmpty())
        {
            SetPreviewError(NodeId, "The BlendSpace could not be loaded or has no samples.");
            return INDEX_NONE;
        }

        for (const FBlendSample& Sample : BlendSpace->GetSampleData())
        {
            if (!std::isfinite(Sample.SampleValue.X) || !ValidateSequence(Sample.Animation, NodeId))
            {
                SetPreviewError(NodeId, "The BlendSpace contains invalid or incompatible samples.");
                return INDEX_NONE;
            }
        }

        const FString Variable = Node->FloatInput.bUseVariable ? Node->FloatInput.Variable : FString();
        RuntimeIndex = Instance.AddBlendSpacePlayer(BlendSpace, Variable, Node->PlayRate);

        FAnimNode_BlendSpacePlayer* Player = Instance.GetNode<FAnimNode_BlendSpacePlayer>(RuntimeIndex);
        Player->bLooping = Node->bLooping;
        Player->X = Node->FloatInput.Constant;
        break;
    }

    case EAnimBlueprintNodeType::TwoWayBlend:
    {
        if (!ValidateFloatInput(*Node))
        {
            return INDEX_NONE;
        }

        const int32 NodeA = BuildPreviewInput(Graph, NodeId, Node->InputPosePinId, Instance, Visiting, NodeIndices);

        if (NodeA == INDEX_NONE)
        {
            return INDEX_NONE;
        }

        const int32 NodeB = BuildPreviewInput(Graph, NodeId, Node->SecondInputPosePinId, Instance, Visiting, NodeIndices);

        if (NodeB == INDEX_NONE)
        {
            return INDEX_NONE;
        }

        const FString Variable = Node->FloatInput.bUseVariable ? Node->FloatInput.Variable : FString();
        RuntimeIndex = Instance.AddTwoWayBlend(NodeA, NodeB, Variable);
        Instance.GetNode<FAnimNode_TwoWayBlend>(RuntimeIndex)->Alpha = Node->FloatInput.Constant;
        break;
    }

    default:
        SetPreviewError(NodeId, "This node type is not supported in preview yet.");
        return INDEX_NONE;
    }

    Visiting.RemoveLast();

    if (RuntimeIndex != INDEX_NONE)
    {
        // Each use gets its own runtime node so shared branches do not advance one player twice.
        NodeIndices[NodeId].Add(RuntimeIndex);
    }

    return RuntimeIndex;
}

bool FAnimBlueprintEditor::ApplyPreview()
{
    if (!MeshComponent || !Mesh)
    {
        return SetPreviewError(INDEX_NONE, "The preview mesh is not available.");
    }

    const FAnimBlueprintGraph* RootGraph = nullptr;

    for (const FAnimBlueprintGraph& Graph : BlueprintData.Graphs)
    {
        if (Graph.Id == BlueprintData.RootGraphId)
        {
            RootGraph = &Graph;
            break;
        }
    }

    if (!RootGraph || RootGraph->Type != EAnimBlueprintGraphType::Pose)
    {
        return SetPreviewError(INDEX_NONE, "The root pose graph is missing.");
    }

    int32 OutputNodeId = INDEX_NONE;

    for (const FAnimBlueprintNode& Node : RootGraph->Nodes)
    {
        if (Node.Type == EAnimBlueprintNodeType::OutputPose)
        {
            if (OutputNodeId != INDEX_NONE)
            {
                return SetPreviewError(Node.Id, "Only one Output Pose is allowed.");
            }

            OutputNodeId = Node.Id;
        }
    }

    if (OutputNodeId == INDEX_NONE)
    {
        return SetPreviewError(INDEX_NONE, "Output Pose is missing.");
    }

    TUniquePtr<UAnimGraphInstance> NewInstance(NewObject<UAnimGraphInstance>(MeshComponent));

    for (const FAnimBlueprintVariable& Variable : BlueprintData.Variables)
    {
        if (Variable.Type == EAnimBlueprintVariableType::Float)
        {
            if (!std::isfinite(Variable.DefaultFloat))
            {
                return SetPreviewError(INDEX_NONE, "A Float default is not finite: " + Variable.Name);
            }

            NewInstance->SetFloat(Variable.Name, Variable.DefaultFloat);
        }
        else
        {
            NewInstance->SetBool(Variable.Name, Variable.bDefaultBool);
        }
    }

    TArray<int32> Visiting;
    TMap<int32, TArray<int32>> NewNodeIndices;
    const int32 RootIndex = BuildPreviewNode(*RootGraph, OutputNodeId, *NewInstance, Visiting, NewNodeIndices);

    if (RootIndex == INDEX_NONE)
    {
        return false;
    }

    if (!NewInstance->SetRoot(RootIndex))
    {
        return SetPreviewError(OutputNodeId, "Failed to initialize the preview graph.");
    }

    // Refresh variable bindings before evaluating the first pose.
    NewInstance->UpdateAnimation(0.0f);
    MeshComponent->SetAnimInstance(NewInstance.get());
    PreviewInstance = std::move(NewInstance);
    PreviewNodeIndices = std::move(NewNodeIndices);
    AppliedVariables = BlueprintData.Variables;

    MeshComponent->RefreshAnimationPose();
    PreviewMessage = "Preview applied. Playback restarted from the beginning.";
    return true;
}

void FAnimBlueprintEditor::CreateInitialGraph()
{
    BlueprintData.Graphs.Empty();
    BlueprintData.Variables.Empty();
    BlueprintData.NextId = 1;
    SelectedNodeId = INDEX_NONE;

    FAnimBlueprintGraph Graph;
    Graph.Id = BlueprintData.AllocateId();
    Graph.Name = "AnimGraph";
    Graph.Type = EAnimBlueprintGraphType::Pose;

    FAnimBlueprintNode Player;
    Player.Id = BlueprintData.AllocateId();
    Player.Type = EAnimBlueprintNodeType::SequencePlayer;
    Player.Name = "Sequence Player";
    Player.Position.X = 40.0f;
    Player.Position.Y = 100.0f;
    Player.OutputPosePinId = BlueprintData.AllocateId();
    Graph.Nodes.Add(Player);

    FAnimBlueprintNode Output;
    Output.Id = BlueprintData.AllocateId();
    Output.Type = EAnimBlueprintNodeType::OutputPose;
    Output.Name = "Output Pose";
    Output.Position.X = 360.0f;
    Output.Position.Y = 100.0f;
    Output.InputPosePinId = BlueprintData.AllocateId();
    Graph.Nodes.Add(Output);

    BlueprintData.RootGraphId = Graph.Id;
    CurrentGraphId = Graph.Id;
    BlueprintData.Graphs.Add(Graph);

    FAnimBlueprintVariable GroundSpeed;
    GroundSpeed.Name = "GroundSpeed";
    GroundSpeed.Type = EAnimBlueprintVariableType::Float;
    BlueprintData.Variables.Add(GroundSpeed);
}

FAnimBlueprintGraph* FAnimBlueprintEditor::GetCurrentGraph()
{
    for (FAnimBlueprintGraph& Graph : BlueprintData.Graphs)
    {
        if (Graph.Id == CurrentGraphId)
        {
            return &Graph;
        }
    }

    return nullptr;
}

FAnimBlueprintNode* FAnimBlueprintEditor::GetSelectedNode()
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return nullptr;
    }

    for (FAnimBlueprintNode& Node : Graph->Nodes)
    {
        if (Node.Id == SelectedNodeId)
        {
            return &Node;
        }
    }

    return nullptr;
}

void FAnimBlueprintEditor::SelectNode(int32 NodeId)
{
    SelectedNodeId = INDEX_NONE;

    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return;
    }

    for (const FAnimBlueprintNode& Node : Graph->Nodes)
    {
        if (Node.Id == NodeId)
        {
            SelectedNodeId = NodeId;
            return;
        }
    }
}

int32 FAnimBlueprintEditor::AddSequencePlayer(const FAnimBlueprintPosition& Position)
{
    return AddPoseNode(EAnimBlueprintNodeType::SequencePlayer, Position);
}

int32 FAnimBlueprintEditor::AddPoseNode(EAnimBlueprintNodeType Type, const FAnimBlueprintPosition& Position)
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph || Graph->Type != EAnimBlueprintGraphType::Pose)
    {
        return INDEX_NONE;
    }

    FAnimBlueprintNode Node;
    Node.Type = Type;
    Node.Position = Position;

    switch (Type)
    {
    case EAnimBlueprintNodeType::SequencePlayer:
        Node.Name = "Sequence Player";
        break;

    case EAnimBlueprintNodeType::BlendSpacePlayer:
        Node.Name = "BlendSpace Player";
        break;

    case EAnimBlueprintNodeType::TwoWayBlend:
        Node.Name = "Two Way Blend";
        Node.InputPosePinId = BlueprintData.AllocateId();
        Node.SecondInputPosePinId = BlueprintData.AllocateId();
        Node.FloatInput.Constant = 0.5f;
        break;

    default:
        return INDEX_NONE;
    }

    Node.Id = BlueprintData.AllocateId();
    Node.OutputPosePinId = BlueprintData.AllocateId();
    const int32 NodeId = Node.Id;
    Graph->Nodes.Add(Node);
    return NodeId;
}

FAnimBlueprintNode* FAnimBlueprintEditor::FindNodeByPin(int32 PinId)
{
    if (PinId <= 0)
    {
        return nullptr;
    }

    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return nullptr;
    }

    for (FAnimBlueprintNode& Node : Graph->Nodes)
    {
        if (Node.InputPosePinId == PinId || Node.SecondInputPosePinId == PinId || Node.OutputPosePinId == PinId)
        {
            return &Node;
        }
    }

    return nullptr;
}

bool FAnimBlueprintEditor::ResolvePoseConnection(int32 FirstPinId, int32 SecondPinId, FAnimBlueprintLink& OutConnection)
{
    FAnimBlueprintNode* FirstNode = FindNodeByPin(FirstPinId);
    FAnimBlueprintNode* SecondNode = FindNodeByPin(SecondPinId);

    if (!FirstNode || !SecondNode || FirstNode == SecondNode)
    {
        return false;
    }

    const bool bFirstIsOutput = FirstNode->OutputPosePinId == FirstPinId;
    const bool bSecondIsOutput = SecondNode->OutputPosePinId == SecondPinId;

    if (bFirstIsOutput == bSecondIsOutput)
    {
        return false;
    }

    FAnimBlueprintNode* SourceNode = FirstNode;
    FAnimBlueprintNode* TargetNode = SecondNode;
    int32 SourcePinId = FirstPinId;
    int32 TargetPinId = SecondPinId;

    if (!bFirstIsOutput)
    {
        SourceNode = SecondNode;
        TargetNode = FirstNode;
        SourcePinId = SecondPinId;
        TargetPinId = FirstPinId;
    }

    if (SourceNode->OutputPosePinId <= 0 || SourceNode->OutputPosePinId != SourcePinId)
    {
        return false;
    }

    if (TargetNode->Type != EAnimBlueprintNodeType::OutputPose && TargetNode->Type != EAnimBlueprintNodeType::TwoWayBlend)
    {
        return false;
    }

    if (HasPosePath(TargetNode->Id, SourceNode->Id))
    {
        return false;
    }

    OutConnection.FromNodeId = SourceNode->Id;
    OutConnection.FromPinId = SourcePinId;
    OutConnection.ToNodeId = TargetNode->Id;
    OutConnection.ToPinId = TargetPinId;

    return true;
}

bool FAnimBlueprintEditor::CanConnectPosePins(int32 FirstPinId, int32 SecondPinId)
{
    FAnimBlueprintLink Connection;
    return ResolvePoseConnection(FirstPinId, SecondPinId, Connection);
}

bool FAnimBlueprintEditor::ConnectPosePins(int32 FirstPinId, int32 SecondPinId)
{
    FAnimBlueprintLink Connection;

    if (!ResolvePoseConnection(FirstPinId, SecondPinId, Connection))
    {
        return false;
    }

    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return false;
    }

    for (int32 Index = Graph->Links.Num() - 1; Index >= 0; --Index)
    {
        if (Graph->Links[Index].ToPinId == Connection.ToPinId)
        {
            Graph->Links.RemoveAt(Index);
        }
    }

    Connection.Id = BlueprintData.AllocateId();
    Graph->Links.Add(Connection);

    return true;
}

bool FAnimBlueprintEditor::CanRemoveNode(int32 NodeId)
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return false;
    }

    for (const FAnimBlueprintNode& Node : Graph->Nodes)
    {
        if (Node.Id == NodeId)
        {
            return Node.Type == EAnimBlueprintNodeType::SequencePlayer || Node.Type == EAnimBlueprintNodeType::BlendSpacePlayer || Node.Type == EAnimBlueprintNodeType::TwoWayBlend;
        }
    }

    return false;
}

bool FAnimBlueprintEditor::RemoveNode(int32 NodeId)
{
    if (!CanRemoveNode(NodeId))
    {
        return false;
    }

    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return false;
    }

    for (int32 Index = Graph->Links.Num() - 1; Index >= 0; --Index)
    {
        const FAnimBlueprintLink& Link = Graph->Links[Index];

        if (Link.FromNodeId == NodeId || Link.ToNodeId == NodeId)
        {
            Graph->Links.RemoveAt(Index);
        }
    }

    for (int32 Index = 0; Index < Graph->Nodes.Num(); ++Index)
    {
        if (Graph->Nodes[Index].Id == NodeId)
        {
            Graph->Nodes.RemoveAt(Index);

            if (SelectedNodeId == NodeId)
            {
                SelectedNodeId = INDEX_NONE;
            }

            return true;
        }
    }

    return false;
}

bool FAnimBlueprintEditor::RemoveLink(int32 LinkId)
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return false;
    }

    for (int32 Index = 0; Index < Graph->Links.Num(); ++Index)
    {
        if (Graph->Links[Index].Id == LinkId)
        {
            Graph->Links.RemoveAt(Index);
            return true;
        }
    }

    return false;
}

bool FAnimBlueprintEditor::HasPosePath(int32 FromNodeId, int32 ToNodeId)
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return false;
    }

    TArray<int32> Pending;
    TArray<int32> Visited;
    Pending.Add(FromNodeId);

    while (!Pending.IsEmpty())
    {
        const int32 NodeId = Pending[Pending.Num() - 1];
        Pending.RemoveLast();

        if (NodeId == ToNodeId)
        {
            return true;
        }

        if (Visited.Find(NodeId) != INDEX_NONE)
        {
            continue;
        }

        Visited.Add(NodeId);

        for (const FAnimBlueprintLink& Link : Graph->Links)
        {
            if (Link.FromNodeId == NodeId)
            {
                Pending.Add(Link.ToNodeId);
            }
        }
    }

    return false;
}

bool FAnimBlueprintEditor::ValidateVariableName(const FString& Name, const FString& PreviousName)
{
    if (Name.empty() || Name.size() >= 128)
    {
        VariableMessage = "Use a variable name with 1 to 127 characters.";
        return false;
    }

    if (!std::isalpha(static_cast<unsigned char>(Name[0])) && Name[0] != '_')
    {
        VariableMessage = "Start the name with a letter or underscore.";
        return false;
    }

    for (unsigned char Character : Name)
    {
        if (!std::isalnum(Character) && Character != '_')
        {
            VariableMessage = "Use letters, numbers and underscores.";
            return false;
        }
    }

    for (const FAnimBlueprintVariable& Variable : BlueprintData.Variables)
    {
        if (Variable.Name == Name && Variable.Name != PreviousName)
        {
            VariableMessage = "A variable with this name already exists.";
            return false;
        }
    }

    VariableMessage.clear();
    return true;
}

bool FAnimBlueprintEditor::AddVariable(const FString& Name, EAnimBlueprintVariableType Type)
{
    if (!ValidateVariableName(Name, FString()))
    {
        return false;
    }

    FAnimBlueprintVariable Variable;
    Variable.Name = Name;
    Variable.Type = Type;
    BlueprintData.Variables.Add(Variable);
    return true;
}

bool FAnimBlueprintEditor::RenameVariable(const FString& PreviousName, const FString& Name)
{
    if (!ValidateVariableName(Name, PreviousName))
    {
        return false;
    }

    for (FAnimBlueprintVariable& Variable : BlueprintData.Variables)
    {
        if (Variable.Name == PreviousName)
        {
            Variable.Name = Name;

            for (FAnimBlueprintGraph& Graph : BlueprintData.Graphs)
            {
                for (FAnimBlueprintNode& Node : Graph.Nodes)
                {
                    if (Node.FloatInput.Variable == PreviousName)
                    {
                        Node.FloatInput.Variable = Name;
                    }
                }
            }

            return true;
        }
    }

    return false;
}

void FAnimBlueprintEditor::RemoveVariable(const FString& Name)
{
    for (int32 Index = 0; Index < BlueprintData.Variables.Num(); ++Index)
    {
        if (BlueprintData.Variables[Index].Name == Name)
        {
            BlueprintData.Variables.RemoveAt(Index);
            break;
        }
    }

    for (FAnimBlueprintGraph& Graph : BlueprintData.Graphs)
    {
        for (FAnimBlueprintNode& Node : Graph.Nodes)
        {
            if (Node.FloatInput.Variable == Name)
            {
                Node.FloatInput.Variable.clear();
                Node.FloatInput.bUseVariable = false;
            }
        }
    }

    VariableMessage = "Variable removed. Bound inputs now use constants. Apply Preview to update playback.";
}

float FAnimBlueprintEditor::GetPreviewFloat(const FString& Name) const
{
    return PreviewInstance ? PreviewInstance->GetFloat(Name) : 0.0f;
}

bool FAnimBlueprintEditor::GetPreviewBool(const FString& Name) const
{
    return PreviewInstance ? PreviewInstance->GetBool(Name) : false;
}

void FAnimBlueprintEditor::SetPreviewFloat(const FString& Name, float Value)
{
    if (PreviewInstance && std::isfinite(Value))
    {
        PreviewInstance->SetFloat(Name, Value);
    }
}

void FAnimBlueprintEditor::SetPreviewBool(const FString& Name, bool bValue)
{
    if (PreviewInstance)
    {
        PreviewInstance->SetBool(Name, bValue);
    }
}
