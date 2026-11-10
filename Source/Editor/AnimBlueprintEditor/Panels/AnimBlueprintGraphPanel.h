#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"
#include "imgui_node_editor.h"

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
    FAnimBlueprintEditor& Editor;
    ax::NodeEditor::EditorContext* NodeEditorContext = nullptr;

    bool bInitialPositionsApplied = false;
};