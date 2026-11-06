#pragma once

#include "UObject/Object.h"
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
	void InitializeAnimation();   
	void UpdateAnimation(float DeltaSeconds);
	void EvaluateAnimation(FPoseContext& Output);
	void TriggerAnimNotifies(float DeltaSeconds);
	void UninitializeAnimation(); 

	USkeletalMeshComponent* GetSkelMeshComponent() const;
	AActor* GetOwningActor() const;
	APawn* TryGetPawnOwner() const;

	FAnimNode_Base* GetRootNode() const { return RootNode; }

protected:
	virtual void NativeInitializeAnimation() {} 
	virtual void NativeUpdateAnimation(float DeltaSeconds) {}
	virtual void NativeUninitializeAnimation() {}

protected:
	FAnimNode_Base* RootNode = nullptr;
};
