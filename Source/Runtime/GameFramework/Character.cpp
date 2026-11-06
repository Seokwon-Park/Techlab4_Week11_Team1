#include "EnginePCH.h"
#include "Components/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character.h"

ACharacter::ACharacter()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	SetRootComponent(CapsuleComponent);

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
	SkeletalMeshComponent->SetupAttachment(CapsuleComponent);

	CharacterMovementComponent = CreateDefaultSubobject<UCharacterMovementComponent>("CharacterMovementComponent");
}

void ACharacter::AddMovementInput(const FVector& WorldDirection, float ScaleValue)
{
	CharacterMovementComponent->Velocity = WorldDirection * ScaleValue;
}

FVector ACharacter::GetVelocity() const
{
	return CharacterMovementComponent->Velocity;
}
