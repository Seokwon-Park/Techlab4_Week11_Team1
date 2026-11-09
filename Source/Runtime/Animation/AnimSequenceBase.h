#pragma once

#include "AnimDataModel.h"
#include "AnimationAsset.h"
#include <memory>
#include "AnimSequenceBase.generated.h"

UCLASS()
class UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_BODY()

public:
	const UAnimDataModel* GetDataModel() const { return DataModel; }

protected:
	void InitializeDataModel(
		TArray<FBoneAnimationTrack> Tracks, FFrameRate FrameRate, float PlayLength)
	{
		if (!DataModel)
		{
			DataModel = NewObject<UAnimDataModel>(this);
		}

		DataModel->Initialize(std::move(Tracks), FrameRate, PlayLength);
	}

private:
	UAnimDataModel* DataModel = nullptr;
};
