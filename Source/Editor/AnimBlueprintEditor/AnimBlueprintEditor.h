#pragma once

#include "Editor/SkeletalPreviewEditor/SkeletalPreviewEditor.h"
#include "Animation/AnimBlueprintData.h"
#include "Containers/Map.h"

class UAnimGraphInstance;
class UAnimSequence;

class FAnimBlueprintEditor : public FSkeletalPreviewEditor
{
public:
    FAnimBlueprintEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager, const FString& InPreviewMeshPath);
    ~FAnimBlueprintEditor() override;

    const char* GetEditorTypeName() const override { return "AnimBlueprint"; }

    FAnimBlueprintGraph* GetCurrentGraph();
    FAnimBlueprintNode* GetSelectedNode();
    FAnimBlueprintGraph* FindGraph(int32 GraphId);
    FAnimBlueprintNode* FindNode(int32 NodeId);
    FAnimBlueprintTransition* GetSelectedTransition();
    TArray<int32> GetGraphPath() const;
    bool NavigateToGraph(int32 GraphId);
    bool OpenNodeGraph(int32 NodeId);
    bool RenameNode(int32 NodeId, const FString& Name);
    void SelectLink(int32 LinkId);

    const FAnimBlueprintData& GetBlueprintData() const { return BlueprintData; }

    void SelectNode(int32 NodeId);
    int32 AddSequencePlayer(const FAnimBlueprintPosition& Position);
    int32 AddPoseNode(EAnimBlueprintNodeType Type, const FAnimBlueprintPosition& Position);
    int32 AddState(const FAnimBlueprintPosition& Position);
    int32 AddAnyState(const FAnimBlueprintPosition& Position);
    bool IsPreviewStateActive(int32 NodeId) const;
    bool CanConnectPins(int32 FirstPinId, int32 SecondPinId);
    bool ConnectPins(int32 FirstPinId, int32 SecondPinId);

    TArray<FAnimBlueprintVariable>& GetVariables() { return BlueprintData.Variables;  }

    bool AddVariable(const FString& Name, EAnimBlueprintVariableType Type);
    bool RenameVariable(const FString& PreviousName, const FString& Name);
    void RemoveVariable(const FString& Name);

    const FString& GetVariableMessage() const { return VariableMessage; }
    const TArray<FAnimBlueprintVariable>& GetAppliedVariables() const { return AppliedVariables; }

    float GetPreviewFloat(const FString& Name) const;
    bool GetPreviewBool(const FString& Name) const;
    void SetPreviewFloat(const FString& Name, float Value);
    void SetPreviewBool(const FString& Name, bool bValue);

    bool CanConnectPosePins(int32 FirstPinId, int32 SecondPinId);
    bool ConnectPosePins(int32 FirstPinId, int32 SecondPinId);

    bool CanRemoveNode(int32 NodeId);
    bool RemoveNode(int32 NodeId);
    bool RemoveLink(int32 LinkId);

    void RequestApplyPreview();

    const FString& GetPreviewMessage() const
    {
        return PreviewMessage;
    }

protected:
    bool InitializeResources() override;
    void ShutdownResources() override;
    void TickBeforeWorld(float DeltaTime) override;
    TUniquePtr<IEditorTab> CreateTab() override;

private:
    void CreateInitialGraph();
    FAnimBlueprintNode* FindNodeByPin(int32 PinId);
    bool ResolvePoseConnection(int32 FirstPinId, int32 SecondPinId, FAnimBlueprintLink& OutConnection);
    bool ResolveStateConnection(int32 FirstPinId, int32 SecondPinId, FAnimBlueprintLink& OutConnection);

    int32 CreateChildGraph(int32 ParentGraphId, int32 OwnerNodeId, const FString& Name, EAnimBlueprintGraphType Type);
    void RemoveChildGraphs(int32 GraphId);

    bool HasPosePath(int32 FromNodeId, int32 ToNodeId);

    bool ValidateVariableName(const FString& Name, const FString& PreviousName);
    bool ValidateFloatInput(const FAnimBlueprintNode& Node);
    bool ValidateSequence(UAnimSequence* Sequence, int32 NodeId);

    int32 BuildPreviewNode(const FAnimBlueprintGraph& Graph, int32 NodeId, UAnimGraphInstance& Instance, TArray<int32>& Visiting, TMap<int32, TArray<int32>>& NodeIndices);
    int32 BuildPreviewInput(const FAnimBlueprintGraph& Graph, int32 NodeId, int32 PinId, UAnimGraphInstance& Instance, TArray<int32>& Visiting, TMap<int32, TArray<int32>>& NodeIndices);
    int32 BuildPreviewPoseGraph(const FAnimBlueprintGraph& Graph, EAnimBlueprintNodeType ResultType, UAnimGraphInstance& Instance, TArray<int32>& Visiting, TMap<int32, TArray<int32>>& NodeIndices);
    int32 BuildPreviewStateMachine(const FAnimBlueprintGraph& ParentGraph, const FAnimBlueprintNode& MachineNode, UAnimGraphInstance& Instance, TArray<int32>& Visiting, TMap<int32, TArray<int32>>& NodeIndices);
    bool BuildPreviewTransitions(const FAnimBlueprintGraph& Graph, int32 MachineIndex, UAnimGraphInstance& Instance, const TMap<int32, int32>& StateIndices);
    
    bool ApplyPreview();
    bool SetPreviewError(int32 NodeId, const FString& Message);

    FAnimBlueprintData BlueprintData;

    int32 CurrentGraphId = INDEX_NONE;
    int32 SelectedNodeId = INDEX_NONE;
    int32 SelectedLinkId = INDEX_NONE;

    TUniquePtr<UAnimGraphInstance> PreviewInstance;
    TMap<int32, TArray<int32>> PreviewNodeIndices;
    TArray<FAnimBlueprintVariable> AppliedVariables;
    FString VariableMessage;
    FString PreviewMessage;
    bool bApplyPreviewRequested = false;
};
