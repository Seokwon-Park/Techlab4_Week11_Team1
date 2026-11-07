#pragma once

#include "AnimNodeBase.h"
#include "BlendSpace1D.h"
#include "AnimationRuntime.h"

#include <cmath>

struct FAnimNode_BlendSpacePlayer : public FAnimNode_Base
{
	UBlendSpace1D* BlendSpace = nullptr;

	float X = 0.0f;
	float PlayRate = 1.0f;
	float Phase = 0.0f;

	void Update(const FAnimationUpdateContext& Context) override
	{
		if (!BlendSpace)
		{
			return;
		}

		FBlendSample SampleA;
		FBlendSample SampleB;
		float Alpha = 0.0f;

		if (!BlendSpace->GetSamples(X, SampleA, SampleB, Alpha)
			|| !SampleA.Animation || !SampleB.Animation
			|| !SampleA.Animation->GetDataModel() || !SampleB.Animation->GetDataModel())
		{
			return;
		}

		const float LengthA = SampleA.Animation->GetDataModel()->GetPlayLength();
		const float LengthB = SampleB.Animation->GetDataModel()->GetPlayLength();
		const float EffectiveLength = FMath::Lerp(LengthA, LengthB, Alpha);

		if (EffectiveLength <= 0.0f)
		{
			Phase = 0.0f;
			return;
		}

		Phase += Context.GetDeltaTime() * PlayRate / EffectiveLength;
		Phase = std::fmod(Phase, 1.0f);

		if (Phase < 0.0f)
		{
			Phase += 1.0f;
		}
	}

	void Evaluate(FPoseContext& Output) override
	{
        if (!BlendSpace)
        {
            Output.ResetToRefPose();
            return;
        }

        FBlendSample SampleA;
        FBlendSample SampleB;
        float Alpha = 0.0f;

        if (!BlendSpace->GetSamples(X, SampleA, SampleB, Alpha) 
            || !SampleA.Animation || !SampleB.Animation
            || !SampleA.Animation->GetDataModel() || !SampleB.Animation->GetDataModel())
        {
            Output.ResetToRefPose();
            return;
        }

        FPoseContext PoseA(Output);
        FPoseContext PoseB(Output);

        const float TimeA = Phase * SampleA.Animation->GetDataModel()->GetPlayLength();
        const float TimeB = Phase * SampleB.Animation->GetDataModel()->GetPlayLength();

        SampleA.Animation->GetAnimationPose(
            TimeA,
            PoseA.Pose,
            Output.GetRefSkeleton());

        SampleB.Animation->GetAnimationPose(
            TimeB,
            PoseB.Pose,
            Output.GetRefSkeleton());

        FAnimationRuntime::BlendTwoPosesTogether(
            PoseA.Pose, PoseB.Pose, PoseA.Curve, PoseB.Curve,
            1.0f - Alpha, Output.Pose, Output.Curve);
    }
};
