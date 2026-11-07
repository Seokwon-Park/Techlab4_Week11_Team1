#include "EnginePCH.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceFile.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

namespace
{
	constexpr uint32 AnimSequenceMagic = 0x4D494E41; // ANIM
	constexpr uint32 AnimSequenceVersion = 1;
	constexpr int64 BytesPerBoneKey = 10 * sizeof(float);

	void SerializeName(FArchive& Ar, FString& Name)
	{
		int32 Size = static_cast<int32>(Name.size());
		Ar << Size;
		if (Ar.IsLoading())
		{
			if (Ar.IsError() || Size < 0 || Size > Ar.TotalSize() - Ar.Tell())
			{
				Ar.SetError();
				return;
			}
			Name.resize(Size);
		}
		if (Size > 0) Ar.Serialize(Name.data(), Size);
	}

	template <typename T>
	void SerializeKeys(FArchive& Ar, TArray<T>& Keys)
	{
		for (T& Key : Keys)
		{
			Ar << Key.X << Key.Y << Key.Z;
			if constexpr (std::is_same_v<T, FQuat>) Ar << Key.W;
		}
	}
}

void UAnimSequence::Serialize(FArchive& Ar)
{
	const bool bLoading = Ar.IsLoading();
	const UAnimDataModel* Model = GetDataModel();
	if (!bLoading && !Model)
	{
		Ar.SetError();
		return;
	}

	uint32 Magic = AnimSequenceMagic;
	uint32 Version = AnimSequenceVersion;
	Ar << Magic << Version;
	if (Ar.IsError() || Magic != AnimSequenceMagic || Version != AnimSequenceVersion)
	{
		Ar.SetError();
		return;
	}

	FString Name = bLoading ? FString{} : GetClipName();
	FFrameRate Rate = bLoading ? FFrameRate{0, 1} : Model->GetFrameRate();
	float Length = bLoading ? 0.0f : Model->GetPlayLength();
	int32 TrackCount = bLoading ? 0 : Model->GetBoneAnimationTracks().Num();
	int32 KeyCount = bLoading ? 0 : Model->GetNumberOfKeys();
	SerializeName(Ar, Name);
	Ar << Rate.Numerator << Rate.Denominator << Length << TrackCount << KeyCount;
	if (Ar.IsError() || Rate.Numerator <= 0 || Rate.Denominator <= 0
		|| !std::isfinite(Length) || Length < 0.0f || TrackCount <= 0 || KeyCount <= 0
		|| (bLoading && TrackCount > (Ar.TotalSize() - Ar.Tell()) / (sizeof(int32) + KeyCount * BytesPerBoneKey)))
	{
		Ar.SetError();
		return;
	}

	TArray<FBoneAnimationTrack> LoadedTracks;
	if (bLoading) LoadedTracks.SetNum(TrackCount);
	for (int32 Index = 0; Index < TrackCount && !Ar.IsError(); ++Index)
	{
		if (bLoading)
		{
			auto& Track = LoadedTracks[Index];
			SerializeName(Ar, Track.Name);
			if (Ar.IsError() || KeyCount * BytesPerBoneKey > Ar.TotalSize() - Ar.Tell())
			{
				Ar.SetError();
				return;
			}
			auto& Keys = Track.InternalTrack;
			Keys.PosKeys.SetNum(KeyCount);
			Keys.RotKeys.SetNum(KeyCount);
			Keys.ScaleKeys.SetNum(KeyCount);
			SerializeKeys(Ar, Keys.PosKeys);
			SerializeKeys(Ar, Keys.RotKeys);
			SerializeKeys(Ar, Keys.ScaleKeys);
		}
		else
		{
			auto Track = Model->GetBoneAnimationTracks()[Index];
			auto& Keys = Track.InternalTrack;
			if (Keys.PosKeys.Num() != KeyCount || Keys.RotKeys.Num() != KeyCount || Keys.ScaleKeys.Num() != KeyCount)
			{
				Ar.SetError();
				return;
			}
			SerializeName(Ar, Track.Name);
			SerializeKeys(Ar, Keys.PosKeys);
			SerializeKeys(Ar, Keys.RotKeys);
			SerializeKeys(Ar, Keys.ScaleKeys);
		}
	}

	if (bLoading && !Ar.IsError())
	{
		if (Ar.Tell() != Ar.TotalSize())
		{
			Ar.SetError();
			return;
		}
		Initialize(std::move(Name), std::move(LoadedTracks), Rate, Length);
	}
}

bool AnimSequenceFile::Read(const std::filesystem::path& Path, UAnimSequence& Sequence, FString& OutError)
{
	OutError.clear();
	std::ifstream File(Path, std::ios::binary | std::ios::ate);
	const auto Size = static_cast<std::streamoff>(File.tellg());
	if (Size <= 0 || Size > (std::numeric_limits<int32>::max)())
	{
		OutError = "Animation binary: cannot open file or invalid size";
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.SetNum(static_cast<int32>(Size));
	File.seekg(0);
	if (!File.read(reinterpret_cast<char*>(Bytes.GetData()), Size))
	{
		OutError = "Animation binary: cannot read file";
		return false;
	}
	FMemoryReader Reader(Bytes, true);
	Sequence.Serialize(Reader);
	if (!Reader.IsError()) return true;
	OutError = "Animation binary: invalid or unsupported sequence data";
	return false;
}

bool AnimSequenceFile::Write(const std::filesystem::path& Path, UAnimSequence& Sequence, FString& OutError)
{
	OutError.clear();
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	Sequence.Serialize(Writer);
	if (Writer.IsError())
	{
		OutError = "Animation binary: invalid sequence data";
		return false;
	}
	std::ofstream File(Path, std::ios::binary | std::ios::trunc);
	File.write(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());
	File.close();
	if (File) return true;
	OutError = "Animation binary: cannot write file";
	return false;
}

void UAnimSequence::GetAnimationPose(float Time, FCompactPose& OutPose, const TArray<FBoneInfo>& RefSkeleton) const
{         
    OutPose.ResetToRefPose(RefSkeleton);
    const UAnimDataModel* Model = GetDataModel();
	if (!Model)
	{
		return;
	}

    const FFrameRate Rate = Model->GetFrameRate();
    const float Length = Model->GetPlayLength();
	if (!std::isfinite(Time) || !std::isfinite(Length) || Length < 0.0f)
	{
		return;
	}

    const double Frame = static_cast<double>(FMath::Clamp(Time, 0.0f, Length)) * Rate.AsDecimal();

    for (const FBoneAnimationTrack& Track : Model->GetBoneAnimationTracks())
    {
        int32 BoneIndex = -1;
        for (int32 Index = 0; Index < RefSkeleton.Num(); ++Index)
        {
            if (RefSkeleton[Index].Name == Track.Name)
            {
                BoneIndex = Index;
                break;
            }
        }
		if (BoneIndex < 0)
		{
			continue;
		}

        auto Sample = [Frame](const auto& Keys, auto& Value, auto Interpolate)
        {
			if (Keys.IsEmpty())
			{
				return;
			}
			const double KeyFrame = std::clamp(Frame, 0.0, static_cast<double>(Keys.Num() - 1));

            const int32 A = static_cast<int32>(KeyFrame);
            const int32 B = std::min(A + 1, Keys.Num() - 1);

            Value = Interpolate(Keys[A], Keys[B], static_cast<float>(KeyFrame - A));
        };

        FTransform& Bone = OutPose[BoneIndex];
        const auto& Keys = Track.InternalTrack;

        auto Lerp = [](const FVector& A, const FVector& B, float Alpha) 
		{ 
			return FMath::Lerp(A, B, Alpha); 
		};

        Sample(Keys.PosKeys, Bone.Location, Lerp);
        Sample(Keys.ScaleKeys, Bone.Scale, Lerp);
        Sample(Keys.RotKeys, Bone.Rotation, [](const FQuat& A, const FQuat& B, float Alpha)
        {
            return FMath::Slerp(A, B, Alpha).Normalized();
        });
    }
}
