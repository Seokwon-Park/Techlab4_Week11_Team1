#pragma once

#include "Pawn.h"
#include <Math/Vector.h>
#include "Character.generated.h"

class USkeletalMeshComponent;
class UCapsuleComponent;
class UCharacterMovementComponent;

UCLASS()
class ACharacter : public APawn
{
	GENERATED_BODY()

public:
	ACharacter();
	virtual void AddMovementInput(const FVector& WorldDirection, float ScaleValue) override;

	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
	UCapsuleComponent* CapsuleComponent = nullptr;
	UCharacterMovementComponent* CharacterMovementComponent = nullptr;

	virtual FVector GetVelocity() const override;
};
