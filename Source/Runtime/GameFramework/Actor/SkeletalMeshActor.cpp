#include "EnginePCH.h"
#include "SkeletalMeshActor.h"
#include "Render/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceFile.h"
#include "Animation/BlendSpace1D.h"

namespace
{
    // A의 임포터가 붙기 전까지 쓰는 테스트 막대. 에셋은 하나만 만들어 모든 액터가 공유한다
    USkeletalMesh* GetTestBoneStickMesh()
    {
        static USkeletalMesh* Cached = nullptr;
        if (Cached) return Cached;

        USkeletalMesh* Mesh = NewObject<USkeletalMesh>();
        FString Error;
        if (Mesh->Build(MakeTestBoneStick(3, 1.0f), Error))
            Cached = Mesh;   // 실패하면(렌더러 초기화 전 등) 다음 생성 때 다시 시도한다
        return Cached;
    }
}

// TEST
void ASkeletalMeshActor::BeginPlay()
{
    Super::BeginPlay();

    auto LoadSequence = [this](const char* Path) -> UAnimSequence*
        {
            auto* Sequence = NewObject<UAnimSequence>(SkeletalMeshComponent);
            FString Error;

            if (!AnimSequenceFile::Read(Path, *Sequence, Error))
            {
                HTR_LOG(Error, "Animation load failed: {} ({})", Path, Error);
                delete Sequence;
                return nullptr;
            }

            return Sequence;
        };

    // FBX 임포터가 스택마다 "FBX이름_스택이름.anim.bin"으로 저장한다.
    // 기존 UAL1_Standard1(스택 1)·UAL1_Standard42(스택 42)와 같은 클립이다.
    auto* Idle = LoadSequence(
        "Assets/FBX/UAL1_Standard/UAL1_Standard_Armature_Crouch_Fwd_Loop.anim.bin");
    auto* Walk = LoadSequence(
        "Assets/FBX/UAL1_Standard/UAL1_Standard_Armature_Walk_Loop.anim.bin");


    auto* BlendSpace = NewObject<UBlendSpace1D>(SkeletalMeshComponent);

    BlendSpace->AddSample(Idle, FVector(0, 0, 0));
    BlendSpace->AddSample(Walk, FVector(200, 0, 0));

    SkeletalMeshComponent->BlendSpaceInput = 0.0f;
    SkeletalMeshComponent->PlayBlendSpace(BlendSpace);
}

ASkeletalMeshActor::ASkeletalMeshActor()
{
    SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("USkeletalMeshComponent");
    SkeletalMeshComponent->SetupAttachment(GetRootComponent());
    SkeletalMeshComponent->SetSkeletalMesh(GetTestBoneStickMesh());
}
