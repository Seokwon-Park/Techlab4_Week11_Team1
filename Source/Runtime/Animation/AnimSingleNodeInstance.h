#pragma once

#include "AnimInstance.h"
#include "AnimNode_SequencePlayer.h"
#include "AnimNode_BlendSpacePlayer.h"
#include "AnimationAsset.h"
#include "AnimSingleNodeInstance.generated.h"

UCLASS()
class UAnimSingleNodeInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetAnimationAsset(UAnimationAsset* InAsset, bool bLoop = true, float InPlayRate = 1.0f);
	void SetBlendSpaceAsset(UBlendSpace1D* InBlendSpace, float InPlayRate = 1.0f);
	void SetBlendSpaceInput(float Value);

protected:
	void NativeInitializeAnimation() override;

private:
	UPROPERTY()
	UAnimationAsset* CurrentAsset = nullptr;

	FAnimNode_SequencePlayer SequencePlayer;
	FAnimNode_BlendSpacePlayer BlendSpacePlayer;

	UPROPERTY()
	bool bLooping = true;

	UPROPERTY()
	float PlayRate = 1.0f;
};