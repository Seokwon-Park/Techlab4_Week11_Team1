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
    StateResult,
    AnyState
};

enum class EAnimBlueprintVariableType : uint8
{
    Float,
    Bool
};

enum class EAnimBlueprintTransitionRule : uint8
{
    Disabled,
    Bool,
    Float,
    Finished
};

enum class EAnimBlueprintCompare : uint8
{
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual
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
    int32 TransitionInputPinId = INDEX_NONE;
    int32 TransitionOutputPinId = INDEX_NONE;

    FString SequencePath;
    FString BlendSpacePath;
    FAnimBlueprintFloatInput FloatInput;
    bool bLooping = true;
    bool bResetOnEntry = true;
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

struct FAnimBlueprintTransition
{
    int32 Id = INDEX_NONE;
    int32 FromStateId = INDEX_NONE;
    int32 ToStateId = INDEX_NONE;
    int32 FromPinId = INDEX_NONE;
    int32 ToPinId = INDEX_NONE;
    EAnimBlueprintTransitionRule Rule = EAnimBlueprintTransitionRule::Disabled;
    FString Variable;
    bool bExpected = true;
    EAnimBlueprintCompare Compare = EAnimBlueprintCompare::Greater;
    float Value = 0.0f;
    float CrossfadeDuration = 0.2f;
    int32 Priority = 0;
};

struct FAnimBlueprintGraph
{
    int32 Id = INDEX_NONE;
    FString Name;
    EAnimBlueprintGraphType Type = EAnimBlueprintGraphType::Pose;
    int32 ParentGraphId = INDEX_NONE;
    int32 OwnerNodeId = INDEX_NONE;

    TArray<FAnimBlueprintNode> Nodes;
    TArray<FAnimBlueprintLink> Links;
    TArray<FAnimBlueprintTransition> Transitions;
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
