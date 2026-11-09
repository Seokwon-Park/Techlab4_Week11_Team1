#pragma once

#include "AnimSequenceEditorPanel.h"

class FAnimSequenceDetailsPanel : public FAnimSequenceEditorPanel
{
public:
    explicit FAnimSequenceDetailsPanel(FAnimSequenceEditor& InEditor)
        : FAnimSequenceEditorPanel(InEditor, "Details") {
    }

protected:
    void DrawContent() override;
};
