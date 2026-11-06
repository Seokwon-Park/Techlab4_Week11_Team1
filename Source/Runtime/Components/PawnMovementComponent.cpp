#include "EnginePCH.h"
#include "PawnMovementComponent.h"
#include "GameFramework/Pawn.h"

void UPawnMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	PawnOwner = Cast<APawn>(GetOwner());
}
