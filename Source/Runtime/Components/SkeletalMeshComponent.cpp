#include "EnginePCH.h"
#include "SkeletalMeshComponent.h"
#include "Serialization/MathSerialization.h"
#include "Animation/BonePose.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Casts.h"
#include "UObject/UObjectIterator.h"
#include "Render/LineBatcher.h"
#include "GameFramework/Actor.h"

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InInstance)
{
	AnimInstance = InInstance;
	if (AnimInstance) AnimInstance->InitializeAnimation();
}

void USkeletalMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AnimInstance)
	{
		AnimInstance->InitializeAnimation();
	}
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
	if (bEnableAnimation && AnimInstance && GetSkeletalMesh())
	{
		if (auto* Instance = Cast<UAnimSingleNodeInstance>(AnimInstance))
		{
			Instance->SetBlendSpaceInput(BlendSpaceInput);
		}

		AnimInstance->UpdateAnimation(DeltaTime);
		FPoseContext Pose(AnimInstance, GetBones());
		AnimInstance->EvaluateAnimation(Pose);
		SetBoneSpaceTransforms(Pose.Pose);
		AnimInstance->TriggerAnimNotifies(DeltaTime);

		// 루트 모션: AnimInstance에 모인 delta를 월드 공간으로 바꿔 CharacterMovement가 가져갈 때까지 쌓아 둔다
		const FRootMotionMovementParams RootMotion = AnimInstance->ConsumeExtractedRootMotion();
		if (RootMotion.bHasRootMotion)
		{
			const FTransform WorldDelta = ConvertRootMotionToWorld(RootMotion.GetRootMotionTransform());
			PendingRootMotion.Location = (bHasPendingRootMotion ? PendingRootMotion.Location : FVector::ZeroVector) + WorldDelta.Location;
			PendingRootMotion.Rotation = bHasPendingRootMotion ? (WorldDelta.Rotation * PendingRootMotion.Rotation).Normalized() : WorldDelta.Rotation;
			bHasPendingRootMotion = true;
		}
	}
	Super::TickComponent(DeltaTime);
}

const TArray<FBoneInfo>& USkeletalMeshComponent::GetBones() const
{
	static const TArray<FBoneInfo> Empty{};
	return GetSkeletalMesh() ? GetSkeletalMesh()->GetRefSkeleton() : Empty;
}

int32 USkeletalMeshComponent::FindBoneIndex(const FString& Name) const
{
	return GetSkeletalMesh() ? GetSkeletalMesh()->FindBoneIndex(Name) : -1;
}

FTransform USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
	if (BoneIndex < 0 || BoneIndex >= BoneSpaceTransforms.Num()) return FTransform();
	return BoneSpaceTransforms[BoneIndex];
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& Local)
{
	if (BoneIndex < 0 || BoneIndex >= BoneSpaceTransforms.Num()) return;
	BoneSpaceTransforms[BoneIndex] = Local;
	MarkBoneTransformsDirty();
}

void USkeletalMeshComponent::SetBoneSpaceTransforms(const FCompactPose& Pose)
{
	if (Pose.GetNumBones() != GetBones().Num())
	{
		HTR_LOG(Warning, "SetBoneSpaceTransforms: pose has {} bones, mesh has {}.", Pose.GetNumBones(), GetBones().Num());
		return;
	}
#if defined(_DEBUG)
	for (int32 i = 0; i < Pose.GetNumBones(); ++i)
	{
		const FQuat& Q = Pose[i].Rotation;
		const float LenSq = Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W;
		if (std::abs(LenSq - 1.0f) > 1e-3f)
			HTR_LOG(Warning, "Pose bone {} rotation not normalized (|q|^2 = {})", i, LenSq);
	}
#endif
	BoneSpaceTransforms = Pose.GetBones();
	MarkBoneTransformsDirty();
}

FMatrix USkeletalMeshComponent::GetBoneComponentMatrix(int32 BoneIndex) const
{
	if (BoneIndex < 0 || BoneIndex >= ComponentSpaceTransforms.Num()) return FMatrix::Identity;
	return ComponentSpaceTransforms[BoneIndex];
}

FMatrix USkeletalMeshComponent::GetBoneWorldMatrix(int32 BoneIndex) const
{
	return GetBoneComponentMatrix(BoneIndex) * GetWorldMatrix();
}

void USkeletalMeshComponent::ResetPose()
{
	OnSkeletalMeshChanged();
	MarkBoneTransformsDirty();
}

void USkeletalMeshComponent::RefreshBoneTransformsNow()
{
	UpdateBoneTransformsNow();
}

void USkeletalMeshComponent::RefreshBoneTransforms()
{
	const TArray<FBoneInfo>& Bones = GetBones();
	if (BoneSpaceTransforms.Num() != Bones.Num())
		OnSkeletalMeshChanged();

	ComponentSpaceTransforms.SetNum(Bones.Num());
	for (int i = 0; i < Bones.Num(); ++i)
	{
		const FMatrix Local = BoneSpaceTransforms[i].GetLocalMatrix();
		const int32 Parent = Bones[i].ParentIndex;
		ComponentSpaceTransforms[i] = Parent < 0 ? Local : Local * ComponentSpaceTransforms[Parent];
	}
}

void USkeletalMeshComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);   // 메시를 먼저 정해서 포즈를 바인드 포즈로 초기화한다

	FArchive& Ar = Record.GetUnderlyingArchive();
	int32 Num = Ar.IsSaving() ? BoneSpaceTransforms.Num() : 0;
	FStructuredArchive::FArray PoseArray = Record.EnterArray("BonePose", Num);

	for (int32 I = 0; I < Num; ++I)
	{
		FStructuredArchive::FRecord BoneRecord = PoseArray.EnterElement().EnterRecord();
		FString Name = Ar.IsSaving() ? GetBones()[I].Name : FString();
		FTransform Local = Ar.IsSaving() ? BoneSpaceTransforms[I] : FTransform();
		BoneRecord << SA_VALUE("Name", Name) << SA_VALUE("Local", Local);

		if (Ar.IsLoading())
		{
			// 메시의 본 순서가 바뀌어도 맞도록 이름으로 찾는다
			const int32 Index = FindBoneIndex(Name);
			if (Index >= 0)
				BoneSpaceTransforms[Index] = Local;
		}
	}

	if (Ar.IsLoading())
		RefreshBoneTransformsNow();   // 에디터 월드에서 Tick이 안 돌아도 포즈가 바로 반영된다
}

void USkeletalMeshComponent::OnSkeletalMeshChanged()
{
	BoneSpaceTransforms.Reset();
	for (const FBoneInfo& Bone : GetBones())
		BoneSpaceTransforms.Add(Bone.LocalBindPose);
}

void USkeletalMeshComponent::SetEnableAnimation(const bool EnableAnimation)
{
	bEnableAnimation = EnableAnimation;
}

void USkeletalMeshComponent::PlayAnimation(UAnimSequence* Sequence, bool bLooping)
{
	auto* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(AnimInstance);

	if (!SingleNodeInstance)
	{
		SingleNodeInstance = NewObject<UAnimSingleNodeInstance>(this);
		SetAnimInstance(SingleNodeInstance);
	}

	SingleNodeInstance->SetAnimationAsset(Sequence, bLooping);

	SetEnableAnimation(true);
}

void USkeletalMeshComponent::PlayBlendSpace(UBlendSpace1D* BlendSpace)
{
	auto* Instance = Cast<UAnimSingleNodeInstance>(AnimInstance);
	if (!Instance)
	{
		Instance = NewObject<UAnimSingleNodeInstance>(this);
		SetAnimInstance(Instance);
	}
	Instance->SetAnimationAsset(BlendSpace);
	Instance->SetBlendSpaceInput(BlendSpaceInput);
	SetEnableAnimation(true);
}
FTransform USkeletalMeshComponent::ConsumeRootMotion()
{
	const FTransform Result = PendingRootMotion;
	PendingRootMotion = FTransform();
	bHasPendingRootMotion = false;
	return Result;
}

FTransform USkeletalMeshComponent::ConvertRootMotionToWorld(const FTransform& ParentSpaceDelta) const
{
	// delta는 root 본의 부모 공간 값이다. UAL은 Armature → root 구조라 부모(Armature)를 거쳐야 컴포넌트 공간이 된다.
	const TArray<FBoneInfo>& Bones = GetBones();
	const int32 RootIndex = GetRootMotionBoneIndex();
	const int32 ParentIndex = RootIndex >= 0 ? Bones[RootIndex].ParentIndex : -1;

	// 이동: 부모(바인드 포즈) → 컴포넌트 → 월드 행렬로 방향·스케일만 적용 (Armature 스케일, 메시 0.01, yaw −180 모두 여기서)
	const FMatrix ComponentToWorld = GetWorldMatrix();
	const FMatrix ParentToWorld = ParentIndex >= 0
		? Bones[ParentIndex].InverseBindPose.Inverse() * ComponentToWorld
		: ComponentToWorld;

	FTransform Result;
	Result.Location = ParentToWorld.TransformVector(ParentSpaceDelta.Location);

	// 회전: 부모 공간 회전을 월드 공간 회전으로 바꾼다 (P × Δ × P⁻¹, P = 컴포넌트 월드 회전 × 부모 본 회전)
	FQuat ParentRotation = FQuat(GetWorldRotation());
	FQuat BoneChainRotation;
	for (int32 Index = ParentIndex; Index >= 0; Index = Bones[Index].ParentIndex)
	{
		BoneChainRotation = Bones[Index].LocalBindPose.Rotation * BoneChainRotation;
	}
	ParentRotation = (ParentRotation * BoneChainRotation).Normalized();
	Result.Rotation = (ParentRotation * ParentSpaceDelta.Rotation * ParentRotation.Inverse()).Normalized();
	return Result;
}

int32 USkeletalMeshComponent::GetRootMotionBoneIndex() const
{
	const int32 RootIndex = FindBoneIndex("root");
	if (RootIndex >= 0)
	{
		return RootIndex;
	}
	return GetBones().IsEmpty() ? -1 : 0;
}

void USkeletalMeshComponent::DrawRootDebug(FLineBatcher* LineBatcher) const
{
	const int32 RootIndex = GetRootMotionBoneIndex();
	if (!LineBatcher || RootIndex < 0 || RootIndex >= ComponentSpaceTransforms.Num())
	{
		return;
	}

	// root 본 위치에 구체
	const FVector Root = GetBoneWorldMatrix(RootIndex).TransformPosition(FVector::ZeroVector);
	const FVector4 Color(1.0f, 0.3f, 0.8f, 1.0f);
	LineBatcher->AddSphere(Root, 0.08f, Color);

	// 액터 위치를 root 높이로 내린 점 → root. 선 길이 = root가 액터(캡슐)에서 벗어난 거리
	if (const AActor* Owner = GetOwner())
	{
		const FVector ActorLocation = Owner->GetActorLocation();
		const FVector Base(ActorLocation.X, ActorLocation.Y, Root.Z);
		LineBatcher->AddLine(Base, Root, Color);
		LineBatcher->AddSphere(Base, 0.03f, Color);
	}
}
bool GShowSkeletalRootDebug = false;

void DrawSkeletalRootDebug(UWorld* World, FLineBatcher* LineBatcher)
{
	if (!World || !LineBatcher) return;
	for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		const USkeletalMeshComponent* Mesh = *It;
		if (!Mesh || Mesh->GetWorld() != World || !Mesh->IsRegistered()) continue;
		Mesh->DrawRootDebug(LineBatcher);
	}
}
