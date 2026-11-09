#pragma once

#include "BonePose.h"

struct FAnimationRuntime
{
	static inline void BlendTwoPosesTogether(
		const FCompactPose& PoseA,
		const FCompactPose& PoseB,
		const FBlendedCurve& CurveA,
		const FBlendedCurve& CurveB,
		float WeightA,
		FCompactPose& OutPose,
		FBlendedCurve& OutCurve
	)
	{
		if (PoseA.GetNumBones() != PoseB.GetNumBones())
		{
			return;
		}

		WeightA = FMath::Clamp(WeightA, 0.0f, 1.0f);
		const float Alpha = 1.0f - WeightA;

		OutPose.SetNumBones(PoseA.GetNumBones());

		for (int32 BoneIndex = 0; BoneIndex < PoseA.GetNumBones(); ++BoneIndex)
		{
			const FTransform& A = PoseA[BoneIndex];
			const FTransform& B = PoseB[BoneIndex];
			FTransform& Result = OutPose[BoneIndex];

			Result.Location = FMath::Lerp(A.Location, B.Location, Alpha);
			Result.Rotation = FMath::Slerp(A.Rotation, B.Rotation, Alpha);
			Result.Scale = FMath::Lerp(A.Scale, B.Scale, Alpha);
		}
		OutCurve = FBlendedCurve();
	}
};