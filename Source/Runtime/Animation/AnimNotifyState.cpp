#include "EnginePCH.h"
#include "AnimNotifyState.h"
#include "AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	UAnimInstance* AnimInstanceOf(USkeletalMeshComponent* MeshComp)
	{
		return MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	}
}

void UAnimNotifyState_Named::NotifyBegin(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (UAnimInstance* Instance = AnimInstanceOf(MeshComp))
		Instance->HandleNamedNotifyStateBegin(FName(NotifyName), TotalDuration);
}

void UAnimNotifyState_Named::NotifyTick(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	if (UAnimInstance* Instance = AnimInstanceOf(MeshComp))
		Instance->HandleNamedNotifyStateTick(FName(NotifyName), FrameDeltaTime);
}

void UAnimNotifyState_Named::NotifyEnd(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UAnimInstance* Instance = AnimInstanceOf(MeshComp))
		Instance->HandleNamedNotifyStateEnd(FName(NotifyName));
}
