#pragma once

#include "Editor/SkeletalPreviewEditor/SkeletalPreviewEditor.h"
#include "Animation/AnimBlueprintData.h"

class FAnimBlueprintEditor : public FSkeletalPreviewEditor
{
public:
    FAnimBlueprintEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager, const FString& InPreviewMeshPath);

    const char* GetEditorTypeName() const override { return "AnimBlueprint"; }

    FAnimBlueprintGraph* GetCurrentGraph();
    FAnimBlueprintNode* GetSelectedNode();

    const FAnimBlueprintData& GetBlueprintData() const
    {
        return BlueprintData;
    }

    void SelectNode(int32 NodeId);

protected:
    bool InitializeResources() override;
    TUniquePtr<IEditorTab> CreateTab() override;

private:
    void CreateInitialGraph();

    FAnimBlueprintData BlueprintData;

    int32 CurrentGraphId = INDEX_NONE;
    int32 SelectedNodeId = INDEX_NONE;
};