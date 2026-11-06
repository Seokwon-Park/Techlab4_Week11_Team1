#pragma once

#include "UObject/Object.h"
#include "AnimNode_SequencePlayer.h"
#include "AnimNode_TwoWayBlend.h"
#include "AnimNode_BlendSpacePlayer.h"

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
	void SetBlendSpaceInput(float Value) { BlendSpacePlayer.X = Value; }

protected:
	virtual void NativeInitializeAnimation();
	virtual void NativeUpdateAnimation(float DeltaSeconds) {}
	virtual void NativeUninitializeAnimation() {}

protected:
	FAnimNode_Base* RootNode = nullptr;

private:
	UTestAnimSequence* SequenceA = nullptr;
	UTestAnimSequence* SequenceB = nullptr;

	FAnimNode_SequencePlayer PlayerA;
	FAnimNode_SequencePlayer PlayerB;

	FAnimNode_TwoWayBlend Blend;
	UBlendSpace1D* BlendSpace = nullptr;
	FAnimNode_BlendSpacePlayer BlendSpacePlayer;
};
