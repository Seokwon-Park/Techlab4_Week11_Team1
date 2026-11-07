#pragma once

#include "AnimNodeBase.h"
#include "AnimationRuntime.h"

struct FAnimNode_TwoWayBlend : public FAnimNode_Base
{
	FPoseLink A;
	FPoseLink B;

	float Alpha = 0.0f;

	virtual void Update(const FAnimationUpdateContext& Context) override
	{
		A.Update(Context);  
		B.Update(Context);
	}

	virtual void Evaluate(FPoseContext& Output) override
	{
		FPoseContext PoseA(Output);
		FPoseContext PoseB(Output);

		A.Evaluate(PoseA);
		B.Evaluate(PoseB);

		const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

		FAnimationRuntime::BlendTwoPosesTogether(
			PoseA.Pose, PoseB.Pose, PoseA.Curve, PoseB.Curve,
			1.0f - ClampedAlpha,
			Output.Pose, Output.Curve);
	}
};