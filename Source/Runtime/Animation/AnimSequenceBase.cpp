#include "EnginePCH.h"
#include "Animation/AnimationRuntime.h"
#include "AnimSequenceBase.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include <algorithm>

UAnimSequenceBase::~UAnimSequenceBase()
{
    // 이 시퀀스를 Outer로 만든 Notify 객체들을 함께 해제한다
    for (FAnimNotifyEvent& Event : AnimNotifies)
    {
        delete Event.Notify;
        delete Event.NotifyStateClass;
    }
}

const FAnimNotifyEvent* UAnimSequenceBase::FindNotify(const FGuid& NotifyGuid) const
{
    const int32 Index = FindNotifyIndex(NotifyGuid);
    return Index != -1 ? &AnimNotifies[Index] : nullptr;
}

FGuid UAnimSequenceBase::AddNotify(float Time, const FName& NotifyName, float Duration)
{
    FAnimNotifyEvent NewEvent;
    NewEvent.NotifyName = NotifyName;
    return AddNotifyEvent(Time, Duration, NewEvent);
}

UObject* UAnimSequenceBase::AddNotifyOfClass(UClass* NotifyClass, float Time, float Duration, FGuid* OutGuid)
{
    if (!NotifyClass)
    {
        return nullptr;
    }

    const bool bIsState = NotifyClass->IsChildOf(UAnimNotifyState::StaticClass());
    if (!bIsState && !NotifyClass->IsChildOf(UAnimNotify::StaticClass()))
    {
        return nullptr;
    }

    UObject* NotifyObject = NewObject<UObject>(this, NotifyClass);
    if (!NotifyObject)
    {
        return nullptr;
    }

    FAnimNotifyEvent NewEvent;
    if (bIsState)
    {
        NewEvent.NotifyStateClass = static_cast<UAnimNotifyState*>(NotifyObject);
    }
    else
    {
        NewEvent.Notify = static_cast<UAnimNotify*>(NotifyObject);
        Duration = 0.f;   // 단발 Notify에 Duration을 주면 구간 동안 매 프레임 불리므로 막는다
    }

    const FGuid NewGuid = AddNotifyEvent(Time, Duration, NewEvent);
    if (OutGuid)
    {
        *OutGuid = NewGuid;
    }
    return NotifyObject;
}

FGuid UAnimSequenceBase::AddNotifyEvent(float Time, float Duration, FAnimNotifyEvent& NewEvent)
{
    NewEvent.TriggerTime = ClampNotifyTime(Time);
    NewEvent.Duration = ClampNotifyDuration(NewEvent.TriggerTime, Duration);
    NewEvent.Guid = FGuid::NewGuid();

    const FGuid NewGuid = NewEvent.Guid;
    AnimNotifies.Add(NewEvent);
    SortNotifies();
    return NewGuid;
}

bool UAnimSequenceBase::SetNotifyTriggerTime(const FGuid& NotifyGuid, float Time)
{
    const int32 Index = FindNotifyIndex(NotifyGuid);
    if (Index == -1)
    {
        return false;
    }

    FAnimNotifyEvent& Event = AnimNotifies[Index];
    Event.TriggerTime = ClampNotifyTime(Time);
    // 뒤로 밀면 끝이 시퀀스 밖으로 나갈 수 있으므로 Duration도 다시 자른다
    Event.Duration = ClampNotifyDuration(Event.TriggerTime, Event.Duration);
    SortNotifies();
    return true;
}

bool UAnimSequenceBase::SetNotifyTriggerWeightThreshold(const FGuid& NotifyGuid, float Threshold)
{
    const int32 Index = FindNotifyIndex(NotifyGuid);
    if (Index == -1)
    {
        return false;
    }

    AnimNotifies[Index].TriggerWeightThreshold = FMath::Clamp(Threshold, 0.f, 1.f);
    return true;
}

bool UAnimSequenceBase::RemoveNotify(const FGuid& NotifyGuid)
{
    const int32 Index = FindNotifyIndex(NotifyGuid);
    if (Index == -1)
    {
        return false;
    }

    RemoveNotifyAt(Index);
    return true;
}

int32 UAnimSequenceBase::RemoveNotifies(const TArray<FGuid>& NotifyGuids)
{
    int32 NumRemoved = 0;
    // 뒤에서부터 지워야 아직 검사하지 않은 원소의 Index가 밀리지 않는다 (UE RemoveNotifies와 같은 방식)
    for (int32 Index = AnimNotifies.Num() - 1; Index >= 0; --Index)
    {
        if (NotifyGuids.Find(AnimNotifies[Index].Guid) != -1)
        {
            RemoveNotifyAt(Index);
            ++NumRemoved;
        }
    }
    return NumRemoved;
}

int32 UAnimSequenceBase::RemoveNotifiesByName(const FName& NotifyName)
{
    int32 NumRemoved = 0;
    for (int32 Index = AnimNotifies.Num() - 1; Index >= 0; --Index)
    {
        if (AnimNotifies[Index].NotifyName == NotifyName)
        {
            RemoveNotifyAt(Index);
            ++NumRemoved;
        }
    }
    return NumRemoved;
}

void UAnimSequenceBase::RemoveAllNotifies()
{
    for (int32 Index = AnimNotifies.Num() - 1; Index >= 0; --Index)
    {
        RemoveNotifyAt(Index);
    }
}

void UAnimSequenceBase::SortNotifies()
{
    // stable_sort: 같은 시간의 Notify는 추가한 순서를 유지한다
    std::stable_sort(AnimNotifies.begin(), AnimNotifies.end(),
        [](const FAnimNotifyEvent& A, const FAnimNotifyEvent& B) { return A.TriggerTime < B.TriggerTime; });
}

float UAnimSequenceBase::ClampNotifyTime(float Time) const
{
    const float PlayLength = DataModel ? DataModel->GetPlayLength() : 0.f;
    return FMath::Clamp(Time, 0.f, FMath::Max(PlayLength, 0.f));
}

float UAnimSequenceBase::ClampNotifyDuration(float TriggerTime, float Duration) const
{
    const float PlayLength = DataModel ? DataModel->GetPlayLength() : 0.f;
    return FMath::Clamp(Duration, 0.f, FMath::Max(PlayLength - TriggerTime, 0.f));
}

int32 UAnimSequenceBase::FindNotifyIndex(const FGuid& NotifyGuid) const
{
    if (!NotifyGuid.IsValid())
    {
        return -1;
    }

    for (int32 Index = 0; Index < AnimNotifies.Num(); ++Index)
    {
        if (AnimNotifies[Index].Guid == NotifyGuid)
        {
            return Index;
        }
    }
    return -1;
}

void UAnimSequenceBase::RemoveNotifyAt(int32 Index)
{
    FAnimNotifyEvent& Event = AnimNotifies[Index];
    delete Event.Notify;
    delete Event.NotifyStateClass;

    // RemoveAtSwap은 정렬을 깨므로 순서를 유지하는 RemoveAt을 쓴다
    AnimNotifies.RemoveAt(static_cast<uint32>(Index));
}

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
