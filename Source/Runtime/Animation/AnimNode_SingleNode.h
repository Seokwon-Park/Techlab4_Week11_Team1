#pragma once

#include "AnimationAsset.h"
#include "AnimNode_SequencePlayer.h"
#include "AnimNode_BlendSpacePlayer.h"
#include "UObject/Casts.h"

struct FAnimNode_SingleNode : public FAnimNode_Base
{

public:
    void SetAnimationAsset(UAnimationAsset* Asset, bool bLoop, float PlayRate)
    {
        // 이전 에셋 연결 해제
        ActivePlayer = nullptr;
        SequencePlayer.Sequence = nullptr;
        BlendSpacePlayer.BlendSpace = nullptr;

        if (auto* Sequence = Cast<UAnimSequence>(Asset))
        {
            SequencePlayer.Sequence = Sequence;
            SequencePlayer.CurrentTime = 0.0f;
            SequencePlayer.bLooping = bLoop;
            SequencePlayer.PlayRate = PlayRate;

            ActivePlayer = &SequencePlayer;
        }
        else if (auto* BlendSpace = Cast<UBlendSpace1D>(Asset))
        {
            BlendSpacePlayer.BlendSpace = BlendSpace;
            BlendSpacePlayer.Phase = 0.0f;
            BlendSpacePlayer.PlayRate = PlayRate;

            ActivePlayer = &BlendSpacePlayer;
        }
    }

    void SetBlendSpaceInput(float Value)
    {
        BlendSpacePlayer.X = Value;
    }

    void Initialize(const FAnimationInitializeContext& Context) override
    {
        if (ActivePlayer)
        {
            ActivePlayer->Initialize(Context);
        }
    }

    void Update(const FAnimationUpdateContext& Context) override
    {
        if (ActivePlayer)
        {
            ActivePlayer->Update(Context);
        }
    }

    void Evaluate(FPoseContext& Output) override
    {
        if (ActivePlayer)
        {
            ActivePlayer->Evaluate(Output);
        }
        else
        {
            Output.ResetToRefPose();
        }
    }

private:
    FAnimNode_SequencePlayer SequencePlayer;
    FAnimNode_BlendSpacePlayer BlendSpacePlayer;

    FAnimNode_Base* ActivePlayer = nullptr;
};