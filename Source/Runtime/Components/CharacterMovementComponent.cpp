#include "EnginePCH.h"
#include "CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/Casts.h"
#include <algorithm>
#include <cmath>

UCharacterMovementComponent::UCharacterMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	Velocity = FVector::ZeroVector;
}

void UCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UCharacterMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (!PawnOwner || !UpdatedComponent || DeltaTime <= 0.0f) return;

	const FVector InputVector = ConsumeInputVector();
	ControlledCharacterMove(InputVector, DeltaTime);
}

void UCharacterMovementComponent::ControlledCharacterMove(const FVector& InputVector, float DeltaTime)
{
	Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector));
	PerformMovement(DeltaTime);
}

FVector UCharacterMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	return FVector(InputAcceleration.X, InputAcceleration.Y, 0.0f);
}

FVector UCharacterMovementComponent::ScaleInputAcceleration(const FVector& InputAcceleration) const
{
	return InputAcceleration.GetClampedToMaxSize(1.0f) * (MaxAcceleration * CmToWorld);
}

void UCharacterMovementComponent::PerformMovement(float DeltaTime)
{
	if (ApplyRootMotion(DeltaTime))
	{
		return;
	}

	CalcVelocity(DeltaTime, GroundFriction, BrakingDecelerationWalking * CmToWorld);
	Velocity.Z = 0.0f;

	const FVector Delta = Velocity * DeltaTime;
	if (Delta.SizeSquared() > SMALL_NUMBER)
		SafeMoveWithSlide(Delta, UpdatedComponent->GetWorldRotation());

	PhysicsRotation(DeltaTime);
	UpdateComponentVelocity();
}

bool UCharacterMovementComponent::ApplyRootMotion(float DeltaTime)
{
	const ACharacter* Character = Cast<ACharacter>(PawnOwner);
	USkeletalMeshComponent* Mesh = Character ? Character->SkeletalMeshComponent : nullptr;
	if (!Mesh || !Mesh->HasPendingRootMotion())
	{
		return false;
	}

	// 월드 공간 delta. 걷기 중이므로 UE처럼 수직 이동은 버린다
	const FTransform Delta = Mesh->ConsumeRootMotion();
	const FVector Move(Delta.Location.X, Delta.Location.Y, 0.0f);
	Velocity = Move * (1.0f / DeltaTime);   // 애니메이션 등에서 속도를 읽을 수 있게 남긴다

	// Root Motion의 전체 회전을 현재 회전 앞에 곱해 누적한다 (Pitch, Yaw, Roll).
	const FQuat NewQuat = (Delta.Rotation * FQuat(UpdatedComponent->GetWorldRotation())).Normalized();
	const FRotator NewRotation = NewQuat.ToFRotator();

	if (Move.SizeSquared() > SMALL_NUMBER)
		SafeMoveWithSlide(Move, NewRotation);   // Sweep: 벽에 막힌다
	else
		MoveUpdatedComponent(FVector::ZeroVector, NewRotation, false, nullptr);

	UpdateComponentVelocity();
	return true;
}

namespace
{
	// UE FMath::FixedTurn: Current에서 Desired로 최단 방향으로 최대 DeltaRate도까지만 돈다
	float FixedTurn(float Current, float Desired, float DeltaRate)
	{
		if (DeltaRate <= 0.0f) return Current;
		const float Delta = FMath::UnwindDegrees(Desired - Current);
		if (std::abs(Delta) <= DeltaRate) return Desired;
		return FMath::NormalizeAxis(Current + (Delta > 0.0f ? DeltaRate : -DeltaRate));
	}
}

void UCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (!bOrientRotationToMovement || !UpdatedComponent) return;

	const FRotator CurrentRotation = UpdatedComponent->GetWorldRotation();
	const FRotator DesiredRotation = ComputeOrientToMovementRotation(CurrentRotation);

	// 캐릭터는 수직을 유지한다 (UE ShouldRemainVertical): Pitch/Roll은 0, Yaw만 RotationRate로 돌린다
	FRotator NewRotation(0.0f, FixedTurn(CurrentRotation.Yaw, DesiredRotation.Yaw, RotationRate.Yaw * DeltaTime), 0.0f);
	if (std::abs(FMath::UnwindDegrees(NewRotation.Yaw - CurrentRotation.Yaw)) <= KINDA_SMALL_NUMBER &&
		std::abs(CurrentRotation.Pitch) <= KINDA_SMALL_NUMBER && std::abs(CurrentRotation.Roll) <= KINDA_SMALL_NUMBER)
		return;

	// 위치는 그대로, 회전만 바꾼다
	MoveUpdatedComponent(FVector::ZeroVector, NewRotation, false, nullptr);
}

FRotator UCharacterMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation) const
{
	if (Acceleration.SizeSquared() < KINDA_SMALL_NUMBER * KINDA_SMALL_NUMBER)
		return CurrentRotation;

	FRotator Result;
	Result.LookDirection(Acceleration);
	return Result;
}

void UCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (Acceleration.SizeSquared() > SMALL_NUMBER)
	{
		// UE와 같이 가속 방향이 아닌 속도 성분을 Friction만큼 깎아 방향 전환을 빠르게 한다
		const float Speed = Velocity.Size();
		const FVector AccelDir = Acceleration.GetSafeNormal();
		Velocity = Velocity - (Velocity - AccelDir * Speed) * std::min(DeltaTime * Friction, 1.0f);
		Velocity = Velocity + Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(MaxWalkSpeed * CmToWorld);
		return;
	}

	// 입력이 없으면 Friction 비율 감속 + 일정 감속. 0을 넘어 반대로 가지 않게 멈춘다
	const float Speed = Velocity.Size();
	if (Speed <= KINDA_SMALL_NUMBER)
	{
		Velocity = FVector::ZeroVector;
		return;
	}
	const float NewSpeed = std::max(Speed - (Speed * Friction + BrakingDeceleration) * DeltaTime, 0.0f);
	Velocity = Velocity * (NewSpeed / Speed);
}

void UCharacterMovementComponent::SafeMoveWithSlide(const FVector& Delta, const FRotator& NewRotation)
{
	const FVector Start = UpdatedComponent->GetWorldLocation();
	FHitResult Hit(1.0f);
	if (MoveUpdatedComponent(Delta, NewRotation, true, &Hit)) return;

	// 시작부터 겹쳐 있던 표면(예: 캡슐이 묻힌 바닥)이 이동 방향을 막지 않으면 Sweep 없이 원래 Delta만큼 간다.
	// MoveComponent가 막힌 위치에 법선 방향으로 살짝 밀어 두므로 그만큼을 빼서 Start + Delta에 맞춘다.
	if (Hit.Time <= KINDA_SMALL_NUMBER && Hit.ImpactNormal.Dot(Delta) >= 0.0f)
	{
		MoveUpdatedComponent(Start + Delta - UpdatedComponent->GetWorldLocation(), NewRotation, false, nullptr);
		return;
	}

	HandleImpact(Hit, 0.0f, Delta);

	// 벽으로 들어가는 속도 성분을 없애고 남은 이동을 표면을 따라 한 번 더 시도한다
	const FVector Normal = Hit.ImpactNormal;
	const float VDotN = Velocity.Dot(Normal);
	if (VDotN < 0.0f) Velocity = Velocity - Normal * VDotN;

	const FVector Slide = ComputeSlideVector(Delta, 1.0f - Hit.Time, Normal, Hit);
	if (Slide.SizeSquared() > SMALL_NUMBER)
		MoveUpdatedComponent(Slide, NewRotation, true, &Hit);
}
