#include "EnginePCH.h"
#include "MyAnimInstance.h"
#include "GameFrameWork/Pawn.h"

void UMyAnimInstance::NativeInitializeAnimation()
{
	//AnimNode 설정
	RootNode = &StateMachine;
}

void UMyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (APawn* Pawn = TryGetPawnOwner())
	{
		Speed = Pawn->GetVelocity().Size();
	}

	//LocomotionPlayer.X = Speed;                     // UE 에서는 핀 바인딩이 대신한다
}
