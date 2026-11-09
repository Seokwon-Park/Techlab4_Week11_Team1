#pragma once

#include "Containers/Array.h"
#include "Editor/AssetEditor/AssetEditorTab.h"
#include "Panels/BlendSpaceEditorPanel.h"

class FBlendSpaceEditor;

class FBlendSpaceEditorTab : public FAssetEditorTab
{
public:
    explicit FBlendSpaceEditorTab(FBlendSpaceEditor& InEditor);

    void DrawMenuBar() override;
    void OnRender(bool bActive) override;

private:
    void BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size);

    FBlendSpaceEditor& BlendSpaceEditor;
    TArray<TUniquePtr<FBlendSpaceEditorPanel>> Panels;
    ImGuiID DockSpaceId = 0;
    ImGuiID PanelClassId = 0;
    bool bLayoutInitialized = false;
};
