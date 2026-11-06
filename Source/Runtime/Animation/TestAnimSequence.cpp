#include "EnginePCH.h"
#include "TestAnimSequence.h"

void UTestAnimSequence::GetAnimationPose(
    float Time,
    FCompactPose& OutPose,
    const TArray<FBoneInfo>& RefSkeleton) const
{
    // 모든 본을 먼저 기준 자세로 채움
    OutPose.ResetToRefPose(RefSkeleton);

    constexpr int32 TestBoneIndex = 1;
    
    if (OutPose.GetNumBones() <= TestBoneIndex)
        return;


    Time = FMath::Clamp(Time, 0.0f, GetPlayLength());

    // 하드코딩한 키:
    const float Alpha = Time <= 1.0f ? Time : 2.0f - Time;

    OutPose[TestBoneIndex].Location += MaxOffset * Alpha;
}