#pragma once

#include "AnimDataModel.h"
#include "AnimationAsset.h"
#include "Animation/AnimTypes.h"
#include "AnimNotifyQueue.h"
#include <memory>
#include "AnimSequenceBase.generated.h"

UCLASS()
class UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_BODY()

public:
	const UAnimDataModel* GetDataModel() const { return DataModel.get(); }
	bool IsNotifyAvailable() const;
	void GetAnimNotifies(float StartTime, float DeltaTime, FAnimNotifyContext& NotifyContext) const;
	void GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition, FAnimNotifyContext& NotifyContext) const;
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
	TArray<FAnimNotifyEvent> AnimNotifies;
};
