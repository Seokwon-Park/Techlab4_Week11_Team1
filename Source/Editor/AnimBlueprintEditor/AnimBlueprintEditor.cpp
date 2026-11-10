#include "EnginePCH.h"
#include "AnimBlueprintEditor.h"
#include "AnimBlueprintEditorTab.h"

#include "Asset/AssetManager.h"
#include "Asset/AssetUtils.h"
#include "Render/SkeletalMesh.h"
#include "UObject/Casts.h"

FAnimBlueprintEditor::FAnimBlueprintEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager, const FString& InPreviewMeshPath)
    : FSkeletalPreviewEditor(InEditorEngine, InEditorManager, InPreviewMeshPath + ".animblueprint-preview")
{
    BlueprintData.PreviewMeshPath = InPreviewMeshPath;
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
    Graph.Nodes.Add(Player);

    FAnimBlueprintNode Output;
    Output.Id = BlueprintData.AllocateId();
    Output.Type = EAnimBlueprintNodeType::OutputPose;
    Output.Name = "Output Pose";
    Output.Position.X = 360.0f;
    Output.Position.Y = 100.0f;
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