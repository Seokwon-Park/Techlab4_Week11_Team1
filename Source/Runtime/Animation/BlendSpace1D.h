#pragma once

#include "UObject/Object.h"
#include "TestAnimSequence.h"
#include "BlendSpace1D.generated.h"

struct FBlendSample
{
	UTestAnimSequence* Animation = nullptr;
	FVector SampleValue = FVector::ZeroVector;
};

UCLASS()
class UBlendSpace1D : public UObject
{
	GENERATED_BODY()

public:
	bool AddSample(UTestAnimSequence* Animation, const FVector& SampleValue);
	bool GetSamples(float X, FBlendSample& OutA, FBlendSample& OutB, float& OutAlpha) const;

	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

private:
	TArray<FBlendSample> BlendSampleData;
};