#include "EnginePCH.h"
#include "AnimBlueprintDetailsPanel.h"
#include "Editor/AnimBlueprintEditor/AnimBlueprintEditor.h"

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
        ImGui::Text("Node: %s", Node->Name.c_str());
        ImGui::Text("ID: %d", Node->Id);
        ImGui::Text("Position: %.1f, %.1f", Node->Position.X, Node->Position.Y);
    }
    else
    {
        ImGui::TextUnformatted("Select a node.");
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Variables", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const FAnimBlueprintData& Data = Editor.GetBlueprintData();

        for (const FAnimBlueprintVariable& Variable : Data.Variables)
        {
            if (Variable.Type == EAnimBlueprintVariableType::Float)
            {
                ImGui::Text("%s: %.2f", Variable.Name.c_str(), Variable.DefaultFloat);
            }
            else
            {
                ImGui::Text("%s: %s", Variable.Name.c_str(), Variable.bDefaultBool ? "true" : "false");
            }
        }
    }
}