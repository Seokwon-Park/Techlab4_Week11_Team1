#pragma once

#include "BlendSpaceEditorPanel.h"

class FBlendSpaceViewportPanel : public FBlendSpaceEditorPanel
{
public:
    explicit FBlendSpaceViewportPanel(FBlendSpaceEditor& InEditor)
        : FBlendSpaceEditorPanel(InEditor, "Viewport") 
    {
    }

protected:
    void DrawContent() override;
};
