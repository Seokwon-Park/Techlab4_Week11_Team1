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

    FAnimSequenceEditor& AnimEditor;
};
