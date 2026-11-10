#include "EnginePCH.h"
#include "GameFramework/Actor/RootMotionTestCharacter.h"
#include "Animation/AnimSequence.h"
#include "Asset/AssetManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include <cmath>
#include "Render/SkeletalMesh.h"
#include "UObject/Casts.h"

ARootMotionTestCharacter::ARootMotionTestCharacter()
{
	// CharacterAnimationTest.scene의 AMyCharacter와 같은 값.
	// UAL 마네킹은 cm 단위라 0.01배, 앞을 보게 Yaw −180, 발이 캡슐 바닥에 오도록 내린다.
	CapsuleComponent->CapsuleHalfHeight = 1.0f;
	CapsuleComponent->CapsuleRadius = 0.5f;
	SkeletalMeshComponent->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -0.97f), FRotator(0.0f, -180.0f, 0.0f));
	SkeletalMeshComponent->SetRelativeScale3D(FVector(0.01f, 0.01f, 0.01f));

	// 두 캐릭터를 구분하는 머리 위 라벨. 캡슐 꼭대기보다 조금 위
	LabelComponent = CreateDefaultSubobject<UTextRenderComponent>("LabelComponent");
	LabelComponent->SetupAttachment(CapsuleComponent);
	LabelComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.4f));
	LabelComponent->SetTextSize(0.3f);
	LabelComponent->SetText("Root Motion");

	// "root" 라벨. 위치는 Tick에서 root 본을 따라간다 (구체·선은 Settings > Debug Draw (PIE) > Show Root)
	RootLabelComponent = CreateDefaultSubobject<UTextRenderComponent>("RootLabelComponent");
	RootLabelComponent->SetupAttachment(CapsuleComponent);
	RootLabelComponent->SetTextSize(0.15f);
	RootLabelComponent->SetText("root");
	RootLabelComponent->SetVisibility(false);   // 에디터에서는 Tick이 돌지 않으므로 숨겨 둔다. PIE에서 Show Root로 켠다
}

void ARootMotionTestCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// "root" 라벨: Settings > Debug Draw (PIE) > Show Root일 때만 보이고, root 본 위치(조금 위)를 따라간다
	const int32 RootIndex = SkeletalMeshComponent->GetRootMotionBoneIndex();
	if (RootLabelComponent)
	{
		RootLabelComponent->SetVisibility(GShowSkeletalRootDebug && RootIndex >= 0);
	}
	if (RootLabelComponent && RootIndex >= 0)
	{
		const FVector Root = SkeletalMeshComponent->GetBoneWorldMatrix(RootIndex).TransformPosition(FVector::ZeroVector);
		RootLabelComponent->SetWorldLocation(Root + FVector(0.0f, 0.0f, 0.15f), false);
	}

	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetPlayerController() : nullptr;
	const APlayerCameraManager* CameraManager = Controller ? Controller->GetCameraManager() : nullptr;
	if (!CameraManager)
	{
		return;
	}

	// 라벨이 카메라를 향하도록 Yaw만 돌린다.
	// 글자는 로컬 YZ 평면에 놓이고 -X 쪽에서 읽히므로, 카메라 반대 방향을 +X로 둔다. (뒤집혀 보이면 +180을 뺀다)
	const FVector CameraLocation = CameraManager->GetCameraCachePOV().Location;
	auto FaceCamera = [&CameraLocation](UTextRenderComponent* Label)
	{
		if (!Label)
		{
			return;
		}
		const FVector ToCamera = CameraLocation - Label->GetWorldLocation();
		if (ToCamera.X * ToCamera.X + ToCamera.Y * ToCamera.Y < 1.0e-6f)
		{
			return;
		}
		const float Yaw = std::atan2(ToCamera.Y, ToCamera.X) * (180.0f / 3.14159265f) + 180.0f;
		Label->SetWorldRotation(FRotator(0.0f, Yaw, 0.0f), false);
	};
	FaceCamera(LabelComponent);
	FaceCamera(RootLabelComponent);
}

void ARootMotionTestCharacter::BeginPlay()
{
	Super::BeginPlay();
	// 라벨은 설정 조합의 결과를 보여준다 (GetAnimationPose의 root 고정 조건과 같다)
	const char* ModeName = "Root Motion ON";
	switch (RootMotionMode)
	{
	case ERootMotionMode::NoRootMotionExtraction: ModeName = "Root Motion OFF"; break;
	case ERootMotionMode::IgnoreRootMotion:       ModeName = "Root Lock (Ignore)"; break;
	default: break;
	}
	LabelComponent->SetText(ModeName);

	// 씬에 메시가 저장돼 있으면 그대로 쓰고, 없을 때만 MeshPath로 불러온다
	FString Error;
	if (!SkeletalMeshComponent->GetSkeletalMesh())
	{
		USkeletalMesh* Mesh = Cast<USkeletalMesh>(UAssetManager::LoadFbxMesh(MeshPath, Error));
		if (!Mesh)
		{
			HTR_LOG(Error, "RootMotionTest: mesh load failed: {} ({})", MeshPath, Error);
			return;
		}
		SkeletalMeshComponent->SetSkeletalMesh(Mesh);
	}

	// 뷰어와 같은 캐시 객체를 쓰므로, 뷰어에서 바꾼 Enable Root Motion / Force Root Lock이 그대로 적용된다
	Sequence = UAssetManager::LoadAnimSequence(AnimationPath);
	if (!Sequence)
	{
		HTR_LOG(Error, "RootMotionTest: animation load failed: {}", AnimationPath);
		return;
	}

	SkeletalMeshComponent->PlayAnimation(Sequence, true);
	if (UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance())
	{
		AnimInstance->SetRootMotionMode(RootMotionMode);   // 캐릭터별 설정 (언리얼도 런타임엔 이렇게 바꾼다)
	}
	HTR_LOG(Info, "RootMotionTest: {} plays {} ({})", GetName(), Sequence->GetClipName(), ModeName);
}
