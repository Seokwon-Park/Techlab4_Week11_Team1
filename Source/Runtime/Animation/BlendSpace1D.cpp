#include "EnginePCH.h"
#include "BlendSpace1D.h"

bool UBlendSpace1D::AddSample(UTestAnimSequence* Animation, const FVector& SampleValue)
{
	if (!Animation)
	{
		return false;
	}

	int32 InsertIndex = 0;
	while (InsertIndex < BlendSampleData.Num() && BlendSampleData[InsertIndex].SampleValue.X < SampleValue.X)
	{
		++InsertIndex;
	}

	if (InsertIndex < BlendSampleData.Num() && BlendSampleData[InsertIndex].SampleValue.X == SampleValue.X)
	{
		return false;
	}

	FBlendSample Sample;
	Sample.Animation = Animation;
	Sample.SampleValue = SampleValue;

	BlendSampleData.Insert(Sample, InsertIndex);
	return true;
}

bool UBlendSpace1D::GetSamples(float X, FBlendSample& OutA, FBlendSample& OutB, float& OutAlpha) const
{
	OutA = FBlendSample();
	OutB = FBlendSample();
	OutAlpha = 0.0f;

	const int32 NumSamples = BlendSampleData.Num();
	if (NumSamples == 0)
	{
		return false;
	}

	// 최소값 이하에서는 첫 샘플만 사용
	if (X <= BlendSampleData[0].SampleValue.X)
	{
		OutA = BlendSampleData[0];
		OutB = OutA;
		return true;
	}

	// 최댓값 이상에서는 마지막 샘플만 사용
	if (X >= BlendSampleData[NumSamples - 1].SampleValue.X)
	{
		OutA = BlendSampleData[NumSamples - 1];
		OutB = OutA;
		return true;
	}

	// X를 사이에 두는 두 샘플 찾기
	for (int32 Index = 0; Index < NumSamples - 1; ++Index)
	{
		const FBlendSample& Left = BlendSampleData[Index];
		const FBlendSample& Right = BlendSampleData[Index + 1];

		if (X <= Right.SampleValue.X)
		{
			OutA = Left;
			OutB = Right;

			OutAlpha = (X - Left.SampleValue.X) / (Right.SampleValue.X - Left.SampleValue.X);

			return true;
		}
	}

	return false;
}