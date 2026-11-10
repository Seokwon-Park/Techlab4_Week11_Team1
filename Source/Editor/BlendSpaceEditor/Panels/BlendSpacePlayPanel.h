#pragma once

#include "BlendSpaceEditorPanel.h"
#include <vector>

class FBlendSpacePlayPanel : public FBlendSpaceEditorPanel
{
public:
    explicit FBlendSpacePlayPanel(FBlendSpaceEditor& InEditor)
        : FBlendSpaceEditorPanel(InEditor, "Preview Controls")
    {
    }

protected:
    void DrawContent() override;

private:
    void DrawSamples();
    void RefreshAnimationList();

private:
    bool bLoop = true;

    std::vector<FString> AnimationPaths;
    int SelectedAnimation = -1;
    bool bAnimationListInitialized = false;
    float SampleX = 0.0f;
    FString SampleMessage;
};
