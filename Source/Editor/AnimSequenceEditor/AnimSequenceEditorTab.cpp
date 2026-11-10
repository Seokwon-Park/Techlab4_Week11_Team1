#include "EnginePCH.h"
#include "AnimSequenceEditorTab.h"
#include "AnimSequenceEditor.h"
#include "Editor/SkeletalPreviewEditor/SkeletalPreviewViewportPanel.h"
#include "Panels/AnimTimelinePanel.h"
#include "Panels/AnimSequenceDetailsPanel.h"

FAnimSequenceEditorTab::FAnimSequenceEditorTab(FAnimSequenceEditor& InEditor)
    : FDockedAssetEditorTab(InEditor, "AnimSequenceDockSpace", "AnimSequencePanelClass")
    , AnimEditor(InEditor)
{
    RegisterPanel(MakeUnique<FSkeletalPreviewViewportPanel>(InEditor));
    RegisterPanel(MakeUnique<FAnimTimelinePanel>(InEditor));
    RegisterPanel(MakeUnique<FAnimSequenceDetailsPanel>(InEditor));
}

void FAnimSequenceEditorTab::BeforeRender()
{
    // 패널이 그려지지 않은 프레임에는 프리뷰 입력을 비운다.
    AnimEditor.SetViewportState(0, 0, {});
}

void FAnimSequenceEditorTab::BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size)
{
    ImGui::DockBuilderAddNode(InDockSpaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(InDockSpaceId, Size);

    ImGuiID ViewportId = InDockSpaceId;
    const ImGuiID DetailsId = ImGui::DockBuilderSplitNode(ViewportId, ImGuiDir_Right, 0.24f, nullptr, &ViewportId);
    const ImGuiID TimelineId = ImGui::DockBuilderSplitNode(ViewportId, ImGuiDir_Down, 0.25f, nullptr, &ViewportId);

    ImGui::DockBuilderDockWindow(Panels[0]->GetWindowName(), ViewportId);
    ImGui::DockBuilderDockWindow(Panels[1]->GetWindowName(), TimelineId);
    ImGui::DockBuilderDockWindow(Panels[2]->GetWindowName(), DetailsId);
    ImGui::DockBuilderFinish(InDockSpaceId);
}
