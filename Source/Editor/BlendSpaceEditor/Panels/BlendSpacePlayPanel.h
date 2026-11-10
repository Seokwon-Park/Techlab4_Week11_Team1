#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"
#include "Editor/BlendSpaceEditor/BlendSpaceEditor.h"
#include <vector>

class FBlendSpacePlayPanel : public FAssetEditorPanel
{
public:
    explicit FBlendSpacePlayPanel(FBlendSpaceEditor& InEditor)
        : FAssetEditorPanel(InEditor, "Preview Controls")
        , Editor(InEditor)
    {
    }

protected:
    void DrawContent() override;

private:
    FBlendSpaceEditor& Editor;

    void DrawSamples();
    void RefreshAnimationList();

    bool bLoop = true;

    std::vector<FString> AnimationPaths;
    int SelectedAnimation = -1;
    bool bAnimationListInitialized = false;
    float SampleX = 0.0f;
    FString SampleMessage;
};
