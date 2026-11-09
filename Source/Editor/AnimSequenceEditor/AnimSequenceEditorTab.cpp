#include "EnginePCH.h"
#include "AnimSequenceEditorTab.h"
#include "AnimSequenceEditor.h"
#include "Panels/AnimViewportPanel.h"
#include "Panels/AnimTimelinePanel.h"
#include "Panels/AnimSequenceDetailsPanel.h"

FAnimSequenceEditorTab::FAnimSequenceEditorTab(FAnimSequenceEditor& InEditor)
    : FAssetEditorTab(InEditor)
    , AnimEditor(InEditor)
{
    // 같은 시퀀스를 다시 열어도 배치가 유지되고, 다른 에셋 창과 섞이지 않도록 에셋 경로로 ID를 만든다
    const ImGuiID AssetId = ImHashStr(InEditor.GetAssetPath().c_str());
    DockSpaceId = ImHashStr("AnimSequenceDockSpace", 0, AssetId);
    PanelClassId = ImHashStr("AnimSequencePanelClass", 0, AssetId);

    Panels.Add(MakeUnique<FAnimViewportPanel>(InEditor));         // 0
    Panels.Add(MakeUnique<FAnimTimelinePanel>(InEditor));         // 1
    Panels.Add(MakeUnique<FAnimSequenceDetailsPanel>(InEditor));  // 2
    for (auto& Panel : Panels)
    {
        Panel->SetDockClass(PanelClassId);
        Panel->Init();
    }
}

void FAnimSequenceEditorTab::DrawMenuBar()
{
    if (ImGui::BeginMenu("Viewer"))
    {
        if (ImGui::MenuItem("Close Viewer"))
        {
            AnimEditor.Close();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        for (auto& Panel : Panels)
        {
            if (ImGui::MenuItem(Panel->GetPanelName(), nullptr, Panel->IsOpen()))
            {
                Panel->SetOpen(!Panel->IsOpen());
            }
        }
        ImGui::EndMenu();
    }
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

void FAnimSequenceEditorTab::OnRender(bool bActive)
{
    // 뷰포트 패널이 그리지 않은 프레임은 보이지 않는 것으로 처리한다
    AnimEditor.SetViewportState(0, 0, {});
    if (AnimEditor.IsCloseRequested())
    {
        return;
    }

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        const ImVec2 Size = ImGui::GetContentRegionAvail();
        if (bActive && Size.x > 0.0f && Size.y > 0.0f &&
            (!bLayoutInitialized || !ImGui::DockBuilderGetNode(DockSpaceId)))
        {
            ImGui::DockBuilderRemoveNode(DockSpaceId);
            BuildDefaultLayout(DockSpaceId, Size);
            bLayoutInitialized = true;
        }

        ImGuiWindowClass WindowClass;
        WindowClass.ClassId = PanelClassId;
        WindowClass.DockingAllowUnclassed = false;
        const ImGuiDockNodeFlags Flags = bActive ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_KeepAliveOnly;
        if (bActive || ImGui::DockBuilderGetNode(DockSpaceId))
        {
            ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, 0.0f), Flags, &WindowClass);
        }
    }

    for (auto& Panel : Panels)
    {
        if (Panel->IsOpen() && (bActive || Panel->IsDetached(DockSpaceId)))
        {
            Panel->OnRender();
        }
    }
}
