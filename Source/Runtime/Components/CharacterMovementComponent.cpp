#include "EnginePCH.h"
#include "CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include <algorithm>

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
	return InputAcceleration.GetClampedToMaxSize(1.0f) * MaxAcceleration;
}

void UCharacterMovementComponent::PerformMovement(float DeltaTime)
{
	CalcVelocity(DeltaTime, GroundFriction, BrakingDecelerationWalking);
	Velocity.Z = 0.0f;

	const FVector Delta = Velocity * DeltaTime;
	if (Delta.SizeSquared() > SMALL_NUMBER)
		SafeMoveWithSlide(Delta, UpdatedComponent->GetWorldRotation());

	UpdateComponentVelocity();
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
		Velocity = Velocity.GetClampedToMaxSize(MaxWalkSpeed);
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
