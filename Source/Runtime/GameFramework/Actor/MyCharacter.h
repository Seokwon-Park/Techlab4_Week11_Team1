#pragma once

#include "GameFramework/Character.h"
#include "MyCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
struct FHitResult;

UCLASS()
class AMyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMyCharacter();

	// ACharacter는 루트를 직접 돌리지만, 여기서는 PlayerController가 갱신한 ControlRotation을
	// SpringArm/Camera가 읽으므로 몸은 돌리지 않는다
	virtual void AddControllerYawInput(float Value) override {}
	virtual void AddControllerPitchInput(float Value) override {}
	virtual void BeginPlay() override;

	virtual FRotator GetMovementInputRotation() const override;

	void Attack();
	// 공격 Sweep 결과를 받는다. 맞은 액터가 AMyCharacter면 피격 처리한다
	void DetectedHit(const FHitResult& Hit);
	void SetIsHit();

	USpringArmComponent* SpringArmComponent = nullptr;
	UCameraComponent* CameraComponent = nullptr;
};
