#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"
#include "Animation/AnimBlueprintData.h"
#include "imgui_node_editor.h"

#include <unordered_set>

class FAnimBlueprintEditor;

class FAnimBlueprintGraphPanel : public FAssetEditorPanel
{
public:
    explicit FAnimBlueprintGraphPanel(FAnimBlueprintEditor& InEditor);
    ~FAnimBlueprintGraphPanel() override;

    bool Init() override;

protected:
    void DrawContent() override;

private:
    void DrawNodes(FAnimBlueprintGraph& Graph);
    void DrawLinks(const FAnimBlueprintGraph& Graph);

    void HandleLinkCreation();
    void HandleDeletion();
    void DrawCreationMenu();

    void UpdateNodePositions(FAnimBlueprintGraph& Graph);
    void UpdateSelection();

    FAnimBlueprintEditor& Editor;
    ax::NodeEditor::EditorContext* NodeEditorContext = nullptr;

    std::unordered_set<int32> PositionedNodeIds;
    FAnimBlueprintPosition CreationPosition;

    bool bInitialViewApplied = false;
};