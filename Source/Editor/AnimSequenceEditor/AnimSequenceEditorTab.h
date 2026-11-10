#pragma once

#include "Editor/AssetEditor/DockedAssetEditorTab.h"

class FAnimSequenceEditor;

class FAnimSequenceEditorTab : public FDockedAssetEditorTab
{
public:
    explicit FAnimSequenceEditorTab(FAnimSequenceEditor& InEditor);

private:
    void BeforeRender() override;
    void BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size) override;
    void BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size);
    // 저장하지 않고 닫으려 할 때: Save and Close / Discard and Close / Cancel
    void DrawUnsavedChangesPrompt();

    FAnimSequenceEditor& AnimEditor;
};
