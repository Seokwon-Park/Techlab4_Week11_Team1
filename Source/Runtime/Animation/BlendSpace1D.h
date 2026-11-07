#pragma once

#include "UObject/Object.h"
#include "AnimSequence.h"
#include "BlendSpace1D.generated.h"

struct FBlendSample
{
	UAnimSequence* Animation = nullptr;
	FVector SampleValue = FVector::ZeroVector;
};

UCLASS()
class UBlendSpace1D : public UObject
{
	GENERATED_BODY()

public:
	bool AddSample(UAnimSequence* Animation, const FVector& SampleValue);
	bool GetSamples(float X, FBlendSample& OutA, FBlendSample& OutB, float& OutAlpha) const;

	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

private:
	TArray<FBlendSample> BlendSampleData;
};