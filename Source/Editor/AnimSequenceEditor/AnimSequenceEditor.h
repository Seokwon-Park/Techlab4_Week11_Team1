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
};