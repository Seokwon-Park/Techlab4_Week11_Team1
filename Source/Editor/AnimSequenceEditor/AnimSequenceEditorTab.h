#pragma once

#include "Editor/AssetEditor/AssetEditorTab.h"
#include "Panels/AnimSequenceEditorPanel.h"

class FAnimSequenceEditor;

class FAnimSequenceEditorTab : public FAssetEditorTab
{
public:
    explicit FAnimSequenceEditorTab(FAnimSequenceEditor& InEditor);

    void DrawMenuBar() override;
    void OnRender(bool bActive) override;

private:
    void BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size);
    // 저장하지 않고 닫으려 할 때: Save and Close / Discard and Close / Cancel
    void DrawUnsavedChangesPrompt();

    FAnimSequenceEditor& AnimEditor;
    TArray<TUniquePtr<FAnimSequenceEditorPanel>> Panels;
    ImGuiID DockSpaceId = 0;
    ImGuiID PanelClassId = 0;
    bool bLayoutInitialized = false;
};
