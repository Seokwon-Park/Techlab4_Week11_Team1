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
};
