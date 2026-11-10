#pragma once

#include "Core/EngineString.h"
#include "Math/MathUtility.h"
#include "Containers/Array.h"

enum class EAnimBlueprintGraphType : uint8
{
    Pose,
    StateMachine
};

enum class EAnimBlueprintNodeType : uint8
{
    OutputPose,
    SequencePlayer,
    BlendSpacePlayer,
    TwoWayBlend,
    StateMachine,
    Entry,
    State,
    StateResult
};

enum class EAnimBlueprintVariableType : uint8
{
    Float,
    Bool
};

struct FAnimBlueprintPosition
{
    float X = 0.0f;
    float Y = 0.0f;
};

struct FAnimBlueprintFloatInput
{
    bool bUseVariable = false;
    FString Variable;
    float Constant = 0.0f;
};

struct FAnimBlueprintNode
{
    int32 Id = INDEX_NONE;
    EAnimBlueprintNodeType Type = EAnimBlueprintNodeType::SequencePlayer;
    FString Name;
    FAnimBlueprintPosition Position;

    int32 InputPosePinId = INDEX_NONE;
    int32 SecondInputPosePinId = INDEX_NONE;
    int32 OutputPosePinId = INDEX_NONE;

    FString SequencePath;
    FString BlendSpacePath;
    FAnimBlueprintFloatInput FloatInput;
    bool bLooping = true;
    float PlayRate = 1.0f;

    int32 ChildGraphId = INDEX_NONE;
};

struct FAnimBlueprintLink
{
    int32 Id = INDEX_NONE;

    int32 FromNodeId = INDEX_NONE;
    int32 FromPinId = INDEX_NONE;

    int32 ToNodeId = INDEX_NONE;
    int32 ToPinId = INDEX_NONE;
};

struct FAnimBlueprintGraph
{
    int32 Id = INDEX_NONE;
    FString Name;
    EAnimBlueprintGraphType Type = EAnimBlueprintGraphType::Pose;

    TArray<FAnimBlueprintNode> Nodes;
    TArray<FAnimBlueprintLink> Links;
};

struct FAnimBlueprintVariable
{
    FString Name;
    EAnimBlueprintVariableType Type = EAnimBlueprintVariableType::Float;

    float DefaultFloat = 0.0f;
    bool bDefaultBool = false;
};

struct FAnimBlueprintData
{
    FString PreviewMeshPath;

    int32 RootGraphId = INDEX_NONE;
    int32 NextId = 1;

    TArray<FAnimBlueprintGraph> Graphs;
    TArray<FAnimBlueprintVariable> Variables;

    int32 AllocateId()
    {
        const int32 Id = NextId;
        ++NextId;
        return Id;
    }
};
