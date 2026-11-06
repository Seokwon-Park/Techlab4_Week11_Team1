#include "EnginePCH.h"
#include "AnimInstance.h"
#include "AnimNodeBase.h"
#include "TestAnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/Casts.h"
#include "UObject/UObjectGlobals.h"

UAnimInstance::~UAnimInstance()
{
	RootNode = nullptr;
	PlayerA.Sequence = nullptr;
	PlayerB.Sequence = nullptr;
	BlendSpacePlayer.BlendSpace = nullptr;

	// NewObject 객체는 엔진의 전역 종료 정리에서 삭제
	BlendSpace = nullptr;
	SequenceA = nullptr;
	SequenceB = nullptr;
}

void UAnimInstance::InitializeAnimation()
{
	RootNode = nullptr;
	NativeInitializeAnimation();
	if (RootNode)
	{
		FAnimationInitializeContext InitContext;
		RootNode->Initialize(InitContext);
	}
}

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	// 변수를 먼저 갱신해야 노드가 이번 프레임 값(Speed 등)으로 상태와 가중치를 정한다.
	NativeUpdateAnimation(DeltaSeconds);

	if (RootNode)
	{
		FAnimationUpdateContext Context(this, DeltaSeconds);
		RootNode->Update(Context);
	}
}

void UAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	if (RootNode)
	{
		RootNode->Evaluate(Output);
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	// TODO(C): FAnimNotifyEvent가 생기면 재생 중인 시퀀스의 [PrevTime, CurrentTime] 구간에 든
	// Notify를 모아 GetSkelMeshComponent()->HandleAnimNotify()로 넘긴다.
}

void UAnimInstance::UninitializeAnimation()
{
	NativeUninitializeAnimation();
	RootNode = nullptr;
}

USkeletalMeshComponent* UAnimInstance::GetSkelMeshComponent() const
{
	return Cast<USkeletalMeshComponent>(GetOuter());
}

AActor* UAnimInstance::GetOwningActor() const
{
	USkeletalMeshComponent* OwnerComponent = GetSkelMeshComponent();
	return OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
}

APawn* UAnimInstance::TryGetPawnOwner() const
{
	return Cast<APawn>(GetOwningActor());
}

void UAnimInstance::NativeInitializeAnimation()
{
	if (!SequenceA)
		SequenceA = NewObject<UTestAnimSequence>(this);

	if (!SequenceB)
		SequenceB = NewObject<UTestAnimSequence>(this);

	SequenceA->MaxOffset = FVector(0, 1, 0);
	SequenceB->MaxOffset = FVector(0, 0, 1);

	PlayerA.Sequence = SequenceA;
	PlayerB.Sequence = SequenceB;

	PlayerA.CurrentTime = 0.0f;
	PlayerB.CurrentTime = 0.0f;

	PlayerA.bLoop = true;
	PlayerB.bLoop = true;

	Blend.A.SetLinkNode(&PlayerA);
	Blend.B.SetLinkNode(&PlayerB);
	Blend.Alpha = 0.5f;

	if (!BlendSpace)
	{
		BlendSpace = NewObject<UBlendSpace1D>(this);

		BlendSpace->AddSample(SequenceA, FVector(200, 0, 0));
		BlendSpace->AddSample(SequenceB, FVector(600, 0, 0));
	}

	BlendSpacePlayer.BlendSpace = BlendSpace;
	BlendSpacePlayer.X = 300.0f;
	BlendSpacePlayer.Phase = 0.0f;

	RootNode = &BlendSpacePlayer;
}
