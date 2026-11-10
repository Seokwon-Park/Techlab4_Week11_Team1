#include "EnginePCH.h"
#include "AnimBlueprintDetailsPanel.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "Editor/AnimBlueprintEditor/AnimBlueprintEditor.h"
#include "Animation/AnimSequenceFile.h"
#include "Animation/BlendSpaceFile.h"
#include "Asset/AssetUtils.h"

FAnimBlueprintDetailsPanel::FAnimBlueprintDetailsPanel(FAnimBlueprintEditor& InEditor)
    : FAssetEditorPanel(InEditor, "Details")
    , Editor(InEditor)
{
}

void FAnimBlueprintDetailsPanel::DrawContent()
{
    FAnimBlueprintNode* Node = Editor.GetSelectedNode();

    if (Node)
    {
        ImGui::PushID(Node->Id);
        ImGui::Text("Node: %s", Node->Name.c_str());
        ImGui::Text("ID: %d", Node->Id);
        ImGui::Separator();

        switch (Node->Type)
        {
        case EAnimBlueprintNodeType::SequencePlayer:
            DrawAssetSelector(Node->SequencePath, false);
            break;

        case EAnimBlueprintNodeType::BlendSpacePlayer:
            DrawAssetSelector(Node->BlendSpacePath, true);
            DrawFloatInput("Blend Input X", Node->FloatInput, false);
            break;

        case EAnimBlueprintNodeType::TwoWayBlend:
            DrawFloatInput("Alpha", Node->FloatInput, true);
            break;

        default:
            ImGui::TextUnformatted("Connect a pose to this output.");
            break;
        }

        if (Node->Type == EAnimBlueprintNodeType::SequencePlayer || Node->Type == EAnimBlueprintNodeType::BlendSpacePlayer)
        {
            ImGui::Checkbox("Loop", &Node->bLooping);
            ImGui::DragFloat("Play Rate", &Node->PlayRate, 0.01f, 0.0f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        }

        ImGui::TextWrapped("Apply Preview updates node settings and restarts playback.");
        ImGui::PopID();
    }
    else
    {
        ImGui::TextUnformatted("Select a node.");
    }

    ImGui::Separator();
    DrawVariables();
    ImGui::Separator();
    DrawPreviewVariables();
}

void FAnimBlueprintDetailsPanel::DrawAssetSelector(FString& AssetPath, bool bBlendSpace)
{
    const FString SelectedName = AssetPath.empty() ? "None" : std::filesystem::path(AssetPath).filename().generic_string();
    const bool bOpen = ImGui::BeginCombo(bBlendSpace ? "BlendSpace" : "Sequence", SelectedName.c_str());

    if (ImGui::IsItemHovered() && !AssetPath.empty())
    {
        ImGui::SetTooltip("%s", AssetPath.c_str());
    }

    if (!bOpen)
    {
        return;
    }

    if (ImGui::IsWindowAppearing())
    {
        RefreshAssetList(bBlendSpace);
    }

    if (ImGui::Selectable("None", AssetPath.empty()))
    {
        AssetPath.clear();
    }

    for (const FString& Path : AssetPaths)
    {
        const FString Name = std::filesystem::path(Path).filename().generic_string();
        const bool bSelected = AssetPath == Path;
        ImGui::PushID(Path.c_str());

        if (ImGui::Selectable(Name.c_str(), bSelected))
        {
            AssetPath = Path;
        }

        if (bSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Path.c_str());
        }

        ImGui::PopID();
    }

    if (!AssetListMessage.empty())
    {
        ImGui::TextWrapped("%s", AssetListMessage.c_str());
    }
    else if (AssetPaths.empty())
    {
        ImGui::TextUnformatted(bBlendSpace ? "No .blendspace files in the preview mesh folder." : "No .anim.bin files in the preview mesh folder.");
    }

    ImGui::EndCombo();
}

void FAnimBlueprintDetailsPanel::RefreshAssetList(bool bBlendSpace)
{
    AssetPaths.clear();
    AssetListMessage.clear();

    const FString& PreviewMeshPath = Editor.GetBlueprintData().PreviewMeshPath;
    FString FilePath = FAssetUtils::ResolveAssetFilePath(PreviewMeshPath);

    if (FilePath.empty())
    {
        FilePath = PreviewMeshPath;
    }

    const std::filesystem::path Directory = std::filesystem::path(FilePath).parent_path();

    if (Directory.empty())
    {
        AssetListMessage = "Preview mesh folder is not available.";
        return;
    }

    std::error_code Error;
    std::filesystem::directory_iterator It(Directory, Error);
    const std::filesystem::directory_iterator End;

    while (!Error && It != End)
    {
        if (It->is_regular_file(Error))
        {
            const FString Path = It->path().generic_string();
            const bool bMatches = bBlendSpace ? BlendSpaceFile::IsBlendSpacePath(Path) : AnimSequenceFile::IsAnimSequencePath(Path);

            if (bMatches)
            {
                const FString AssetPath = FAssetUtils::GetAssetKey(Path);

                if (!AssetPath.empty())
                {
                    AssetPaths.push_back(AssetPath);
                }
            }
        }

        if (!Error)
        {
            It.increment(Error);
        }
    }

    std::sort(AssetPaths.begin(), AssetPaths.end());

    if (Error)
    {
        AssetListMessage = "Cannot read asset folder: " + Error.message();
    }
}

void FAnimBlueprintDetailsPanel::DrawFloatInput(const char* Label, FAnimBlueprintFloatInput& Input, bool bAlpha)
{
    ImGui::PushID(Label);
    ImGui::SeparatorText(Label);
    ImGui::Checkbox("Use Float Variable", &Input.bUseVariable);

    if (Input.bUseVariable)
    {
        const char* Selected = Input.Variable.empty() ? "Select Float variable" : Input.Variable.c_str();

        if (ImGui::BeginCombo("Variable", Selected))
        {
            for (const FAnimBlueprintVariable& Variable : Editor.GetBlueprintData().Variables)
            {
                if (Variable.Type != EAnimBlueprintVariableType::Float)
                {
                    continue;
                }

                if (ImGui::Selectable(Variable.Name.c_str(), Input.Variable == Variable.Name))
                {
                    Input.Variable = Variable.Name;
                }
            }

            ImGui::EndCombo();
        }
    }
    else if (bAlpha)
    {
        ImGui::SliderFloat("Constant", &Input.Constant, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    }
    else
    {
        ImGui::DragFloat("Constant", &Input.Constant, 1.0f);
    }

    ImGui::PopID();
}

void FAnimBlueprintDetailsPanel::DrawVariables()
{
    if (!ImGui::CollapsingHeader("Variable Defaults", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::InputText("Name", NewVariableName, sizeof(NewVariableName));
    ImGui::Combo("Type", &NewVariableType, "Float\0Bool\0");

    if (ImGui::Button("Add Variable"))
    {
        const EAnimBlueprintVariableType Type = NewVariableType == 0 ? EAnimBlueprintVariableType::Float : EAnimBlueprintVariableType::Bool;

        if (Editor.AddVariable(NewVariableName, Type))
        {
            NewVariableName[0] = '\0';
        }
    }

    FString RemoveName;
    bool bOpenRename = false;

    for (FAnimBlueprintVariable& Variable : Editor.GetVariables())
    {
        ImGui::PushID(Variable.Name.c_str());

        if (ImGui::TreeNode("Variable", "%s (%s)", Variable.Name.c_str(), Variable.Type == EAnimBlueprintVariableType::Float ? "Float" : "Bool"))
        {
            if (Variable.Type == EAnimBlueprintVariableType::Float)
            {
                ImGui::DragFloat("Default", &Variable.DefaultFloat, 0.1f);
            }
            else
            {
                ImGui::Checkbox("Default", &Variable.bDefaultBool);
            }

            if (ImGui::SmallButton("Rename"))
            {
                RenamingVariable = Variable.Name;
                std::snprintf(RenameBuffer, sizeof(RenameBuffer), "%s", Variable.Name.c_str());
                bOpenRename = true;
            }

            ImGui::SameLine();

            if (ImGui::SmallButton("Remove"))
            {
                RemoveName = Variable.Name;
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    if (!RemoveName.empty())
    {
        Editor.RemoveVariable(RemoveName);
    }

    if (bOpenRename)
    {
        ImGui::OpenPopup("Rename Variable");
    }

    if (ImGui::BeginPopup("Rename Variable"))
    {
        ImGui::InputText("New Name", RenameBuffer, sizeof(RenameBuffer));

        if (ImGui::Button("Rename"))
        {
            if (Editor.RenameVariable(RenamingVariable, RenameBuffer))
            {
                ImGui::CloseCurrentPopup();
            }
        }

        const FString& Message = Editor.GetVariableMessage();

        if (!Message.empty())
        {
            ImGui::TextWrapped("%s", Message.c_str());
        }

        ImGui::EndPopup();
    }

    const FString& Message = Editor.GetVariableMessage();

    if (!Message.empty())
    {
        ImGui::TextWrapped("%s", Message.c_str());
    }

    ImGui::TextWrapped("Defaults and bindings take effect when Apply Preview is pressed.");
}

void FAnimBlueprintDetailsPanel::DrawPreviewVariables()
{
    if (!ImGui::CollapsingHeader("Preview Values", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    const TArray<FAnimBlueprintVariable>& Variables = Editor.GetAppliedVariables();

    if (Variables.IsEmpty())
    {
        ImGui::TextUnformatted("Apply Preview to edit runtime values.");
        return;
    }

    ImGui::TextWrapped("These values update playback immediately and do not change defaults.");

    for (const FAnimBlueprintVariable& Variable : Variables)
    {
        ImGui::PushID(Variable.Name.c_str());

        if (Variable.Type == EAnimBlueprintVariableType::Float)
        {
            float Value = Editor.GetPreviewFloat(Variable.Name);

            if (ImGui::DragFloat(Variable.Name.c_str(), &Value, 0.1f))
            {
                Editor.SetPreviewFloat(Variable.Name, Value);
            }
        }
        else
        {
            bool bValue = Editor.GetPreviewBool(Variable.Name);

            if (ImGui::Checkbox(Variable.Name.c_str(), &bValue))
            {
                Editor.SetPreviewBool(Variable.Name, bValue);
            }
        }

        ImGui::PopID();
    }
}
