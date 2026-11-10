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
    // 타임라인에서 선택한 Notify의 시간/길이/임계값/트랙/프로퍼티 편집
    void DrawNotifySection(class UAnimSequence& Sequence);
};
