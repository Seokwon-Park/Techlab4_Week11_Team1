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

private:
    FString SaveMessage;   // 마지막 저장 결과 (성공/실패)
};
