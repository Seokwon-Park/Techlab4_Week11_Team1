#pragma once

#include "PawnMovementComponent.h"
#include "CharacterMovementComponent.generated.h"

// 입력 → 가속도 → 속도 → 이동 순으로 처리한다. 현재는 수평(Walking)만 다루고 중력/바닥 판정은 없다.
UCLASS()
class UCharacterMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()
public:
	UCharacterMovementComponent();
	virtual void InitializeComponent() override;
	virtual void TickComponent(float DeltaTime) override;

	UPROPERTY(EditAnywhere)
	float MaxWalkSpeed = 6.0f;
	UPROPERTY(EditAnywhere)
	float MaxAcceleration = 20.0f;
	// 입력이 없을 때 일정하게 줄이는 감속도
	UPROPERTY(EditAnywhere)
	float BrakingDecelerationWalking = 20.0f;
	// 클수록 방향 전환과 정지가 빠르다
	UPROPERTY(EditAnywhere)
	float GroundFriction = 8.0f;

	FVector GetCurrentAcceleration() const { return Acceleration; }

protected:
	// UE: 입력 벡터를 가속도로 바꾼 뒤 PerformMovement
	void ControlledCharacterMove(const FVector& InputVector, float DeltaTime);
	// 걷기 중에는 Z 입력을 버린다
	FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const;
	// 크기 1 이하로 자른 입력 × MaxAcceleration
	FVector ScaleInputAcceleration(const FVector& InputAcceleration) const;

	void PerformMovement(float DeltaTime);
	void CalcVelocity(float DeltaTime, float Friction, float BrakingDeceleration);
	// Sweep 이동. 막히면 표면을 따라 한 번 미끄러진다
	void SafeMoveWithSlide(const FVector& Delta, const FRotator& NewRotation);

	FVector Acceleration;
};
