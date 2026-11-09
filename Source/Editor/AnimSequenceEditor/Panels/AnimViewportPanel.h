#pragma once

#include "AnimSequenceEditorPanel.h"

class FAnimViewportPanel : public FAnimSequenceEditorPanel
{
public:
    explicit FAnimViewportPanel(FAnimSequenceEditor& InEditor)
        : FAnimSequenceEditorPanel(InEditor, "Viewport") {
    }

protected:
    void DrawContent() override;
};
