#pragma once

#include "AnimDataModel.h"
#include "AnimationAsset.h"
#include "Animation/AnimTypes.h"
#include "AnimNotifyQueue.h"
#include <memory>
#include "AnimSequenceBase.generated.h"

UCLASS()
class UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_BODY()

public:
	virtual ~UAnimSequenceBase() override;

	const UAnimDataModel* GetDataModel() const { return DataModel; }
	bool IsNotifyAvailable() const;
	void GetAnimNotifies(float StartTime, float DeltaTime, FAnimNotifyContext& NotifyContext) const;
	void GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition, FAnimNotifyContext& NotifyContext) const;

	// ---------------- Notify 편집 (타임라인 UI용) ----------------
	// - Notify는 항상 TriggerTime 오름차순으로 정렬된 상태를 유지한다.
	// - 배열 Index는 추가/삭제/정렬마다 바뀌므로 UI는 Guid로만 Notify를 가리킨다.
	// - 클래스 기반 Notify 객체(UAnimNotify)는 이 시퀀스가 소유하며, 삭제 시 함께 delete된다.
	// - 주의: 애니메이션 틱 도중(Notify 핸들러 안 등)에 호출하면 안 된다.
	//   큐에 담긴 FAnimNotifyEventReference가 배열 원소를 포인터로 가리키기 때문이다.

	const TArray<FAnimNotifyEvent>& GetNotifies() const { return AnimNotifies; }

	// 없으면 nullptr. 반환된 Event의 Notify 객체 파라미터는 수정해도 되지만, 시간은 SetNotifyTriggerTime으로 바꾼다.
	const FAnimNotifyEvent* FindNotify(const FGuid& NotifyGuid) const;

	// 이름 기반 Notify 추가. 새 Notify의 Guid를 반환한다.
	FGuid AddNotify(float Time, const FName& NotifyName);

	// 클래스 기반 Notify 추가. 이 시퀀스를 Outer로 객체를 만들어 반환하므로 호출한 쪽이 파라미터를 채운다.
	template <typename T>
	T* AddNotify(float Time, FGuid* OutGuid = nullptr)
	{
		static_assert(std::is_base_of_v<UAnimNotify, T>, "T must derive from UAnimNotify");
		T* NotifyObject = NewObject<T>(this);

		FAnimNotifyEvent NewEvent;
		NewEvent.Notify = NotifyObject;
		const FGuid NewGuid = AddNotifyEvent(Time, NewEvent);
		if (OutGuid)
		{
			*OutGuid = NewGuid;
		}
		return NotifyObject;
	}

	// 시간 변경 (타임라인에서 드래그). 범위를 제한하고 다시 정렬한다.
	bool SetNotifyTriggerTime(const FGuid& NotifyGuid, float Time);
	// 이 가중치 이상으로 블렌드될 때만 발동한다 (BlendSpace에서 우세한 샘플만 울리게 할 때)
	bool SetNotifyTriggerWeightThreshold(const FGuid& NotifyGuid, float Threshold);

	bool RemoveNotify(const FGuid& NotifyGuid);
	int32 RemoveNotifies(const TArray<FGuid>& NotifyGuids);   // 다중 선택 삭제, 삭제한 개수 반환
	int32 RemoveNotifiesByName(const FName& NotifyName);      // 같은 이름 전부 삭제 (UE RemoveNotifies(Names))
	void RemoveAllNotifies();                                 // UE RemoveNotifies()

	void SortNotifies();

private:
	FGuid AddNotifyEvent(float Time, FAnimNotifyEvent& NewEvent);
	float ClampNotifyTime(float Time) const;
	int32 FindNotifyIndex(const FGuid& NotifyGuid) const;
	void RemoveNotifyAt(int32 Index);   // 소유한 Notify 객체를 delete하고 순서를 유지한 채 제거

protected:
	void InitializeDataModel(
		TArray<FBoneAnimationTrack> Tracks, FFrameRate FrameRate, float PlayLength)
	{
		if (!DataModel)
		{
			// unique_ptr은 raw 포인터를 '='로 받을 수 없다. 소유권을 넘길 때는 reset을 쓴다.
			DataModel = NewObject<UAnimDataModel>(this);
		}

		DataModel->Initialize(std::move(Tracks), FrameRate, PlayLength);
	}

private:
	UAnimDataModel* DataModel = nullptr;
	TArray<FAnimNotifyEvent> AnimNotifies;
};
