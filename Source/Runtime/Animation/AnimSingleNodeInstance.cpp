#include "EnginePCH.h"
#include "AnimSingleNodeInstance.h"
#include "UObject/Casts.h"

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimationAsset* InAsset, bool bLoop, float InPlayRate)
{
	CurrentAsset = InAsset;
	bLooping = bLoop;
	PlayRate = InPlayRate;

	InitializeAnimation();
}

void UAnimSingleNodeInstance::NativeInitializeAnimation()
{
	SingleNode.SetAnimationAsset(CurrentAsset, bLooping, PlayRate);
	RootNode = &SingleNode;
}

void UAnimSingleNodeInstance::SetBlendSpaceAsset(UBlendSpace1D* InBlendSpace, float InPlayRate)
{
    SetAnimationAsset(InBlendSpace, true, InPlayRate);
}

void UAnimSingleNodeInstance::SetBlendSpaceInput(float Value)
{
	SingleNode.SetBlendSpaceInput(Value);
}