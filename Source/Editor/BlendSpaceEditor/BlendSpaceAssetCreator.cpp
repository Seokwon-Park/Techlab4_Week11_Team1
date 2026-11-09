#include "EnginePCH.h"
#include "BlendSpaceAssetCreator.h"

#include <memory>

#include "Animation/BlendSpace1D.h"
#include "Animation/BlendSpaceFile.h"

#include "Asset/AssetManager.h"
#include "Render/SkeletalMesh.h"
#include "imgui.h"

void FBlendSpaceAssetCreator::DrawContextMenu(const FString& AssetPath, const std::filesystem::path& Directory)
{
    URenderAsset* Loaded = UAssetManager::FindLoadedAsset(AssetPath);
    const bool bSkeletalMesh = (Loaded && Loaded->IsA<USkeletalMesh>()) ||
        UAssetManager::GetUnloadedAssetType(AssetPath) == EAssetType::SkeletalMesh;

    if (!bSkeletalMesh)
    {
        return;
    }

    if (ImGui::BeginPopupContextItem("##BlendSpaceContext"))
    {
        if (ImGui::MenuItem("Create BlendSpace 1D"))
        {
            PreviewMeshPath = AssetPath;
            CreateDirectory = Directory;
            strcpy_s(AssetName, "NewBlendSpace");
            ErrorMessage.clear();
            bOpenDialog = true;
        }
        ImGui::EndPopup();
    }
}

void FBlendSpaceAssetCreator::DrawCreateDialog()
{
    if (bOpenDialog)
    {
        ImGui::OpenPopup("Create BlendSpace 1D");
        bOpenDialog = false;
    }

    if (ImGui::BeginPopupModal("Create BlendSpace 1D", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("Preview mesh: %s", PreviewMeshPath.c_str());
        if (ImGui::InputText("Name", AssetName, sizeof(AssetName)))
        {
            ErrorMessage.clear();
        }

        if (!ErrorMessage.empty())
        {
            ImGui::TextWrapped("%s", ErrorMessage.c_str());
        }

        ImGui::BeginDisabled(AssetName[0] == '\0');

        if (ImGui::Button("Create"))
        {
            if (CreateAsset())
            {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

bool FBlendSpaceAssetCreator::CreateAsset()
{
    ErrorMessage.clear();
    const FString Name = AssetName;
    const std::filesystem::path Target = CreateDirectory / (Name + ".blendspace");

    std::error_code Error;
    const bool bExists = std::filesystem::exists(Target, Error);
    if (Error || bExists)
    {
        ErrorMessage = Error ? Error.message() : "An asset with this name already exists.";
        return false;
    }

    auto BlendSpace = std::unique_ptr<UBlendSpace1D>(NewObject<UBlendSpace1D>());
    BlendSpace->SetPreviewMeshPath(PreviewMeshPath);

    if (!BlendSpaceFile::Write(Target, *BlendSpace, ErrorMessage))
    {
        return false;
    }

    if (OnCreated)
    {
        OnCreated(Target.generic_string());
    }
    return true;
}