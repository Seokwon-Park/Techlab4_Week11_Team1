#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"

class FAnimBlueprintEditor;

class FAnimBlueprintDetailsPanel : public FAssetEditorPanel
{
public:
    explicit FAnimBlueprintDetailsPanel(FAnimBlueprintEditor& InEditor);

protected:
    void DrawContent() override;

private:
    FAnimBlueprintEditor& Editor;
};