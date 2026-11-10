#pragma once

#include "SkinnedMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "SkeletalMeshComponent.generated.h"

class UAnimInstance;
class UAnimSequence;
class UBlendSpace1D;
class UAnimationAsset;
class UAnimSingleNodeInstance;
class UAnimGraphInstance;

UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
	GENERATED_BODY()

public:
    USkeletalMeshComponent();
    ~USkeletalMeshComponent() override;
    bool SetAnimBlueprint(const FString& Path);
    bool ReloadAnimBlueprint();
    UAnimGraphInstance* CreateAnimGraph();
    const FString& GetAnimBlueprintPath() const;
    const FString& GetAnimBlueprintError() const;
	void SetAnimInstance(UAnimInstance* InInstance);
	UAnimSingleNodeInstance* GetSingleNodeInstance() const;
	UAnimInstance* GetAnimInstance() const { return AnimInstance; }
	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;

	const TArray<FBoneInfo>& GetBones() const;
	int32 FindBoneIndex(const FString& Name) const;
	FTransform GetBoneLocalTransform(int32 BoneIndex) const;
	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& Local);
	const TArray<FTransform>& GetBoneSpaceTransforms() const { return BoneSpaceTransforms; }
	void SetBoneSpaceTransforms(const struct FCompactPose& Pose); // 애니메이션 평가 결과를 받는 입구
	FMatrix GetBoneComponentMatrix(int32 BoneIndex) const;
	FMatrix GetBoneWorldMatrix(int32 BoneIndex) const;

	void ResetPose();
	void RefreshBoneTransformsNow();

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	void SetAnimation(UAnimationAsset* NewAnimToPlay);
	void Play(bool bLooping = true);
	void Stop();
	bool IsPlaying() const;
	void PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLoop = true);
	void RefreshAnimationPose();

	UPROPERTY()
	float BlendSpaceInput = 300.0f;

protected:
	// 단일 애니메이션 에셋 경로/루프/재생 속도를 씬에 저장·복원한다 (Serialize에서 부른다)
	void SerializeSingleAnimation(FStructuredArchive::FRecord& Record);

	void RefreshBoneTransforms() override;
	void OnSkeletalMeshChanged() override;

private:
    UPROPERTY()
    FString AnimBlueprintPath;

    TUniquePtr<UAnimGraphInstance> ManagedAnimGraph;
    FString AnimBlueprintError;
    bool bBlueprintReloadRequested = false;
    bool bUpdatingAnimation = false;

	UPROPERTY()
	UAnimInstance* AnimInstance = nullptr;

	TArray<FTransform> BoneSpaceTransforms;
};
