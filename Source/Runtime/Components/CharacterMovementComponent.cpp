#include "EnginePCH.h"
#include "CharacterMovementComponent.h"

UCharacterMovementComponent::UCharacterMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCharacterMovementComponent::InitializeComponent()
{
}

void UCharacterMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}
