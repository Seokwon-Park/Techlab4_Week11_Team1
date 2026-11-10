#include "EnginePCH.h"
#include "AnimBlueprintEditorTab.h"
#include "AnimBlueprintEditor.h"

#include "Editor/SkeletalPreviewEditor/SkeletalPreviewViewportPanel.h"
#include "Panels/AnimBlueprintGraphPanel.h"
#include "Panels/AnimBlueprintDetailsPanel.h"

FAnimBlueprintEditorTab::FAnimBlueprintEditorTab(FAnimBlueprintEditor& InEditor)
    : FDockedAssetEditorTab(InEditor, "AnimBlueprintDockSpace", "AnimBlueprintPanelClass")
    , Editor(InEditor)
{
    RegisterPanel(MakeUnique<FSkeletalPreviewViewportPanel>(InEditor));
    RegisterPanel(MakeUnique<FAnimBlueprintGraphPanel>(InEditor));
    RegisterPanel(MakeUnique<FAnimBlueprintDetailsPanel>(InEditor));
}

void FAnimBlueprintEditorTab::BeforeRender()
{
    Editor.SetViewportState(0, 0, {});
}

void FAnimBlueprintEditorTab::BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size)
{
    ImGui::DockBuilderAddNode(InDockSpaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(InDockSpaceId, Size);

    ImGuiID GraphId = InDockSpaceId;

    const ImGuiID DetailsId = ImGui::DockBuilderSplitNode(GraphId, ImGuiDir_Right, 0.25f, nullptr, &GraphId);
    const ImGuiID ViewportId = ImGui::DockBuilderSplitNode(GraphId, ImGuiDir_Left, 0.30f, nullptr, &GraphId);

    ImGui::DockBuilderDockWindow(Panels[0]->GetWindowName(), ViewportId);
    ImGui::DockBuilderDockWindow(Panels[1]->GetWindowName(), GraphId);
    ImGui::DockBuilderDockWindow(Panels[2]->GetWindowName(), DetailsId);

    ImGui::DockBuilderFinish(InDockSpaceId);
}