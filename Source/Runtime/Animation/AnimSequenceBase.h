#pragma once

#include "Animation/AnimDataModel.h"
#include <memory>
#include "AnimSequenceBase.generated.h"

UCLASS()
class UAnimSequenceBase : public UObject
{
	GENERATED_BODY()

public:
	const UAnimDataModel* GetDataModel() const { return DataModel.get(); }

protected:
	void InitializeDataModel(
		TArray<FBoneAnimationTrack> Tracks, FFrameRate FrameRate, float PlayLength)
	{
		if (!DataModel)
		{
			DataModel.reset(NewObject<UAnimDataModel>(this));
		}

		DataModel->Initialize(std::move(Tracks), FrameRate, PlayLength);
	}

private:
	std::unique_ptr<UAnimDataModel> DataModel;
};
