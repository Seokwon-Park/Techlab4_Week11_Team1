#pragma once

#include "BlendSpaceEditorPanel.h"

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
    bool bLoop = true;
};