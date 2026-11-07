#include "EnginePCH.h"
#include "AnimSingleNodeInstance.h"

void UAnimSingleNodeInstance::SetAnimationAsset(
	UAnimSequence* InSequence, bool bLoop, float InPlayRate)
{
	SequencePlayer.Sequence = InSequence;
	SequencePlayer.CurrentTime = 0.0f;
	SequencePlayer.PlayRate = InPlayRate;
	SequencePlayer.bLoop = bLoop;

	AnimationAsset = InSequence;
	bLooping = bLoop;
	PlayRate = InPlayRate;

	bUseBlendSpace = false;
	RootNode = &SequencePlayer;
}

void UAnimSingleNodeInstance::NativeInitializeAnimation()
{
	if (bUseBlendSpace)
	{
		BlendSpacePlayer.Phase = 0.0f;
		BlendSpacePlayer.BlendSpace = BlendSpaceAsset;
		BlendSpacePlayer.PlayRate = PlayRate;
		RootNode = &BlendSpacePlayer;
	}
	else
	{
		SequencePlayer.CurrentTime = 0.0f;
		SequencePlayer.Sequence = AnimationAsset;
		SequencePlayer.bLoop = bLooping;
		SequencePlayer.PlayRate = PlayRate;
		RootNode = &SequencePlayer;
	}
}

void UAnimSingleNodeInstance::SetBlendSpaceAsset(UBlendSpace1D* InBlendSpace, float InPlayRate)
{
	bUseBlendSpace = true;
	BlendSpacePlayer.BlendSpace = InBlendSpace;
	BlendSpacePlayer.Phase = 0.0f;
	BlendSpacePlayer.PlayRate = InPlayRate;

	BlendSpaceAsset = InBlendSpace;
	PlayRate = InPlayRate;

	RootNode = &BlendSpacePlayer;
}

void UAnimSingleNodeInstance::SetBlendSpaceInput(float Value)
{
	BlendSpacePlayer.X = Value;
}