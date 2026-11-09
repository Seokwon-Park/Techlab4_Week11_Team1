#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimNode_StateMachine.h"
#include "MyAnimInstance.generated.h"

UCLASS()
class UMyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

private:
	float Speed = 0.0f;
	bool bIsAttacking = false;
	bool bIsHit = false;

	// 상태마다 포즈를 내는 노드
	FAnimNode_StateMachine     StateMachine;
	//FAnimNode_BlendSpacePlayer LocomotionPlayer;   // Locomotion: Idle / Walk / Run Blend Space
	//FAnimNode_SequencePlayer   AttackPlayer;       // Attack: 공격 클립 한 번 재생
	//FAnimNode_SequencePlayer   HitPlayer;          // Hit: 피격 클립 한 번 재생

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
