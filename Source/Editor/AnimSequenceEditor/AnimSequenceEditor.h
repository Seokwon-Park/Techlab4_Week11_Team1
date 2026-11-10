#pragma once

#include "Editor/SkeletalPreviewEditor/SkeletalPreviewEditor.h"
#include "Animation/AnimSequence.h"

class FAnimSequenceEditor : public FSkeletalPreviewEditor
{
public:
    FAnimSequenceEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager,
        const FString& InAssetPath, UAnimSequence* InSequence);

    const char* GetEditorTypeName() const override { return "AnimSequence"; }

    // 재생 API — 패널은 이것만 부른다
    void  Play();
    void  Pause();
    void  SetPlaybackTime(float Time);   // 스크럽: Pause + 시간 설정 + bPoseDirty
    void  StepFrame(int32 Delta);        // ±1프레임 (Pause)
    
    bool  IsPlaying() const { return bPlaying; }
    float GetPlaybackTime() const { return CurrentTime; }
    float GetPlayLength() const;
    float GetFrameRate() const;
    int32 GetNumFrames() const;
    UAnimSequence* GetSequence() const { return Sequence; }

    // Notify 선택 (타임라인과 디테일 패널이 공유). 배열 Index는 정렬마다 바뀌므로 Guid로 가리킨다.
    const FGuid& GetSelectedNotify() const { return SelectedNotify; }
    void SelectNotify(const FGuid& Guid) { SelectedNotify = Guid; }
    void ClearNotifySelection() { SelectedNotify.Invalidate(); }
    // 선택한 Notify가 지워졌으면 nullptr. 편집 함수를 부른 뒤에는 포인터가 바뀔 수 있으니 다시 받는다.
    const FAnimNotifyEvent* GetSelectedNotifyEvent() const
    {
        return (Sequence && SelectedNotify.IsValid()) ? Sequence->FindNotify(SelectedNotify) : nullptr;
    }

    // 저장하지 않은 Notify 편집이 있는지 (저장 UI 단계에서 쓴다)
    void MarkNotifiesDirty() { bNotifiesDirty = true; }
    bool HasUnsavedNotifyChanges() const { return bNotifiesDirty; }

    bool  bLooping = true;
    float PlayRate = 1.0f;

protected:
    TUniquePtr<IEditorTab> CreateTab() override;
    bool InitializeResources() override;
    void ShutdownResources() override;
    void TickBeforeWorld(float DeltaTime) override;

private:
    bool IsCompatible(const USkeletalMesh& InMesh) const;

    UAnimSequence* Sequence = nullptr;
    FCompactPose PlaybackPose;
    TArray<int32> FiredNotifies;
    float CurrentTime = 0.0f;
    bool  bPlaying = true;            
    bool  bPoseDirty = true;

    FGuid SelectedNotify;
    bool  bNotifiesDirty = false;
};