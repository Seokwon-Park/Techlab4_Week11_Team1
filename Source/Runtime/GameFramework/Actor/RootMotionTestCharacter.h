#pragma once

#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "RootMotionTestCharacter.generated.h"

class UAnimSequence;
class UTextRenderComponent;

// TEMP: 루트 모션 시연용 캐릭터. RM 클립 하나를 루프 재생하고, bUseRootMotion으로 ON/OFF를 비교한다.
// 시퀀스는 AssetManager 캐시를 공유한다. 루트 모션 설정(Enable / Force Root Lock)은 시퀀스 에셋에 저장된 값을 쓴다.
UCLASS()
class ARootMotionTestCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARootMotionTestCharacter();
	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	// 이 캐릭터 AnimInstance의 Root Motion Mode. BeginPlay에서 SetRootMotionMode로 넣는다
	// (시퀀스의 Enable Root Motion / Force Root Lock은 뷰어에서 설정·저장한다)
	UPROPERTY(EditAnywhere)
	ERootMotionMode RootMotionMode = ERootMotionMode::RootMotionFromEverything;

	UPROPERTY(EditAnywhere)
	FString MeshPath = "Assets/FBX/UAL2_Standard_RM/UAL2_Standard_RM.fbx.bin";

	UPROPERTY(EditAnywhere)
	FString AnimationPath = "Assets/FBX/UAL2_Standard_RM/UAL2_Standard_RM_Armature_Zombie_Walk_Fwd_Loop.anim.bin";

private:
	UAnimSequence* Sequence = nullptr;
	UTextRenderComponent* LabelComponent = nullptr;  // 머리 위 "Root Motion ON/OFF" 표시
	UTextRenderComponent* RootLabelComponent = nullptr;  // root 본을 따라다니는 "root" 표시
};
