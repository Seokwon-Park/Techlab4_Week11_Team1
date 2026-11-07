#pragma once

#include "SkinnedMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "SkeletalMeshComponent.generated.h"

class UAnimInstance;
class UTestAnimSequence;
class UBlendSpace1D;

UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
	GENERATED_BODY()

public:
	void SetAnimInstance(UAnimInstance* InInstance);
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
	void SetEnableAnimation(const bool EnableAnimation);

	void ResetPose();
	void RefreshBoneTransformsNow();

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	void PlayAnimation(UTestAnimSequence* Sequence, bool bLoop = true);
	void PlayBlendSpace(UBlendSpace1D* BlendSpace);

	UPROPERTY(EditAnywhere)
	float BlendSpaceInput = 300.0f;

protected:
	void RefreshBoneTransforms() override;
	void OnSkeletalMeshChanged() override;

private:
	UPROPERTY()
	UAnimInstance* AnimInstance = nullptr;

	UPROPERTY(EditAnywhere)
	bool bEnableAnimation = false;

	TArray<FTransform> BoneSpaceTransforms;
};