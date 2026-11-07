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
    if (auto* Sequence = Cast<UAnimSequence>(CurrentAsset))
    {
        SequencePlayer.Sequence = Sequence;
        SequencePlayer.CurrentTime = 0.0f;
        SequencePlayer.bLooping = bLooping;
        SequencePlayer.PlayRate = PlayRate;

        RootNode = &SequencePlayer;
    }
    else if (auto* BlendSpace = Cast<UBlendSpace1D>(CurrentAsset))
    {
        BlendSpacePlayer.BlendSpace = BlendSpace;
        BlendSpacePlayer.Phase = 0.0f;
        BlendSpacePlayer.PlayRate = PlayRate;

        RootNode = &BlendSpacePlayer;
    }
    else
    {
        RootNode = nullptr;
    }
}

void UAnimSingleNodeInstance::SetBlendSpaceAsset(UBlendSpace1D* InBlendSpace, float InPlayRate)
{
    SetAnimationAsset(InBlendSpace, true, InPlayRate);
}

void UAnimSingleNodeInstance::SetBlendSpaceInput(float Value)
{
	BlendSpacePlayer.X = Value;
}