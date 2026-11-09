#pragma once

#include "GameFramework/Character.h"
#include "MyCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

// UE 3인칭 템플릿 구성의 테스트 캐릭터. 마우스 회전은 캐릭터 몸이 아니라 ControlRotation으로 가고,
//  - SpringArm(bUsePawnControlRotation)이 ControlRotation의 Yaw/Pitch를 따라 캐릭터 주위를 공전, 카메라는 팔 끝을 따른다
//  - 몸은 CMC bOrientRotationToMovement로 이동 방향을 향한다
// 이동 방향은 ControlRotation의 Yaw 기준 (카메라가 보는 수평 방향)
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

	virtual FRotator GetMovementInputRotation() const override;

	USpringArmComponent* SpringArmComponent = nullptr;
	UCameraComponent* CameraComponent = nullptr;
};
