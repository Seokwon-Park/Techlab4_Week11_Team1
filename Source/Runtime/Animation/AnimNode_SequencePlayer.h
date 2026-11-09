#pragma once

#include "AnimNodeBase.h"
#include "AnimSequence.h"

#include <cmath>
#include "AnimInstance.h"
#include "AnimNotifyQueue.h"

struct FAnimNode_SequencePlayer : public FAnimNode_Base
{
	UAnimSequence* Sequence = nullptr;

	float CurrentTime = 0.0f;
	float PrevTime = 0.0f;
	float PlayRate = 1.0f;

	bool bLooping = false;

	void Update(const FAnimationUpdateContext& Context) override
	{
		if (!Sequence)
		{
			return;
		}

		const UAnimDataModel* Model = Sequence->GetDataModel();
		const float Length = Model ? Model->GetPlayLength() : 0.0f;
		if (Length <= 0.0f)
		{
			CurrentTime = PrevTime = 0.0f;
			return;
		}

		PrevTime = CurrentTime;
		CurrentTime += Context.GetDeltaTime() * PlayRate;

		if (bLooping)
		{
			CurrentTime = std::fmod(CurrentTime, Length);
			if (CurrentTime < 0.0f)
			{
				CurrentTime += Length;
			}
		}
		else
		{
			CurrentTime = FMath::Clamp(CurrentTime, 0.0f, Length);
		}


		FAnimNotifyContext NotifyContext(bLooping);
		Sequence->GetAnimNotifies(PrevTime, Context.GetDeltaTime() * PlayRate, NotifyContext);
		UAnimInstance* AnimInstance = Context.GetAnimInstance<UAnimInstance>();
		AnimInstance->AddNotifies(NotifyContext.ActiveNotifies, Context.GetFinalBlendWeight());
	}

	void Evaluate(FPoseContext& Output) override
	{
		if (!Sequence)
		{
			Output.ResetToRefPose();
			return;
		}

		Sequence->GetAnimationPose(CurrentTime, Output.Pose, Output.GetRefSkeleton());
	}
};