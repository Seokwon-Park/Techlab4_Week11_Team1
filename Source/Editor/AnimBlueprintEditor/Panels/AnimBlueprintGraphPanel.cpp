#include "EnginePCH.h"
#include "AnimBlueprintGraphPanel.h"
#include "Editor/AnimBlueprintEditor/AnimBlueprintEditor.h"

namespace NodeEditor = ax::NodeEditor;

FAnimBlueprintGraphPanel::FAnimBlueprintGraphPanel(FAnimBlueprintEditor& InEditor)
    : FAssetEditorPanel(InEditor, "AnimGraph")
    , Editor(InEditor)
{
}

FAnimBlueprintGraphPanel::~FAnimBlueprintGraphPanel()
{
    if (NodeEditorContext)
    {
        NodeEditor::DestroyEditor(NodeEditorContext);
    }
}

bool FAnimBlueprintGraphPanel::Init()
{
    if (NodeEditorContext)
    {
        return true;
    }

    NodeEditor::Config Config;
    Config.SettingsFile = nullptr;

    NodeEditorContext = NodeEditor::CreateEditor(&Config);
    return NodeEditorContext != nullptr;
}

void FAnimBlueprintGraphPanel::DrawContent()
{
    FAnimBlueprintGraph* Graph = Editor.GetCurrentGraph();

    if (!Graph || !NodeEditorContext)
    {
        ImGui::TextUnformatted("Graph is not available.");
        return;
    }

    ImGui::TextUnformatted(Graph->Name.c_str());
    ImGui::Separator();

    NodeEditor::EditorContext* PreviousContext = NodeEditor::GetCurrentEditor();
    NodeEditor::SetCurrentEditor(NodeEditorContext);
    NodeEditor::Begin("AnimBlueprintCanvas");

    for (const FAnimBlueprintNode& Node : Graph->Nodes)
    {
        const NodeEditor::NodeId NodeId(Node.Id);

        if (!bInitialPositionsApplied)
        {
            NodeEditor::SetNodePosition(NodeId, ImVec2(Node.Position.X, Node.Position.Y));
        }

        NodeEditor::BeginNode(NodeId);

        ImGui::TextUnformatted(Node.Name.c_str());
        ImGui::Dummy(ImVec2(140.0f, 30.0f));

        NodeEditor::EndNode();
    }

    NodeEditor::End();
    if (!bInitialPositionsApplied)
    {
        NodeEditor::NavigateToContent(0.0f);
        bInitialPositionsApplied = true;
    }

    for (FAnimBlueprintNode& Node : Graph->Nodes)
    {
        const ImVec2 Position = NodeEditor::GetNodePosition(NodeEditor::NodeId(Node.Id));
        Node.Position.X = Position.x;
        Node.Position.Y = Position.y;
    }

    NodeEditor::NodeId SelectedNodes[1];
    const int32 SelectedCount = NodeEditor::GetSelectedNodes(SelectedNodes, 1);

    if (SelectedCount > 0)
    {
        Editor.SelectNode(static_cast<int32>(SelectedNodes[0].Get()));
    }
    else
    {
        Editor.SelectNode(INDEX_NONE);
    }

    NodeEditor::SetCurrentEditor(PreviousContext);
}