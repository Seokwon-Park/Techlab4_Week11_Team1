#include "EnginePCH.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditor.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditorTab.h"
#include "Animation/AnimSequence.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Render/SkeletalMesh.h"
#include "UObject/Casts.h"

#include <algorithm>
#include <cmath>

FAnimSequenceEditor::FAnimSequenceEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager,
    const FString& InAssetPath, UAnimSequence* InSequence)
    : FSkeletalPreviewEditor(InEditorEngine, InEditorManager, InAssetPath)
    , Sequence(InSequence)
{
}

bool FAnimSequenceEditor::InitializeResources()
{
    if (!Sequence || !Sequence->GetDataModel())
    {
        HTR_LOG(Warning, "AnimSequence has no animation data: {}", GetAssetPath());
        return false;
    }

    const FString& PreviewPath = Sequence->GetPreviewMeshPath();
    if (PreviewPath.empty())
    {
        HTR_LOG(Warning, "AnimSequence has no preview mesh. Re-import its FBX to record one: {}", GetAssetPath());
        return false;
    }

    // 경로 표기가 달라도 같은 파일을 찾도록 실제 파일 경로로 바꾼 뒤 로드한다 (EditorManager와 같은 방식)
    const FString FilePath = FAssetUtils::ResolveAssetFilePath(PreviewPath);
    FString Error;
    USkeletalMesh* PreviewMesh = Cast<USkeletalMesh>(UAssetManager::LoadFbxMesh(FilePath.empty() ? PreviewPath : FilePath, Error));
    if (!PreviewMesh)
    {
        HTR_LOG(Warning, "AnimSequence preview mesh failed to load: {} ({})", PreviewPath, Error);
        return false;
    }

    if (!IsCompatible(*PreviewMesh))
    {
        HTR_LOG(Warning, "AnimSequence tracks do not match the preview mesh bones: {}", GetAssetPath());
        return false;
    }

    Mesh = PreviewMesh;
    if (!InitializePreview())
    {
        return false;
    }

    CurrentTime = 0.0f;
    bPoseDirty = true;
    return true;
}

void FAnimSequenceEditor::ShutdownResources()
{
    bPlaying = false;
    FSkeletalPreviewEditor::ShutdownResources();
}

TUniquePtr<IEditorTab> FAnimSequenceEditor::CreateTab()
{
    return MakeUnique<FAnimSequenceEditorTab>(*this);
}

float FAnimSequenceEditor::GetPlayLength() const
{
    const UAnimDataModel* Model = Sequence ? Sequence->GetDataModel() : nullptr;
    return Model ? Model->GetPlayLength() : 0.0f;
}

float FAnimSequenceEditor::GetFrameRate() const
{
    const UAnimDataModel* Model = Sequence ? Sequence->GetDataModel() : nullptr;
    const float Rate = Model ? static_cast<float>(Model->GetFrameRate().AsDecimal()) : 0.0f;
    return Rate > 0.0f ? Rate : 30.0f;
}

int32 FAnimSequenceEditor::GetNumFrames() const
{
    const UAnimDataModel* Model = Sequence ? Sequence->GetDataModel() : nullptr;
    return Model ? Model->GetNumberOfFrames() : 0;
}

void FAnimSequenceEditor::Play()
{
    // 루프가 아니고 끝에 멈춰 있으면 처음부터 다시 재생한다
    if (!bLooping && CurrentTime >= GetPlayLength())
    {
        CurrentTime = 0.0f;
        bPoseDirty = true;
    }
    bPlaying = true;
}

void FAnimSequenceEditor::Pause()
{
    bPlaying = false;
}

void FAnimSequenceEditor::SetPlaybackTime(float Time)
{
    bPlaying = false;
    CurrentTime = std::clamp(Time, 0.0f, GetPlayLength());
    bPoseDirty = true;
}

void FAnimSequenceEditor::StepFrame(int32 Delta)
{
    const float Rate = GetFrameRate();
    const int32 Frame = static_cast<int32>(std::lround(CurrentTime * Rate)) + Delta;
    SetPlaybackTime(static_cast<float>(Frame) / Rate);
}

bool FAnimSequenceEditor::IsCompatible(const USkeletalMesh& InMesh) const
{
    // GetAnimationPose는 트랙을 본 이름으로 찾는다. 순서가 아니라 이름이 모두 있는지를 본다
    const TArray<FBoneAnimationTrack>& Tracks = Sequence->GetDataModel()->GetBoneAnimationTracks();
    if (Tracks.IsEmpty())
    {
        return false;
    }

    for (const FBoneAnimationTrack& Track : Tracks)
    {
        if (InMesh.FindBoneIndex(Track.Name) < 0)
        {
            return false;
        }
    }
    return true;
}

void FAnimSequenceEditor::TickBeforeWorld(float DeltaTime)
{
    if (!Sequence || !MeshComponent || !Mesh)
    {
        return;
    }

    const float Length = GetPlayLength();
    if (bPlaying && Length > 0.0f)
    {
        float Next = CurrentTime + DeltaTime * PlayRate;
        if (bLooping)
        {
            // fmod는 음수에 음수를 돌려주므로 Length를 더해 한 번 더 나눈다 (역재생 대응)
            Next = std::fmod(std::fmod(Next, Length) + Length, Length);
        }
        else if (Next >= Length || Next < 0.0f)
        {
            Next = std::clamp(Next, 0.0f, Length);
            bPlaying = false;
        }

        CurrentTime = Next;
        bPoseDirty = true;
    }

    if (bPoseDirty)
    {
        // 트랙이 없는 본은 GetAnimationPose 안에서 바인드 포즈로 남는다
        Sequence->GetAnimationPose(CurrentTime, PlaybackPose, Mesh->GetRefSkeleton());
        MeshComponent->SetBoneSpaceTransforms(PlaybackPose);
        bPoseDirty = false;
    }
}
