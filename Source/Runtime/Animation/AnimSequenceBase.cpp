#include "EnginePCH.h"
#include "Animation/AnimationRuntime.h"
#include "AnimSequenceBase.h"

bool UAnimSequenceBase::IsNotifyAvailable() const
{
    return (AnimNotifies.Num() != 0) && (DataModel->GetPlayLength() > 0.f);
}

void UAnimSequenceBase::GetAnimNotifies(float StartTime, float DeltaTime, FAnimNotifyContext& Context) const
{
    // Early out if we have no notifies
    if (!IsNotifyAvailable())
    {
        return;
    }

    bool const bPlayingBackwards = (DeltaTime < 0.f);
    float PreviousPosition = StartTime;
    float CurrentPosition = StartTime;
    float DesiredDeltaMove = DeltaTime;
    const float PlayLength = DataModel->GetPlayLength();

    uint32_t MaxLoopCount = 2;

    //최대 1000번까지 루프 가능
    if (PlayLength > 0.0f && std::abs(DeltaTime) > PlayLength)
    {
        MaxLoopCount = FMath::Clamp(int32(DesiredDeltaMove / PlayLength), 2, 1000);
    }

    for (uint32_t i = 0; i < MaxLoopCount; i++)
    {
        // 루프를 끈 채로 시간 진행 → 끝에 닿으면 거기서 멈춤
        const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, CurrentPosition, PlayLength);

        GetAnimNotifiesFromDeltaPositions(PreviousPosition, CurrentPosition, Context);

        // 끝에 닿았고 루프 재생이면 → 남은 이동량으로 0초(역재생이면 끝)부터 다시
        if (AdvanceType == ETypeAdvanceAnim::ETAA_Finished && Context.bIsLooping)
        {
            const float ActualDeltaMove = (CurrentPosition - PreviousPosition);
            DesiredDeltaMove -= ActualDeltaMove;
            PreviousPosition = bPlayingBackwards ? PlayLength : 0.f;
            CurrentPosition = PreviousPosition;
        }
        else break;
    }
}

void UAnimSequenceBase::GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition, FAnimNotifyContext& NotifyContext) const
{
	// Early out if we have no notifies
	if (AnimNotifies.Num() == 0)
	{
		return;
	}

	bool const bPlayingBackwards = (CurrentPosition < PreviousPosition);

	// If playing backwards, flip Min and Max.
	if (bPlayingBackwards)
	{
		for (int32 NotifyIndex = 0; NotifyIndex < AnimNotifies.Num(); NotifyIndex++)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = AnimNotifies[NotifyIndex];
			const float NotifyStartTime = AnimNotifyEvent.TriggerTime;
			const float NotifyEndTime = AnimNotifyEvent.GetEndTriggerTime();

			if ((NotifyStartTime < PreviousPosition) && (NotifyEndTime >= CurrentPosition))
			{			
				NotifyContext.ActiveNotifies.Emplace(&AnimNotifyEvent, this);
				const bool bHasFinished = CurrentPosition <= FMath::Max(NotifyStartTime, 0.f);
				if (bHasFinished)
				{
					//TODO : Context를 붙이는 방법을 사용하여 추가 정보를 붙인다, 추후에 알아보기
					//NotifyContext.ActiveNotifies.Top().AddContextData<UE::Anim::FAnimNotifyEndDataContext>(true);
					NotifyContext.bHasFinished = true;
				}
			}
		}
	}
	else
	{
		for (int32 NotifyIndex = 0; NotifyIndex < AnimNotifies.Num(); NotifyIndex++)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = AnimNotifies[NotifyIndex];
			const float NotifyStartTime = AnimNotifyEvent.TriggerTime;
			const float NotifyEndTime = AnimNotifyEvent.GetEndTriggerTime();

			// Note that if you arrive with zero delta time (CurrentPosition == PreviousPosition), only Notify States will be extracted
			if ((NotifyStartTime <= CurrentPosition) && (NotifyEndTime > PreviousPosition))
			{
				NotifyContext.ActiveNotifies.Emplace(&AnimNotifyEvent, this);

				const bool bHasFinished = CurrentPosition >= NotifyEndTime;
				if (bHasFinished)
				{
					//NotifyContext.ActiveNotifies.Top().AddContextData<UE::Anim::FAnimNotifyEndDataContext>(true);
					NotifyContext.bHasFinished = true;
				}
			}
		}
	}
}