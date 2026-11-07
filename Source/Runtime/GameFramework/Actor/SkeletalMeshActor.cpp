#include "EnginePCH.h"
#include "SkeletalMeshActor.h"
#include "Render/SkeletalMesh.h"
#include "Animation/AnimSingleNodeInstance.h"

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

ASkeletalMeshActor::ASkeletalMeshActor()
{
    SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("USkeletalMeshComponent");
    SkeletalMeshComponent->SetupAttachment(GetRootComponent());
    SkeletalMeshComponent->SetSkeletalMesh(GetTestBoneStickMesh());

    auto* Idle = NewObject<UTestAnimSequence>(SkeletalMeshComponent);
    auto* Walk = NewObject<UTestAnimSequence>(SkeletalMeshComponent);
    auto* Run = NewObject<UTestAnimSequence>(SkeletalMeshComponent);
    Idle->MaxOffset = FVector(0, 0, 0);
    Walk->MaxOffset = FVector(0, 1, 0);
    Run->MaxOffset = FVector(0, 0, 1);

    auto* BlendSpace = NewObject<UBlendSpace1D>(SkeletalMeshComponent);
    BlendSpace->AddSample(Idle, FVector(0, 0, 0));
    BlendSpace->AddSample(Walk, FVector(200, 0, 0));
    BlendSpace->AddSample(Run, FVector(600, 0, 0));
    SkeletalMeshComponent->PlayBlendSpace(BlendSpace);
}