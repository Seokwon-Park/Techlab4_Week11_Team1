#include "EnginePCH.h"
#include "AnimNotifyState_AttackWindow.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor/MyCharacter.h"
#include "Engine/World.h"
#include "Collision/HitResult.h"

void UAnimNotifyState_AttackWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	HitActors.Reset();
}

void UAnimNotifyState_AttackWindow::NotifyTick(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	AMyCharacter* OwnerCharacter = MeshComp ? Cast<AMyCharacter>(MeshComp->GetOwner()) : nullptr;
	UWorld* World = OwnerCharacter ? OwnerCharacter->GetWorld() : nullptr;
	if (!World)
		return;

	// 몸은 이동 방향을 향하므로(bOrientRotationToMovement) 액터 전방이 주먹 방향이다
	const FVector Forward = OwnerCharacter->GetActorRotation().Quaternion().GetForwardVector();
	const FRay AttackRay{ OwnerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, AttackHeight), Forward };

	FHitResult Hit;
	if (!World->SweepSingle(AttackRay, AttackRange, AttackRadius, Hit, OwnerCharacter))
		return;

	AActor* HitActor = Hit.HitComponent ? Hit.HitComponent->GetOwner() : nullptr;
	if (!HitActor || HitActors.Find(HitActor) != -1)
		return;

	HitActors.Add(HitActor);
	OwnerCharacter->DetectedHit(Hit);
}

void UAnimNotifyState_AttackWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// 맞은 액터가 공격 사이에 사라져도 댕글링 포인터를 들고 있지 않도록 바로 비운다
	HitActors.Reset();
}
