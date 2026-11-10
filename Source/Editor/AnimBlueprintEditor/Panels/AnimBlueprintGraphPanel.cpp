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

    if (ImGui::Button("Apply Preview"))
    {
        Editor.RequestApplyPreview();
    }

    ImGui::SameLine();
    ImGui::Text("%s | Nodes: %d | Links: %d", Graph->Name.c_str(), Graph->Nodes.Num(), Graph->Links.Num());

    const FString& PreviewMessage = Editor.GetPreviewMessage();

    if (!PreviewMessage.empty())
    {
        ImGui::TextWrapped("%s", PreviewMessage.c_str());
    }

    ImGui::Separator();

    const ImVec2 AvailableSize = ImGui::GetContentRegionAvail();

    if (AvailableSize.x < 1.0f || AvailableSize.y < 1.0f)
    {
        return;
    }

    NodeEditor::EditorContext* PreviousContext = NodeEditor::GetCurrentEditor();
    NodeEditor::SetCurrentEditor(NodeEditorContext);

    NodeEditor::Begin("AnimBlueprintCanvas");

    DrawNodes(*Graph);
    DrawLinks(*Graph);

    HandleLinkCreation();
    HandleDeletion();
    DrawCreationMenu();

    NodeEditor::End();

    if (!bInitialViewApplied)
    {
        NodeEditor::NavigateToContent(0.0f);
        bInitialViewApplied = true;
    }

    UpdateNodePositions(*Graph);
    UpdateSelection();

    NodeEditor::SetCurrentEditor(PreviousContext);
}

void FAnimBlueprintGraphPanel::DrawNodes(FAnimBlueprintGraph& Graph)
{
    for (const FAnimBlueprintNode& Node : Graph.Nodes)
    {
        const NodeEditor::NodeId NodeId(Node.Id);

        if (PositionedNodeIds.find(Node.Id) == PositionedNodeIds.end())
        {
            NodeEditor::SetNodePosition(NodeId, ImVec2(Node.Position.X, Node.Position.Y));
            PositionedNodeIds.insert(Node.Id);
        }

        NodeEditor::BeginNode(NodeId);

        ImGui::TextUnformatted(Node.Name.c_str());
        ImGui::Separator();

        if (Node.InputPosePinId != INDEX_NONE)
        {
            NodeEditor::BeginPin(NodeEditor::PinId(Node.InputPosePinId), NodeEditor::PinKind::Input);
            ImGui::TextUnformatted(Node.Type == EAnimBlueprintNodeType::TwoWayBlend ? "o A" : "o Pose");
            NodeEditor::EndPin();
        }

        if (Node.SecondInputPosePinId != INDEX_NONE)
        {
            NodeEditor::BeginPin(NodeEditor::PinId(Node.SecondInputPosePinId), NodeEditor::PinKind::Input);
            ImGui::TextUnformatted("o B");
            NodeEditor::EndPin();
        }

        if (Node.OutputPosePinId != INDEX_NONE)
        {
            ImGui::Dummy(ImVec2(100.0f, 0.0f));
            ImGui::SameLine();

            NodeEditor::BeginPin(NodeEditor::PinId(Node.OutputPosePinId), NodeEditor::PinKind::Output);
            ImGui::TextUnformatted("Pose o");
            NodeEditor::EndPin();
        }

        ImGui::Dummy(ImVec2(160.0f, 12.0f));

        NodeEditor::EndNode();
    }
}

void FAnimBlueprintGraphPanel::DrawLinks(const FAnimBlueprintGraph& Graph)
{
    for (const FAnimBlueprintLink& Link : Graph.Links)
    {
        NodeEditor::Link(NodeEditor::LinkId(Link.Id), NodeEditor::PinId(Link.FromPinId), NodeEditor::PinId(Link.ToPinId));
    }
}

void FAnimBlueprintGraphPanel::HandleLinkCreation()
{
    if (NodeEditor::BeginCreate())
    {
        NodeEditor::PinId FirstPin;
        NodeEditor::PinId SecondPin;

        if (NodeEditor::QueryNewLink(&FirstPin, &SecondPin))
        {
            if (FirstPin && SecondPin)
            {
                const int32 FirstPinId = static_cast<int32>(FirstPin.Get());
                const int32 SecondPinId = static_cast<int32>(SecondPin.Get());

                if (Editor.CanConnectPosePins(FirstPinId, SecondPinId))
                {
                    if (NodeEditor::AcceptNewItem())
                    {
                        Editor.ConnectPosePins(FirstPinId, SecondPinId);
                    }
                }
                else
                {
                    NodeEditor::RejectNewItem();
                }
            }
        }
    }

    NodeEditor::EndCreate();
}

void FAnimBlueprintGraphPanel::HandleDeletion()
{
    if (NodeEditor::BeginDelete())
    {
        NodeEditor::LinkId LinkId;

        while (NodeEditor::QueryDeletedLink(&LinkId))
        {
            if (NodeEditor::AcceptDeletedItem())
            {
                Editor.RemoveLink(static_cast<int32>(LinkId.Get()));
            }
        }

        NodeEditor::NodeId NodeId;

        while (NodeEditor::QueryDeletedNode(&NodeId))
        {
            const int32 Id = static_cast<int32>(NodeId.Get());

            if (Editor.CanRemoveNode(Id))
            {
                if (NodeEditor::AcceptDeletedItem())
                {
                    Editor.RemoveNode(Id);
                    PositionedNodeIds.erase(Id);
                }
            }
            else
            {
                NodeEditor::RejectDeletedItem();
            }
        }
    }

    NodeEditor::EndDelete();
}

void FAnimBlueprintGraphPanel::DrawCreationMenu()
{
    const bool bOpenMenu = NodeEditor::ShowBackgroundContextMenu();

    // Restore screen-space mouse coordinates before converting to canvas space.
    NodeEditor::Suspend();

    if (bOpenMenu)
    {
        const ImVec2 Position = NodeEditor::ScreenToCanvas(ImGui::GetMousePos());
        CreationPosition.X = Position.x;
        CreationPosition.Y = Position.y;
    }

    if (bOpenMenu)
    {
        ImGui::OpenPopup("CreateAnimBlueprintNode");
    }

    if (ImGui::BeginPopup("CreateAnimBlueprintNode"))
    {
        if (ImGui::MenuItem("Sequence Player"))
        {
            Editor.AddSequencePlayer(CreationPosition);
        }

        if (ImGui::MenuItem("BlendSpace Player"))
        {
            Editor.AddPoseNode(EAnimBlueprintNodeType::BlendSpacePlayer, CreationPosition);
        }

        if (ImGui::MenuItem("Two Way Blend"))
        {
            Editor.AddPoseNode(EAnimBlueprintNodeType::TwoWayBlend, CreationPosition);
        }

        ImGui::EndPopup();
    }

    NodeEditor::Resume();
}

void FAnimBlueprintGraphPanel::UpdateNodePositions(FAnimBlueprintGraph& Graph)
{
    for (FAnimBlueprintNode& Node : Graph.Nodes)
    {
        if (PositionedNodeIds.find(Node.Id) == PositionedNodeIds.end())
        {
            continue;
        }

        const ImVec2 Position = NodeEditor::GetNodePosition(NodeEditor::NodeId(Node.Id));
        Node.Position.X = Position.x;
        Node.Position.Y = Position.y;
    }
}

void FAnimBlueprintGraphPanel::UpdateSelection()
{
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
}
