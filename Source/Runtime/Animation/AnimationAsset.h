#pragma once

#include "UObject/Object.h"
#include "AnimationAsset.generated.h"

UCLASS()
class UAnimationAsset : public UObject
{
    GENERATED_BODY()

public:
    void SetPreviewMeshPath(FString InPath) { PreviewMeshPath = std::move(InPath); }
    const FString& GetPreviewMeshPath() const { return PreviewMeshPath; }
    bool IsAnimSequencePath(const FString& Path);
private:
    FString PreviewMeshPath;
};