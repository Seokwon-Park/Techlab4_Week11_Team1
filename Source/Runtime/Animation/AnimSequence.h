#pragma once

#include "Animation/AnimSequenceBase.h"
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

	const FString& GetClipName() const { return ClipName; }

private:
	FString ClipName;
};
