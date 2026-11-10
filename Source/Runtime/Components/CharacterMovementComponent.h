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

	// 아래 속도·가속도는 UE와 같은 cm 단위(cm/s, cm/s²)다. 계산할 때 CmToWorld로 월드 단위(m)로 바꾼다.
	// Velocity는 월드 단위(m/s) 그대로다.
	UPROPERTY(EditAnywhere)
	float MaxWalkSpeed = 600.0f;
	UPROPERTY(EditAnywhere)
	float MaxAcceleration = 2048.0f;
	// 입력이 없을 때 일정하게 줄이는 감속도
	UPROPERTY(EditAnywhere)
	float BrakingDecelerationWalking = 2048.0f;
	// 클수록 방향 전환과 정지가 빠르다 (1/s, 단위 없음)
	UPROPERTY(EditAnywhere)
	float GroundFriction = 8.0f;

	// UE: 가속(입력) 방향으로 캐릭터를 RotationRate만큼씩 돌린다.
	// Controller 입력으로 몸을 직접 돌리는 Pawn(ACharacter 기본 Yaw 입력)과 같이 켜면 서로 싸운다
	UPROPERTY(EditAnywhere)
	bool bOrientRotationToMovement = false;
	// 초당 최대 회전 각도(deg/s). 캐릭터는 수직을 유지하므로 Yaw만 쓴다
	UPROPERTY(EditAnywhere)
	FRotator RotationRate = FRotator(0.0f, 360.0f, 0.0f);

	FVector GetCurrentAcceleration() const { return Acceleration; }

protected:
	// UE: 입력 벡터를 가속도로 바꾼 뒤 PerformMovement
	void ControlledCharacterMove(const FVector& InputVector, float DeltaTime);
	// 걷기 중에는 Z 입력을 버린다
	FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const;
	// 크기 1 이하로 자른 입력 × MaxAcceleration
	FVector ScaleInputAcceleration(const FVector& InputAcceleration) const;

	void PerformMovement(float DeltaTime);
	// UE: 메시에 루트 모션이 있으면 입력 대신 그 이동량으로 Actor를 옮긴다. 적용했으면 true
	bool ApplyRootMotion(float DeltaTime);
	void CalcVelocity(float DeltaTime, float Friction, float BrakingDeceleration);
	// Sweep 이동. 막히면 표면을 따라 한 번 미끄러진다
	void SafeMoveWithSlide(const FVector& Delta, const FRotator& NewRotation);

	// UE: 이동이 끝난 뒤 회전을 갱신한다 (bOrientRotationToMovement)
	void PhysicsRotation(float DeltaTime);
	// 가속 방향의 회전. 가속이 없으면 현재 회전 유지
	FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation) const;

	FVector Acceleration;
};
