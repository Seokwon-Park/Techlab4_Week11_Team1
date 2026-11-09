#include "EnginePCH.h"
#include "AnimInstance.h"
#include "AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/Casts.h"
#include "UObject/UObjectGlobals.h"
#include "AnimTypes.h"
#include "AnimNotify.h"

UAnimInstance::~UAnimInstance()
{
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
	NotifyQueue.Reset();

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
	USkeletalMeshComponent * SkeletalMeshComp = GetSkelMeshComponent();
	for (uint32 i = 0; i < NotifyQueue.AnimNotifies.Num(); ++i)
	{
		const FAnimNotifyEventReference& NotifyRef = NotifyQueue.AnimNotifies[i];
		const FAnimNotifyEvent* NotifyEvent = NotifyRef.GetNotify();

		if (!NotifyEvent)
			continue;

		//TODO : NotifyStatClass
		if (NotifyEvent->NotifyStateClass)
			continue;

		if (NotifyEvent->Notify)
			NotifyEvent->Notify->Notify(SkeletalMeshComp, NotifyRef.Source, NotifyRef);

		//TODO : 이름 만으로 호출하고 싶을 떄 HandleNamedNotify에서 구현
		//else if (/* NotifyName이 비어 있지 않으면 */)
		//	HandleNamedNotify(NotifyEvent->NotifyName);       // MyAnimInstance가 오버라이드
	}
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

void UAnimInstance::AddNotifies(const TArray<FAnimNotifyEventReference>& ActiveNotifies, const float Weight)
{
	NotifyQueue.AddAnimNotifies(ActiveNotifies, Weight);
}