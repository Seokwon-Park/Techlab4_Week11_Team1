#pragma once

#include "Animation/AnimSequenceBase.h"
#include "Animation/BonePose.h"
#include "AnimSequence.generated.h"

UCLASS()
class UAnimSequence : public UAnimSequenceBase
{
	GENERATED_BODY()

public:
	using Super::Serialize;
	void Serialize(FArchive& Ar) override;

	void Initialize(
		FString InClipName, TArray<FBoneAnimationTrack> Tracks,
		FFrameRate FrameRate, float PlayLength)
	{
		ClipName = std::move(InClipName);
		InitializeDataModel(std::move(Tracks), FrameRate, PlayLength);
	}

	void GetAnimationPose(float Time, FCompactPose& OutPose, const TArray<FBoneInfo>& RefSkeleton) const;

	const FString& GetClipName() const { return ClipName; }

private:
	FString ClipName;
};
