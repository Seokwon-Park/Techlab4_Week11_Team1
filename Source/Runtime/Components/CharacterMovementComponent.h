#pragma once

#include "PawnMovementComponent.h"
#include "CharacterMovementComponent.generated.h"

UCLASS()
class UCharacterMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()
public:
	UCharacterMovementComponent();
	virtual void InitializeComponent() override;
	virtual void TickComponent(float DeltaTime) override;
};
