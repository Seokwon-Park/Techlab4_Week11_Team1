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
#include "Core/NameTypes.h"

#include "Animation/AnimNode_BlendSpacePlayer.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimNode_TwoWayBlend.h"

#include "Components/SkeletalMeshComponent.h"

FAnimBlueprintEditor::FAnimBlueprintEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager, 
    const FString& InPreviewMeshPath)
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
        PreviewMessage += " (ID " + std::to_string(NodeId) + ")";
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

int32 FAnimBlueprintEditor::BuildPreviewInput(const FAnimBlueprintGraph& Graph, int32 NodeId, 
    int32 PinId, UAnimGraphInstance& Instance, TArray<int32>& Visiting, TMap<int32, TArray<int32>>& NodeIndices)
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
        SetPreviewError(NodeId, "Connect every required pose input in graph: " + Graph.Name);
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
    case EAnimBlueprintNodeType::StateResult:
        RuntimeIndex = BuildPreviewInput(Graph, NodeId, Node->InputPosePinId, Instance, Visiting, NodeIndices);
        break;

    case EAnimBlueprintNodeType::StateMachine:
        RuntimeIndex = BuildPreviewStateMachine(Graph, *Node, Instance, Visiting, NodeIndices);
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

int32 FAnimBlueprintEditor::BuildPreviewPoseGraph(const FAnimBlueprintGraph& Graph, EAnimBlueprintNodeType ResultType, 
    UAnimGraphInstance& Instance, TArray<int32>& Visiting, TMap<int32, TArray<int32>>& NodeIndices)
{
    if (Graph.Type != EAnimBlueprintGraphType::Pose)
    {
        SetPreviewError(Graph.OwnerNodeId, "A state must contain a pose graph.");
        return INDEX_NONE;
    }

    const FAnimBlueprintNode* ResultNode = nullptr;

    for (const FAnimBlueprintNode& Node : Graph.Nodes)
    {
        if (Node.Type == EAnimBlueprintNodeType::OutputPose || Node.Type == EAnimBlueprintNodeType::StateResult)
        {
            if (Node.Type != ResultType || ResultNode)
            {
                SetPreviewError(Node.Id, "The pose graph must have exactly one matching result node.");
                return INDEX_NONE;
            }

            ResultNode = &Node;
        }
    }

    if (!ResultNode)
    {
        SetPreviewError(Graph.OwnerNodeId, "The pose graph result is missing: " + Graph.Name);
        return INDEX_NONE;
    }

    return BuildPreviewNode(Graph, ResultNode->Id, Instance, Visiting, NodeIndices);
}

int32 FAnimBlueprintEditor::BuildPreviewStateMachine(const FAnimBlueprintGraph& ParentGraph, const FAnimBlueprintNode& MachineNode, UAnimGraphInstance& Instance, TArray<int32>& Visiting, TMap<int32, TArray<int32>>& NodeIndices)
{
    const FAnimBlueprintGraph* MachineGraph = FindGraph(MachineNode.ChildGraphId);

    if (!MachineGraph || MachineGraph->Type != EAnimBlueprintGraphType::StateMachine || MachineGraph->ParentGraphId != ParentGraph.Id || MachineGraph->OwnerNodeId != MachineNode.Id)
    {
        SetPreviewError(MachineNode.Id, "The State Machine graph is missing or belongs to another node.");
        return INDEX_NONE;
    }

    const FAnimBlueprintNode* Entry = nullptr;
    int32 StateCount = 0;
    int32 AnyStateCount = 0;

    for (const FAnimBlueprintNode& Node : MachineGraph->Nodes)
    {
        if (Node.Type == EAnimBlueprintNodeType::Entry)
        {
            if (Entry)
            {
                SetPreviewError(Node.Id, "A State Machine must have exactly one Entry.");
                return INDEX_NONE;
            }

            Entry = &Node;
        }
        else if (Node.Type == EAnimBlueprintNodeType::State)
        {
            ++StateCount;
        }
        else if (Node.Type == EAnimBlueprintNodeType::AnyState)
        {
            ++AnyStateCount;

            if (AnyStateCount > 1)
            {
                SetPreviewError(Node.Id, "Only one Any State node is allowed.");
                return INDEX_NONE;
            }
        }
        else
        {
            SetPreviewError(Node.Id, "Only Entry, State and Any State nodes are allowed inside a State Machine.");
            return INDEX_NONE;
        }
    }

    if (!Entry || Entry->TransitionOutputPinId <= 0 || StateCount == 0)
    {
        SetPreviewError(MachineNode.Id, "A State Machine needs an Entry and at least one State.");
        return INDEX_NONE;
    }

    if (MachineGraph->Links.Num() != 1)
    {
        SetPreviewError(Entry->Id, "Connect Entry to exactly one initial State.");
        return INDEX_NONE;
    }

    const FAnimBlueprintLink& InitialLink = MachineGraph->Links[0];

    if (InitialLink.FromNodeId != Entry->Id || InitialLink.FromPinId != Entry->TransitionOutputPinId)
    {
        SetPreviewError(Entry->Id, "The initial State connection must start at Entry.");
        return INDEX_NONE;
    }

    const int32 MachineIndex = Instance.AddStateMachine();
    TMap<int32, int32> StateIndices;
    TArray<FName> StateNames;

    for (const FAnimBlueprintNode& State : MachineGraph->Nodes)
    {
        if (State.Type != EAnimBlueprintNodeType::State)
        {
            continue;
        }

        const FName StateName(State.Name);

        if (State.Name.find_first_not_of(" \t\r\n") == FString::npos || StateNames.Find(StateName) != INDEX_NONE)
        {
            SetPreviewError(State.Id, "State names must be non-empty and unique within the State Machine.");
            return INDEX_NONE;
        }

        StateNames.Add(StateName);
        const FAnimBlueprintGraph* StateGraph = FindGraph(State.ChildGraphId);

        if (!StateGraph || StateGraph->ParentGraphId != MachineGraph->Id || StateGraph->OwnerNodeId != State.Id)
        {
            SetPreviewError(State.Id, "The State pose graph is missing or belongs to another node.");
            return INDEX_NONE;
        }

        const int32 PoseIndex = BuildPreviewPoseGraph(*StateGraph, EAnimBlueprintNodeType::StateResult, 
            Instance, Visiting, NodeIndices);

        if (PoseIndex == INDEX_NONE)
        {
            return INDEX_NONE;
        }

        const int32 StateIndex = Instance.AddState(MachineIndex, StateName, PoseIndex, State.bResetOnEntry);

        if (StateIndex == INDEX_NONE)
        {
            SetPreviewError(State.Id, "Failed to register the State pose.");
            return INDEX_NONE;
        }

        StateIndices.Add(State.Id, StateIndex);
        NodeIndices[State.Id].Add(PoseIndex);

        if (State.Id == InitialLink.ToNodeId)
        {
            if (State.TransitionInputPinId <= 0 || InitialLink.ToPinId != State.TransitionInputPinId)
            {
                SetPreviewError(State.Id, "Entry is connected to an invalid State input pin.");
                return INDEX_NONE;
            }
        }
    }

    const int32* InitialStateIndex = StateIndices.Find(InitialLink.ToNodeId);

    if (!InitialStateIndex || !Instance.SetInitialState(MachineIndex, *InitialStateIndex))
    {
        SetPreviewError(Entry->Id, "Entry must point to a State in this State Machine.");
        return INDEX_NONE;
    }

    if (!BuildPreviewTransitions(*MachineGraph, MachineIndex, Instance, StateIndices))
    {
        return INDEX_NONE;
    }

    return MachineIndex;
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
    const int32 RootIndex = BuildPreviewPoseGraph(*RootGraph, EAnimBlueprintNodeType::OutputPose, *NewInstance, Visiting, NewNodeIndices);

    if (RootIndex == INDEX_NONE)
    {
        return false;
    }

    if (!NewInstance->SetRoot(RootIndex))
    {
        return SetPreviewError(INDEX_NONE, "Failed to initialize the preview graph.");
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
    return FindGraph(CurrentGraphId);
}

FAnimBlueprintGraph* FAnimBlueprintEditor::FindGraph(int32 GraphId)
{
    for (FAnimBlueprintGraph& Graph : BlueprintData.Graphs)
    {
        if (Graph.Id == GraphId)
        {
            return &Graph;
        }
    }

    return nullptr;
}

FAnimBlueprintNode* FAnimBlueprintEditor::FindNode(int32 NodeId)
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return nullptr;
    }

    for (FAnimBlueprintNode& Node : Graph->Nodes)
    {
        if (Node.Id == NodeId)
        {
            return &Node;
        }
    }

    return nullptr;
}

FAnimBlueprintNode* FAnimBlueprintEditor::GetSelectedNode()
{
    return FindNode(SelectedNodeId);
}

void FAnimBlueprintEditor::SelectNode(int32 NodeId)
{
    SelectedNodeId = INDEX_NONE;
    SelectedLinkId = INDEX_NONE;

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

    const int32 ParentGraphId = Graph->Id;

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

    case EAnimBlueprintNodeType::StateMachine:
        Node.Name = "State Machine";
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

    if (Type == EAnimBlueprintNodeType::StateMachine)
    {
        Node.ChildGraphId = CreateChildGraph(ParentGraphId, NodeId, Node.Name, EAnimBlueprintGraphType::StateMachine);
        Graph = FindGraph(ParentGraphId);
    }

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
        if (Node.InputPosePinId == PinId 
            || Node.SecondInputPosePinId == PinId 
            || Node.OutputPosePinId == PinId 
            || Node.TransitionInputPinId == PinId 
            || Node.TransitionOutputPinId == PinId)
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

    if (TargetNode->Type != EAnimBlueprintNodeType::OutputPose 
        && TargetNode->Type != EAnimBlueprintNodeType::StateResult 
        && TargetNode->Type != EAnimBlueprintNodeType::TwoWayBlend)
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
            return Node.Type == EAnimBlueprintNodeType::SequencePlayer 
                || Node.Type == EAnimBlueprintNodeType::BlendSpacePlayer 
                || Node.Type == EAnimBlueprintNodeType::TwoWayBlend 
                || Node.Type == EAnimBlueprintNodeType::StateMachine 
                || Node.Type == EAnimBlueprintNodeType::State || Node.Type == EAnimBlueprintNodeType::AnyState;
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
    int32 ChildGraphId = INDEX_NONE;

    for (int32 Index = Graph->Links.Num() - 1; Index >= 0; --Index)
    {
        const FAnimBlueprintLink& Link = Graph->Links[Index];

        if (Link.FromNodeId == NodeId || Link.ToNodeId == NodeId)
        {
            Graph->Links.RemoveAt(Index);
        }
    }

    for (int32 Index = Graph->Transitions.Num() - 1; Index >= 0; --Index)
    {
        const FAnimBlueprintTransition& Transition = Graph->Transitions[Index];

        if (Transition.FromStateId == NodeId || Transition.ToStateId == NodeId)
        {
            Graph->Transitions.RemoveAt(Index);
        }
    }

    for (int32 Index = 0; Index < Graph->Nodes.Num(); ++Index)
    {
        if (Graph->Nodes[Index].Id == NodeId)
        {
            ChildGraphId = Graph->Nodes[Index].ChildGraphId;
            Graph->Nodes.RemoveAt(Index);
            break;
        }
    }

    SelectedNodeId = INDEX_NONE;
    SelectedLinkId = INDEX_NONE;

    if (ChildGraphId != INDEX_NONE)
    {
        RemoveChildGraphs(ChildGraphId);
    }

    return true;
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
            SelectedLinkId = INDEX_NONE;
            return true;
        }
    }

    for (int32 Index = 0; Index < Graph->Transitions.Num(); ++Index)
    {
        if (Graph->Transitions[Index].Id == LinkId)
        {
            Graph->Transitions.RemoveAt(Index);
            SelectedLinkId = INDEX_NONE;
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
                for (FAnimBlueprintTransition& Transition : Graph.Transitions)
                {
                    if (Transition.Variable == PreviousName)
                    {
                        Transition.Variable = Name;
                    }
                }

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
        for (FAnimBlueprintTransition& Transition : Graph.Transitions)
        {
            if (Transition.Variable == Name)
            {
                Transition.Variable.clear();
                Transition.Rule = EAnimBlueprintTransitionRule::Disabled;
            }
        }

        for (FAnimBlueprintNode& Node : Graph.Nodes)
        {
            if (Node.FloatInput.Variable == Name)
            {
                Node.FloatInput.Variable.clear();
                Node.FloatInput.bUseVariable = false;
            }
        }
    }

    VariableMessage = "Variable removed. Bound inputs use constants and affected transitions are disabled. Apply Preview to update playback.";
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

int32 FAnimBlueprintEditor::CreateChildGraph(int32 ParentGraphId, int32 OwnerNodeId, const FString& Name, 
    EAnimBlueprintGraphType Type)
{
    FAnimBlueprintGraph Graph;
    Graph.Id = BlueprintData.AllocateId();
    Graph.Name = Name;
    Graph.Type = Type;
    Graph.ParentGraphId = ParentGraphId;
    Graph.OwnerNodeId = OwnerNodeId;

    FAnimBlueprintNode Root;
    Root.Id = BlueprintData.AllocateId();
    Root.Position.X = 40.0f;
    Root.Position.Y = 100.0f;

    if (Type == EAnimBlueprintGraphType::StateMachine)
    {
        Root.Type = EAnimBlueprintNodeType::Entry;
        Root.Name = "Entry";
        Root.TransitionOutputPinId = BlueprintData.AllocateId();
    }
    else
    {
        Root.Type = EAnimBlueprintNodeType::StateResult;
        Root.Name = "State Result";
        Root.Position.X = 360.0f;
        Root.InputPosePinId = BlueprintData.AllocateId();
    }

    Graph.Nodes.Add(Root);
    const int32 GraphId = Graph.Id;
    BlueprintData.Graphs.Add(Graph);
    return GraphId;
}

int32 FAnimBlueprintEditor::AddState(const FAnimBlueprintPosition& Position)
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph || Graph->Type != EAnimBlueprintGraphType::StateMachine)
    {
        return INDEX_NONE;
    }

    const int32 ParentGraphId = Graph->Id;
    FAnimBlueprintNode State;
    State.Id = BlueprintData.AllocateId();
    State.Type = EAnimBlueprintNodeType::State;
    State.Name = "State " + std::to_string(State.Id);
    State.Position = Position;
    State.TransitionInputPinId = BlueprintData.AllocateId();
    State.TransitionOutputPinId = BlueprintData.AllocateId();
    State.ChildGraphId = CreateChildGraph(ParentGraphId, State.Id, State.Name, EAnimBlueprintGraphType::Pose);

    Graph = FindGraph(ParentGraphId);
    Graph->Nodes.Add(State);
    return State.Id;
}

TArray<int32> FAnimBlueprintEditor::GetGraphPath() const
{
    TArray<int32> ReversePath;
    int32 GraphId = CurrentGraphId;

    while (GraphId != INDEX_NONE && ReversePath.Find(GraphId) == INDEX_NONE)
    {
        const FAnimBlueprintGraph* Found = nullptr;

        for (const FAnimBlueprintGraph& Graph : BlueprintData.Graphs)
        {
            if (Graph.Id == GraphId)
            {
                Found = &Graph;
                break;
            }
        }

        if (!Found)
        {
            break;
        }

        ReversePath.Add(GraphId);
        GraphId = Found->ParentGraphId;
    }

    TArray<int32> Path;
    for (int32 Index = ReversePath.Num() - 1; Index >= 0; --Index)
    {
        Path.Add(ReversePath[Index]);
    }

    return Path;
}

bool FAnimBlueprintEditor::NavigateToGraph(int32 GraphId)
{
    if (!FindGraph(GraphId))
    {
        return false;
    }

    CurrentGraphId = GraphId;
    SelectedNodeId = INDEX_NONE;
    SelectedLinkId = INDEX_NONE;
    return true;
}

bool FAnimBlueprintEditor::OpenNodeGraph(int32 NodeId)
{
    const FAnimBlueprintNode* Node = FindNode(NodeId);

    if (!Node || (Node->Type != EAnimBlueprintNodeType::StateMachine && Node->Type != EAnimBlueprintNodeType::State))
    {
        return false;
    }

    return NavigateToGraph(Node->ChildGraphId);
}

bool FAnimBlueprintEditor::RenameNode(int32 NodeId, const FString& Name)
{
    FAnimBlueprintNode* Node = FindNode(NodeId);
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Node || !Graph || (Node->Type != EAnimBlueprintNodeType::State && Node->Type != EAnimBlueprintNodeType::StateMachine))
    {
        return false;
    }

    if (Name.size() >= 128 || Name.find_first_not_of(" \t\r\n") == FString::npos)
    {
        return SetPreviewError(NodeId, "Use a non-empty name with fewer than 128 bytes.");
    }

    for (const FAnimBlueprintNode& Other : Graph->Nodes)
    {
        if (Other.Id != NodeId && Other.Name == Name)
        {
            return SetPreviewError(NodeId, "A node with this name already exists in the graph.");
        }
    }

    Node->Name = Name;
    FAnimBlueprintGraph* Child = FindGraph(Node->ChildGraphId);

    if (Child)
    {
        Child->Name = Name;
    }

    return true;
}

void FAnimBlueprintEditor::SelectLink(int32 LinkId)
{
    SelectedNodeId = INDEX_NONE;
    SelectedLinkId = LinkId;
}

FAnimBlueprintTransition* FAnimBlueprintEditor::GetSelectedTransition()
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (Graph)
    {
        for (FAnimBlueprintTransition& Transition : Graph->Transitions)
        {
            if (Transition.Id == SelectedLinkId)
            {
                return &Transition;
            }
        }
    }

    return nullptr;
}

bool FAnimBlueprintEditor::ResolveStateConnection(int32 FirstPinId, int32 SecondPinId, FAnimBlueprintLink& OutConnection)
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();
    FAnimBlueprintNode* First = FindNodeByPin(FirstPinId);
    FAnimBlueprintNode* Second = FindNodeByPin(SecondPinId);

    if (!Graph || Graph->Type != EAnimBlueprintGraphType::StateMachine || !First || !Second || First == Second)
    {
        return false;
    }

    const bool bFirstOutput = First->TransitionOutputPinId == FirstPinId;
    const bool bSecondOutput = Second->TransitionOutputPinId == SecondPinId;

    if (bFirstOutput == bSecondOutput)
    {
        return false;
    }

    const FAnimBlueprintNode* Source = bFirstOutput ? First : Second;
    const FAnimBlueprintNode* Target = bFirstOutput ? Second : First;

    if ((Source->Type != EAnimBlueprintNodeType::Entry 
        && Source->Type != EAnimBlueprintNodeType::State && Source->Type != EAnimBlueprintNodeType::AnyState) 
        || Target->Type != EAnimBlueprintNodeType::State)
    {
        return false;
    }

    for (const FAnimBlueprintTransition& Transition : Graph->Transitions)
    {
        if (Transition.FromStateId == Source->Id && Transition.ToStateId == Target->Id)
        {
            return false;
        }
    }

    OutConnection.FromNodeId = Source->Id;
    OutConnection.FromPinId = Source->TransitionOutputPinId;
    OutConnection.ToNodeId = Target->Id;
    OutConnection.ToPinId = Target->TransitionInputPinId;
    return true;
}

bool FAnimBlueprintEditor::CanConnectPins(int32 FirstPinId, int32 SecondPinId)
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return false;
    }

    if (Graph->Type == EAnimBlueprintGraphType::Pose)
    {
        return CanConnectPosePins(FirstPinId, SecondPinId);
    }

    FAnimBlueprintLink Connection;
    return ResolveStateConnection(FirstPinId, SecondPinId, Connection);
}

bool FAnimBlueprintEditor::ConnectPins(int32 FirstPinId, int32 SecondPinId)
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph)
    {
        return false;
    }

    if (Graph->Type == EAnimBlueprintGraphType::Pose)
    {
        return ConnectPosePins(FirstPinId, SecondPinId);
    }

    FAnimBlueprintLink Connection;

    if (!ResolveStateConnection(FirstPinId, SecondPinId, Connection))
    {
        return false;
    }

    Connection.Id = BlueprintData.AllocateId();
    const FAnimBlueprintNode* Source = FindNode(Connection.FromNodeId);

    if (Source->Type == EAnimBlueprintNodeType::Entry)
    {
        Graph->Links.Empty();
        Graph->Links.Add(Connection);
    }
    else
    {
        FAnimBlueprintTransition Transition;
        Transition.Id = Connection.Id;
        Transition.FromStateId = Connection.FromNodeId;
        Transition.ToStateId = Connection.ToNodeId;
        Transition.FromPinId = Connection.FromPinId;
        Transition.ToPinId = Connection.ToPinId;
        Graph->Transitions.Add(Transition);
    }

    return true;
}

void FAnimBlueprintEditor::RemoveChildGraphs(int32 GraphId)
{
    TArray<int32> RemovedIds;
    RemovedIds.Add(GraphId);

    for (int32 Index = 0; Index < RemovedIds.Num(); ++Index)
    {
        for (const FAnimBlueprintGraph& Graph : BlueprintData.Graphs)
        {
            if (Graph.ParentGraphId == RemovedIds[Index] && RemovedIds.Find(Graph.Id) == INDEX_NONE)
            {
                RemovedIds.Add(Graph.Id);
            }
        }
    }

    for (int32 Index = BlueprintData.Graphs.Num() - 1; Index >= 0; --Index)
    {
        if (RemovedIds.Find(BlueprintData.Graphs[Index].Id) != INDEX_NONE)
        {
            BlueprintData.Graphs.RemoveAt(Index);
        }
    }
}

int32 FAnimBlueprintEditor::AddAnyState(const FAnimBlueprintPosition& Position)
{
    FAnimBlueprintGraph* Graph = GetCurrentGraph();

    if (!Graph || Graph->Type != EAnimBlueprintGraphType::StateMachine)
    {
        return INDEX_NONE;
    }

    for (const FAnimBlueprintNode& Node : Graph->Nodes)
    {
        if (Node.Type == EAnimBlueprintNodeType::AnyState)
        {
            return Node.Id;
        }
    }

    FAnimBlueprintNode Node;
    Node.Id = BlueprintData.AllocateId();
    Node.Name = "Any State";
    Node.Type = EAnimBlueprintNodeType::AnyState;
    Node.Position = Position;
    Node.TransitionOutputPinId = BlueprintData.AllocateId();
    Graph->Nodes.Add(Node);
    return Node.Id;
}

bool FAnimBlueprintEditor::BuildPreviewTransitions(const FAnimBlueprintGraph& Graph, int32 MachineIndex, 
    UAnimGraphInstance& Instance, const TMap<int32, int32>& StateIndices)
{
    for (const FAnimBlueprintTransition& Transition : Graph.Transitions)
    {
        const FAnimBlueprintNode* Source = nullptr;
        const FAnimBlueprintNode* Target = nullptr;

        for (const FAnimBlueprintNode& Node : Graph.Nodes)
        {
            if (Node.Id == Transition.FromStateId)
            {
                Source = &Node;
            }

            if (Node.Id == Transition.ToStateId)
            {
                Target = &Node;
            }
        }

        if (!Source || !Target || Target->Type != EAnimBlueprintNodeType::State || Source == Target)
        {
            return SetPreviewError(Transition.Id, "The transition endpoints are invalid.");
        }

        if ((Source->Type != EAnimBlueprintNodeType::State 
            && Source->Type != EAnimBlueprintNodeType::AnyState) 
            || Source->TransitionOutputPinId != Transition.FromPinId 
            || Target->TransitionInputPinId != Transition.ToPinId)
        {
            return SetPreviewError(Transition.Id, "The transition pins are invalid.");
        }

        if (Transition.Rule == EAnimBlueprintTransitionRule::Disabled)
        {
            continue;
        }

        if (!std::isfinite(Transition.CrossfadeDuration) || Transition.CrossfadeDuration < 0.0f)
        {
            return SetPreviewError(Transition.Id, "Crossfade duration must be finite and non-negative.");
        }

        const int32* ToIndex = StateIndices.Find(Target->Id);
        const int32* SourceIndex = StateIndices.Find(Source->Id);
        const bool bAnyState = Source->Type == EAnimBlueprintNodeType::AnyState;

        if (!ToIndex || (!bAnyState && !SourceIndex))
        {
            return SetPreviewError(Transition.Id, "The transition State is not registered.");
        }

        const int32 FromIndex = bAnyState ? INDEX_NONE : *SourceIndex;
        FAnimTransitionRule Rule;

        if (Transition.Rule == EAnimBlueprintTransitionRule::Finished)
        {
            if (bAnyState)
            {
                return SetPreviewError(Transition.Id, "Finished requires a specific source State.");
            }

            const int32 PoseIndex = Instance.GetStatePoseNode(MachineIndex, FromIndex);
            const FAnimNode_SequencePlayer* Player = Instance.GetNode<FAnimNode_SequencePlayer>(PoseIndex);

            if (!Player || Player->bLooping)
            {
                return SetPreviewError(Transition.Id, "Finished requires the source State to output a non-looping Sequence Player directly.");
            }

            Rule = Instance.MakeFinishedRule(PoseIndex);
        }
        else
        {
            const FAnimBlueprintVariable* MatchedVariable = nullptr;

            for (const FAnimBlueprintVariable& Variable : BlueprintData.Variables)
            {
                if (Variable.Name == Transition.Variable)
                {
                    MatchedVariable = &Variable;
                    break;
                }
            }

            if (!MatchedVariable)
            {
                return SetPreviewError(Transition.Id, "Select a variable for the transition condition.");
            }

            if (Transition.Rule == EAnimBlueprintTransitionRule::Bool)
            {
                if (MatchedVariable->Type != EAnimBlueprintVariableType::Bool)
                {
                    return SetPreviewError(Transition.Id, "This transition requires a Bool variable.");
                }

                Rule = Instance.MakeBoolRule(Transition.Variable, Transition.bExpected);
            }
            else if (Transition.Rule == EAnimBlueprintTransitionRule::Float)
            {
                if (MatchedVariable->Type != EAnimBlueprintVariableType::Float || !std::isfinite(Transition.Value))
                {
                    return SetPreviewError(Transition.Id, "This transition requires a Float variable and a finite comparison value.");
                }

                EAnimGraphCompare Compare;

                switch (Transition.Compare)
                {
                case EAnimBlueprintCompare::Less:
                    Compare = EAnimGraphCompare::Less;
                    break;

                case EAnimBlueprintCompare::LessEqual:
                    Compare = EAnimGraphCompare::LessEqual;
                    break;

                case EAnimBlueprintCompare::Greater:
                    Compare = EAnimGraphCompare::Greater;
                    break;

                case EAnimBlueprintCompare::GreaterEqual:
                    Compare = EAnimGraphCompare::GreaterEqual;
                    break;

                case EAnimBlueprintCompare::Equal:
                    Compare = EAnimGraphCompare::Equal;
                    break;

                case EAnimBlueprintCompare::NotEqual:
                    Compare = EAnimGraphCompare::NotEqual;
                    break;

                default:
                    return SetPreviewError(Transition.Id, "The comparison operator is invalid.");
                }

                Rule = Instance.MakeFloatRule(Transition.Variable, Compare, Transition.Value);
            }
            else
            {
                return SetPreviewError(Transition.Id, "The transition rule type is invalid.");
            }
        }

        if (Instance.AddTransition(MachineIndex, FromIndex, *ToIndex, std::move(Rule), Transition.CrossfadeDuration, Transition.Priority) == INDEX_NONE)
        {
            return SetPreviewError(Transition.Id, "Failed to register the transition.");
        }
    }

    return true;
}

bool FAnimBlueprintEditor::IsPreviewStateActive(int32 NodeId) const
{
    if (!PreviewInstance)
    {
        return false;
    }

    const TArray<int32>* PoseIndices = PreviewNodeIndices.Find(NodeId);

    if (!PoseIndices)
    {
        return false;
    }

    for (const FAnimBlueprintGraph& Graph : BlueprintData.Graphs)
    {
        if (Graph.Id != CurrentGraphId || Graph.Type != EAnimBlueprintGraphType::StateMachine)
        {
            continue;
        }

        const TArray<int32>* MachineIndices = PreviewNodeIndices.Find(Graph.OwnerNodeId);

        if (!MachineIndices)
        {
            return false;
        }

        for (int32 MachineIndex : *MachineIndices)
        {
            const FAnimNode_StateMachine* Machine = PreviewInstance->GetNode<FAnimNode_StateMachine>(MachineIndex);

            if (Machine)
            {
                const int32 CurrentPose = PreviewInstance->GetStatePoseNode(MachineIndex, Machine->GetCurrentStateIndex());

                if (PoseIndices->Find(CurrentPose) != INDEX_NONE)
                {
                    return true;
                }
            }
        }
    }

    return false;
}
