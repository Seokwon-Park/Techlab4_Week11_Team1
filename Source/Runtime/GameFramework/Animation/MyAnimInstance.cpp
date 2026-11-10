#include "EnginePCH.h"
#include "MyAnimInstance.h"
#include "Animation/AnimSequenceFile.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFrameWork/Pawn.h"

void UMyAnimInstance::NativeInitializeAnimation()
{
	USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(GetOuter());
	SkeletalMesh->SetEnableAnimation(true);

	//AnimNode 설정
	int32 Loco = StateMachine.AddState("Locomotion", &LocomotionPlayer);
	int32 Hit = StateMachine.AddState("Hit", &HitPlayer);

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
	auto* BlendSpace = NewObject<UBlendSpace1D>();

	BlendSpace->AddSample(Idle, FVector(0, 0, 0));
	BlendSpace->AddSample(Walk, FVector(200, 0, 0));
	LocomotionPlayer.BlendSpace = BlendSpace;
	LocomotionPlayer.X = 0.f;

	AttackPlayer.Sequence = LoadSequence(
		"Assets/FBX/UAL1_Standard/UAL1_Standard_Armature_Punch_Jab.anim.bin");
	AttackPlayer.bLooping = false;

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
