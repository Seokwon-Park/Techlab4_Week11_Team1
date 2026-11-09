#pragma once

#include "Core/EngineString.h"

#include <filesystem>
#include <functional>

class FBlendSpaceAssetCreator
{
public:
    void DrawContextMenu(const FString& AssetPath, const std::filesystem::path& Directory);
    void DrawCreateDialog();
    void SetCreatedCallback(std::function<void(const FString&)> InCallback)
    {
        OnCreated = std::move(InCallback);
    }

private:
    bool CreateAsset();

    std::function<void(const FString&)> OnCreated;

    FString PreviewMeshPath;
    std::filesystem::path CreateDirectory;
    FString ErrorMessage;

    char AssetName[64] = "NewBlendSpace";
    bool bOpenDialog = false;
};