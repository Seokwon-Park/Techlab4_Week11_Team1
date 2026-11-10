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

		// 이름 기반 Notify는 파생 AnimInstance가 HandleNamedNotify에서 처리한다
		else if (!NotifyEvent->NotifyName.IsNone())
			HandleNamedNotify(NotifyEvent->NotifyName);
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