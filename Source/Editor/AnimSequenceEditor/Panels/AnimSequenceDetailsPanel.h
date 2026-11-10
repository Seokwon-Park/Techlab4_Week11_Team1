#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditor.h"

class FAnimSequenceDetailsPanel : public TAssetEditorPanel<FAnimSequenceEditor>
{
public:
    explicit FAnimSequenceDetailsPanel(FAnimSequenceEditor& InEditor)
        : TAssetEditorPanel<FAnimSequenceEditor>(InEditor, "Details", "AnimSequencePanel")
    {
    }

protected:
    void DrawContent() override;

private:
    FString SaveMessage;   // 마지막 저장 결과 (성공/실패)
    // 타임라인에서 선택한 Notify의 시간/길이/임계값/트랙/프로퍼티 편집
    void DrawNotifySection(class UAnimSequence& Sequence);
};
