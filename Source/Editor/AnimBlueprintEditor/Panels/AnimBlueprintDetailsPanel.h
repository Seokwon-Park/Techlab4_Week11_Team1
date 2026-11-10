#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"
#include <vector>

class FAnimBlueprintEditor;
struct FAnimBlueprintNode;
struct FAnimBlueprintFloatInput;

class FAnimBlueprintDetailsPanel : public FAssetEditorPanel
{
public:
    explicit FAnimBlueprintDetailsPanel(FAnimBlueprintEditor& InEditor);

protected:
    void DrawContent() override;

private:
    void DrawAssetSelector(FString& AssetPath, bool bBlendSpace);
    void RefreshAssetList(bool bBlendSpace);
    void DrawFloatInput(const char* Label, FAnimBlueprintFloatInput& Input, bool bAlpha);
    void DrawVariables();
    void DrawPreviewVariables();

    FAnimBlueprintEditor& Editor;

    std::vector<FString> AssetPaths;
    FString AssetListMessage;
    char NewVariableName[128] = {};
    int NewVariableType = 0;
    FString RenamingVariable;
    char RenameBuffer[128] = {};
};
