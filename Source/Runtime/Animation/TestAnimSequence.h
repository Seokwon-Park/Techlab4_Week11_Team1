#pragma once

#include "UObject/Object.h"
#include "Animation/BonePose.h"
#include "TestAnimSequence.generated.h"

UCLASS()
class UTestAnimSequence : public UObject
{
    GENERATED_BODY()

public:
    float GetPlayLength() const { return 2.0f; }

    void GetAnimationPose(
        float Time,
        FCompactPose& OutPose,
        const TArray<FBoneInfo>& RefSkeleton) const;

    FVector MaxOffset = FVector(1, 0, 0);
};