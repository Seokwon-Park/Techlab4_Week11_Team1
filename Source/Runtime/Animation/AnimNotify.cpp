#include "EnginePCH.h"
#include "AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameplayStatics.h"
#include "Asset/AssetManager.h"
#include "Engine/Classes/Sound/SoundWave.h"

void UAnimNotify_PlaySound::Notify(USkeletalMeshComponent* MeshComp, const UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || SoundPath.empty())
		return;

	UWorld* World = MeshComp->GetWorld();
	if (!World)
		return;

	// AssetManager가 경로별로 캐시하므로 처음 한 번만 디코딩한다
	USoundWave* Sound = UAssetManager::Get().LoadSound(SoundPath);
	if (!Sound)
	{
		HTR_LOG(Warning, "AnimNotify_PlaySound: sound load failed: {}", SoundPath);
		return;
	}

	// UE UAnimNotify_PlaySound처럼 메시 위치에서 재생한다 (bFollow 없음)
	UGameplayStatics::PlaySoundAtLocation(World, Sound, MeshComp->GetWorldLocation(), Volume);
}
