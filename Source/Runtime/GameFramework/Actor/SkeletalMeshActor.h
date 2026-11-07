#pragma once

#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkeletalMeshActor.generated.h"

UCLASS()
class ASkeletalMeshActor : public AActor
{
	GENERATED_BODY()

public:
	ASkeletalMeshActor();

	void BeginPlay() override;
	
	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }
private:
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
};

