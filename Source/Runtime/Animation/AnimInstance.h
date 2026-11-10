#pragma once

#include "UObject/Object.h"
#include "Animation/AnimNotifyQueue.h"
#include "AnimInstance.generated.h"

struct FAnimNode_Base;
struct FPoseContext;
class AActor;
class APawn;
class USkeletalMeshComponent;


UCLASS()
class UAnimInstance : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UAnimInstance() override;

	void InitializeAnimation();   
	void UpdateAnimation(float DeltaSeconds);
	void EvaluateAnimation(FPoseContext& Output);
	void TriggerAnimNotifies(float DeltaSeconds);
	void UninitializeAnimation(); 

	USkeletalMeshComponent* GetSkelMeshComponent() const;
	AActor* GetOwningActor() const;
	APawn* TryGetPawnOwner() const;

	FAnimNode_Base* GetRootNode() const { return RootNode; }

	void AddNotifies(const TArray<FAnimNotifyEventReference>& ActiveNotifies, const float Weight);

protected:
	virtual void NativeInitializeAnimation() {}
	virtual void NativeUpdateAnimation(float DeltaSeconds) {}
	virtual void NativeUninitializeAnimation() {}
	// 클래스 없이 이름만 있는 Notify(FAnimNotifyEvent::NotifyName)를 받는다
	virtual void HandleNamedNotify(const FName& NotifyName) {}

protected:
	FAnimNode_Base* RootNode = nullptr;
	FAnimNotifyQueue NotifyQueue;

private:
	// 지난 프레임까지 Begin을 받고 아직 End를 받지 않은 NotifyState들.
	// 이번 프레임 큐와 비교해 새로 들어온 것은 Begin, 사라진 것은 End를 보낸다 (UE ActiveAnimNotifyState).
	// 주의: 시퀀스의 Notify 배열 원소를 포인터로 가리키므로 재생 중에 시퀀스 Notify를 편집하면 안 된다.
	TArray<FAnimNotifyEventReference> ActiveAnimNotifyStates;

	void EndAllAnimNotifyStates();
};
