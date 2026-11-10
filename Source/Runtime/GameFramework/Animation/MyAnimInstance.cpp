#include "EnginePCH.h"
#include "MyAnimInstance.h"
#include "Animation/AnimSequenceFile.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFrameWork/Pawn.h"
#include "GameFramework/Actor/MyCharacter.h"
#include "Engine/World.h"
#include "Collision/HitResult.h"
#include "Animation/AnimNotify.h"

namespace
{
	// Punch_Jab(30fps)에서 왼팔이 가장 뻗는 프레임 7 ≈ 0.233초
	const FName AttackHitNotifyName("AttackHit");
	constexpr float AttackHitNotifyTime = 7.0f / 30.0f;

	// 발이 바닥에 닿는 프레임 (발목/발끝 높이가 최저점에 닿는 순간, 30fps 기준)
	// 프레임 0은 (Prev, Curr] 검사에 걸리지 않으므로 반 프레임 뒤로 둔다
	constexpr float WalkFootstepFrameL = 0.5f;
	constexpr float WalkFootstepFrameR = 20.0f;
	constexpr float JogFootstepFrameL = 1.0f;
	constexpr float JogFootstepFrameR = 15.0f;

	const char* FootstepSoundL = "Assets/Sounds/Footstep_StoneL.flac";
	const char* FootstepSoundR = "Assets/Sounds/Footstep_StoneR.flac";

	void AddFootstepNotify(UAnimSequence* Sequence, float Frame, const char* SoundPath)
	{
		if (!Sequence || !Sequence->GetDataModel())
			return;

		const FFrameRate Rate = Sequence->GetDataModel()->GetFrameRate();
		const float Time = Frame * static_cast<float>(Rate.Denominator) / static_cast<float>(Rate.Numerator);

		FGuid Guid;
		UAnimNotify_PlaySound* Notify = Sequence->AddNotify<UAnimNotify_PlaySound>(Time, &Guid);
		Notify->SoundPath = SoundPath;
		Notify->Volume = 1.0f;

		// BlendSpace 구간에서 Walk/Jog 발소리가 겹치지 않도록 우세한 샘플만 울린다
		Sequence->SetNotifyTriggerWeightThreshold(Guid, 0.5f);
	}
}

void UMyAnimInstance::NativeInitializeAnimation()
{
	USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(GetOuter());
	SkeletalMesh->SetEnableAnimation(true);

	//AnimNode 설정
	int32 Loco = StateMachine.AddState("Locomotion", &LocomotionPlayer);
	// Hit 상태를 빠져나가는 순간 피격 플래그를 내린다
	FAnimStateDesc HitState;
	HitState.Name = "Hit";
	HitState.Pose.SetLinkNode(&HitPlayer);
	HitState.OnExit = [this](UAnimInstance*) { bIsHit = false; };
	int32 Hit = StateMachine.AddState(HitState);

	// Attack 상태를 빠져나가는 순간(정상 종료든 Hit에 끊기든) 공격 플래그를 내린다
	FAnimStateDesc AttackState;
	AttackState.Name = "Attack";
	AttackState.Pose.SetLinkNode(&AttackPlayer);
	AttackState.OnExit = [this](UAnimInstance*) { bIsAttacking = false; };
	int32 Attack = StateMachine.AddState(AttackState);

	//TEMP : BlendSpace 생성 밎 설정
	auto LoadSequence = [&](const char* Path) -> UAnimSequence*
		{
			auto* Sequence = NewObject<UAnimSequence>(SkeletalMesh);
			FString Error;

			if (!AnimSequenceFile::Read(Path, *Sequence, Error))
			{
				HTR_LOG(Error, "Animation load failed: {} ({})", Path, Error);
				delete Sequence;
				return nullptr;
			}

			return Sequence;
		};

	auto* Idle = LoadSequence(
		"Assets/FBX/UAL1_Standard/UAL1_Standard_Armature_Idle_Loop.anim.bin");
	auto* Walk = LoadSequence(
		"Assets/FBX/UAL1_Standard/UAL1_Standard_Armature_Walk_Loop.anim.bin");
	auto* Running = LoadSequence(
		"Assets/FBX/UAL1_Standard/UAL1_Standard_Armature_Jog_Fwd_Loop.anim.bin");

	AddFootstepNotify(Walk, WalkFootstepFrameL, FootstepSoundL);
	AddFootstepNotify(Walk, WalkFootstepFrameR, FootstepSoundR);
	AddFootstepNotify(Running, JogFootstepFrameL, FootstepSoundL);
	AddFootstepNotify(Running, JogFootstepFrameR, FootstepSoundR);

	auto* BlendSpace = NewObject<UBlendSpace1D>();

	BlendSpace->AddSample(Idle, FVector(0, 0, 0));
	BlendSpace->AddSample(Walk, FVector(200, 0, 0));
	BlendSpace->AddSample(Running, FVector(550, 0, 0));
	LocomotionPlayer.BlendSpace = BlendSpace;
	LocomotionPlayer.X = 0.f;

	AttackPlayer.Sequence = LoadSequence(
		"Assets/FBX/UAL1_Standard/UAL1_Standard_Armature_Punch_Jab.anim.bin");
	AttackPlayer.bLooping = false;

	//Attack Sequence에 "AttackHit" Notify 추가
	if (AttackPlayer.Sequence)
		AttackPlayer.Sequence->AddNotify(AttackHitNotifyTime, AttackHitNotifyName);

	HitPlayer.Sequence = LoadSequence(
		"Assets/FBX/UAL1_Standard/UAL1_Standard_Armature_Hit_Chest.anim.bin");
	HitPlayer.bLooping = false;

	StateMachine.SetInitialState(Loco);

	FAnimTransitionDesc LocoToAttack;
	LocoToAttack.From = Loco;
	LocoToAttack.To = Attack;
	LocoToAttack.Rule = [this](const FAnimTransitionContext&) { return bIsAttacking; };
	StateMachine.AddTransition(LocoToAttack);

	FAnimTransitionDesc AttackToLoco;
	AttackToLoco.From = Attack;
	AttackToLoco.To = Loco;
	AttackToLoco.Rule = [this](const FAnimTransitionContext&) { return AttackPlayer.IsFinished(); };
	StateMachine.AddTransition(AttackToLoco);

	//AnyTransition으로 Hit 추가
	FAnimTransitionDesc AnyToHit;
	AnyToHit.From = INDEX_NONE;
	AnyToHit.To = Hit;
	AnyToHit.Priority = -1;
	AnyToHit.CrossfadeDuration = 0.1f;
	AnyToHit.Rule = [this](const FAnimTransitionContext&) { return bIsHit; };
	StateMachine.AddTransition(AnyToHit);

	// Hit 클립이 끝나면 Locomotion으로 복귀 (OnExit에서 bIsHit = false)
	FAnimTransitionDesc HitToLoco;
	HitToLoco.From = Hit;
	HitToLoco.To = Loco;
	HitToLoco.Rule = [this](const FAnimTransitionContext&) { return HitPlayer.IsFinished(); };
	StateMachine.AddTransition(HitToLoco);

	RootNode = &StateMachine;
}

void UMyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (APawn* Pawn = TryGetPawnOwner())
	{
		// UE GroundSpeed: 수평 속도만, cm/s 단위 (BlendSpace 샘플이 UE 값 기준)
		const FVector Velocity = Pawn->GetVelocity();
		Speed = FVector(Velocity.X, Velocity.Y, 0.0f).Size() * WorldToCm;
	}

	LocomotionPlayer.X = Speed;                     // UE 에서는 핀 바인딩이 대신한다
}

void UMyAnimInstance::HandleNamedNotify(const FName& NotifyName)
{
	if (!(NotifyName == AttackHitNotifyName)) return;

	AMyCharacter* OwnerCharacter = Cast<AMyCharacter>(TryGetPawnOwner());
	UWorld* World = OwnerCharacter ? OwnerCharacter->GetWorld() : nullptr;
	if (!World) return;

	// 몸은 이동 방향을 향하므로(bOrientRotationToMovement) 액터 전방이 주먹 방향이다
	const FVector Forward = OwnerCharacter->GetActorRotation().Quaternion().GetForwardVector();
	const FRay AttackRay{ OwnerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, AttackHeight), Forward };

	FHitResult Hit;
	if (World->SweepSingle(AttackRay, AttackRange, AttackRadius, Hit, OwnerCharacter))
		OwnerCharacter->DetectedHit(Hit);
}
