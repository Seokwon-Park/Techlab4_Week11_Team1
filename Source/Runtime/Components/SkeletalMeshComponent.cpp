#include "EnginePCH.h"
#include "SkeletalMeshComponent.h"
#include "Serialization/MathSerialization.h"
#include "Animation/BonePose.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimGraphInstance.h"
#include "Animation/AnimBlueprintCompiler.h"
#include "Animation/AnimBlueprintFile.h"
#include "Asset/AssetUtils.h"
#include "Core/Misc/Paths.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Casts.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/BlendSpaceFile.h"
#include "Asset/AssetManager.h"

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InInstance)
{
	if (AnimInstance == InInstance)
	{
		return;
	}
	if (AnimInstance)
	{
		AnimInstance->UninitializeAnimation();
	}
    AnimInstance = InInstance;

    if (ManagedAnimGraph && ManagedAnimGraph.get() != InInstance)
    {
        ManagedAnimGraph.reset();
    }

    AnimBlueprintPath.clear();
    bBlueprintReloadRequested = false;

	if (AnimInstance)
	{
		AnimInstance->InitializeAnimation();
	}
}

void USkeletalMeshComponent::BeginPlay()
{
	Super::BeginPlay();

    if (!AnimBlueprintPath.empty() && (bBlueprintReloadRequested || !ManagedAnimGraph))
    {
        ReloadAnimBlueprint();
    }

	if (AnimInstance)
	{
		AnimInstance->InitializeAnimation();

		if (auto* Instance = GetSingleNodeInstance())
		{
			if (Instance->GetCurrentAsset())
			{
				Instance->SetPlaying(true);
			}
		}
	}
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    if (bBlueprintReloadRequested)
    {
        bBlueprintReloadRequested = false;
        ReloadAnimBlueprint();
    }

	if (AnimInstance && GetSkeletalMesh())
	{
        bUpdatingAnimation = true;
		if (auto* Instance = Cast<UAnimSingleNodeInstance>(AnimInstance))
		{
			Instance->SetBlendSpaceInput(BlendSpaceInput);
		}

		AnimInstance->UpdateAnimation(DeltaTime);
		FPoseContext Pose(AnimInstance, GetBones());
		AnimInstance->EvaluateAnimation(Pose);
		SetBoneSpaceTransforms(Pose.Pose);
		AnimInstance->TriggerAnimNotifies(DeltaTime);
        bUpdatingAnimation = false;
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
    FArchive& Ar = Record.GetUnderlyingArchive();
    UAnimInstance* SavedInstance = AnimInstance;

    if (Ar.IsSaving() && (!AnimBlueprintPath.empty() || ManagedAnimGraph))
    {
        AnimInstance = nullptr;
    }

    Super::Serialize(Record);

    if (Ar.IsSaving())
    {
        AnimInstance = SavedInstance;
    }

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

	SerializeSingleAnimation(Record);

    if (Ar.IsLoading())
    {
        if (!AnimBlueprintPath.empty())
        {
            ReloadAnimBlueprint();
        }

        RefreshBoneTransformsNow();
    }
}

void USkeletalMeshComponent::SerializeSingleAnimation(FStructuredArchive::FRecord& Record)
{
	// 디테일 패널에서 고른 단일 애니메이션(SingleNodeInstance의 에셋)은 AnimInstance 객체가 아니라
	// 에셋 경로로 저장한다. 없으면 필드를 쓰지 않으므로 예전 씬도 그대로 읽힌다.
	FArchive& Ar = Record.GetUnderlyingArchive();
	UAnimSingleNodeInstance* Instance = GetSingleNodeInstance();
	UAnimationAsset* Asset = Instance ? Instance->GetCurrentAsset() : nullptr;
	const bool bHasAnimation = Ar.IsSaving() && Asset && !Asset->GetAssetPath().empty();

	TOptional<FStructuredArchive::FSlot> AnimSlot = Record.TryEnterField("SingleAnimation", bHasAnimation);
	if (!AnimSlot.IsSet())
		return;

	FStructuredArchive::FRecord AnimRecord = AnimSlot.GetValue().EnterRecord();
	FString Path = Ar.IsSaving() ? Asset->GetAssetPath() : FString();
	bool bLoop = Ar.IsSaving() ? Instance->IsLooping() : true;
	float Rate = Ar.IsSaving() ? Instance->GetPlayRate() : 1.0f;
	AnimRecord << SA_VALUE("Asset", Path) << SA_VALUE("Looping", bLoop) << SA_VALUE("PlayRate", Rate);

	if (!Ar.IsLoading())
		return;

	// AssetManager 캐시로 불러온다 (디테일 패널이 고를 때와 같은 객체)
	UAnimationAsset* Loaded = BlendSpaceFile::IsBlendSpacePath(Path)
		? static_cast<UAnimationAsset*>(UAssetManager::LoadBlendSpace(Path))
		: static_cast<UAnimationAsset*>(UAssetManager::LoadAnimSequence(Path));
	if (!Loaded)
	{
		HTR_LOG(Warning, "SkeletalMeshComponent: single animation not found: {}", Path);
		return;
	}

	// 재생 시작은 BeginPlay가 한다 (에디터 월드에서는 첫 프레임 포즈만 보인다)
	SetAnimation(Loaded);
	if (UAnimSingleNodeInstance* NewInstance = GetSingleNodeInstance())
	{
		NewInstance->SetLooping(bLoop);
		NewInstance->SetPlayRate(Rate);
	}
}

void USkeletalMeshComponent::OnSkeletalMeshChanged()
{
    bBlueprintReloadRequested = !AnimBlueprintPath.empty();
	BoneSpaceTransforms.Reset();
	for (const FBoneInfo& Bone : GetBones())
		BoneSpaceTransforms.Add(Bone.LocalBindPose);
}
UAnimSingleNodeInstance* USkeletalMeshComponent::GetSingleNodeInstance() const
{
	return Cast<UAnimSingleNodeInstance>(AnimInstance);
}

void USkeletalMeshComponent::SetAnimation(UAnimationAsset* NewAnimToPlay)
{
	if (NewAnimToPlay && !Cast<UAnimSequence>(NewAnimToPlay) && !Cast<UBlendSpace1D>(NewAnimToPlay))
	{
		HTR_LOG(Warning, "SetAnimation: unsupported animation asset type");
		return;
	}
    AnimBlueprintPath.clear();
    AnimBlueprintError.clear();
    bBlueprintReloadRequested = false;
	auto* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(AnimInstance);

	if (!SingleNodeInstance)
	{
		SingleNodeInstance = NewObject<UAnimSingleNodeInstance>(this);
		SetAnimInstance(SingleNodeInstance);
	}

	SingleNodeInstance->SetAnimationAsset(NewAnimToPlay);
	SingleNodeInstance->SetBlendSpaceInput(BlendSpaceInput);

	RefreshAnimationPose();
}

void USkeletalMeshComponent::Play(bool bLooping)
{
	if (auto* Instance = GetSingleNodeInstance())
	{
		Instance->SetLooping(bLooping);
		Instance->SetPlaying(true);
	}
}

void USkeletalMeshComponent::Stop()
{
	if (auto* Instance = GetSingleNodeInstance())
	{
		Instance->SetPlaying(false);
	}
}

bool USkeletalMeshComponent::IsPlaying() const
{
	const auto* Instance = GetSingleNodeInstance();
	return Instance && Instance->IsPlaying();
}

void USkeletalMeshComponent::PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping)
{
	if (NewAnimToPlay && !Cast<UAnimSequence>(NewAnimToPlay) && !Cast<UBlendSpace1D>(NewAnimToPlay))
	{
		return;
	}

	SetAnimation(NewAnimToPlay);
	Play(bLooping);
}

void USkeletalMeshComponent::RefreshAnimationPose()
{
    if (!AnimInstance)
    {
        ResetPose();
        RefreshBoneTransformsNow();
        return;
    }

	if (auto* Instance = GetSingleNodeInstance())
	{
		Instance->SetBlendSpaceInput(BlendSpaceInput);
	}

	FPoseContext Pose(AnimInstance, GetBones());
	AnimInstance->EvaluateAnimation(Pose);
	SetBoneSpaceTransforms(Pose.Pose);
	RefreshBoneTransformsNow();
}

USkeletalMeshComponent::USkeletalMeshComponent()
{
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    if (ManagedAnimGraph)
    {
        if (AnimInstance == ManagedAnimGraph.get())
        {
            SetAnimInstance(nullptr);
        }
        else
        {
            ManagedAnimGraph->UninitializeAnimation();
        }
    }
}

const FString& USkeletalMeshComponent::GetAnimBlueprintPath() const
{
    return AnimBlueprintPath;
}

const FString& USkeletalMeshComponent::GetAnimBlueprintError() const
{
    return AnimBlueprintError;
}

bool USkeletalMeshComponent::SetAnimBlueprint(const FString& Path)
{
    if (bUpdatingAnimation)
    {
        AnimBlueprintError = "Load or reload Animation Blueprints from actor Tick, outside animation callbacks.";
        return false;
    }

    if (Path.empty())
    {
        const bool bHadBlueprint = !AnimBlueprintPath.empty();
        AnimBlueprintPath.clear();
        AnimBlueprintError.clear();
        bBlueprintReloadRequested = false;

        if (ManagedAnimGraph && bHadBlueprint)
        {
            SetAnimInstance(nullptr);
            ResetPose();
            RefreshBoneTransformsNow();
        }

        return true;
    }

    AnimBlueprintError.clear();
    const FString Key = FAssetUtils::GetAssetKey(Path);
    const FString FilePath = FAssetUtils::ResolveAssetFilePath(Key);
    FAnimBlueprintData Data;

    if (!AnimBlueprintFile::IsAnimBlueprintPath(Key) || !AnimBlueprintFile::Read(FPaths::FromUtf8(FilePath.empty() ? Key : FilePath), Data, AnimBlueprintError))
    {
        if (AnimBlueprintError.empty())
        {
            AnimBlueprintError = "Use an .animblueprint asset.";
        }

        return false;
    }

    TUniquePtr<UAnimGraphInstance> Instance;
    AnimBlueprintCompiler::FNodeMap NodeIndices;

    if (!AnimBlueprintCompiler::Build(Data, *this, Instance, NodeIndices, AnimBlueprintError))
    {
        return false;
    }

    SetAnimInstance(Instance.get());
    ManagedAnimGraph = std::move(Instance);
    AnimBlueprintPath = Key;
    bBlueprintReloadRequested = false;
    AnimBlueprintError.clear();
    RefreshAnimationPose();
    return true;
}

bool USkeletalMeshComponent::ReloadAnimBlueprint()
{
    const FString Path = AnimBlueprintPath;
    bBlueprintReloadRequested = false;

    if (Path.empty())
    {
        return false;
    }

    if (!SetAnimBlueprint(Path))
    {
        HTR_LOG(Warning, "AnimBlueprint load failed: {} ({})", Path, AnimBlueprintError);
        return false;
    }

    return true;
}

UAnimGraphInstance* USkeletalMeshComponent::CreateAnimGraph()
{
    if (bUpdatingAnimation)
    {
        AnimBlueprintError = "Create animation graphs outside animation callbacks.";
        return nullptr;
    }

    if (!AnimBlueprintPath.empty())
    {
        AnimBlueprintError = "An Animation Blueprint is assigned. Use GetAnimGraph to update its variables.";
        return nullptr;
    }

    AnimBlueprintError.clear();

    if (UAnimGraphInstance* Existing = Cast<UAnimGraphInstance>(AnimInstance))
    {
        Existing->ResetGraph();
        return Existing;
    }

    TUniquePtr<UAnimGraphInstance> Instance(NewObject<UAnimGraphInstance>(this));
    SetAnimInstance(Instance.get());
    ManagedAnimGraph = std::move(Instance);
    return ManagedAnimGraph.get();
}
