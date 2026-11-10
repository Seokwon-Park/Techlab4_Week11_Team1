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
        if (ImGui::MenuItem("Save", "Ctrl+S", false, AnimEditor.IsDirty()))
        {
            AnimEditor.Save();
        }
        if (ImGui::MenuItem("Revert to Saved", nullptr, false, AnimEditor.IsDirty()))
        {
            AnimEditor.RevertToSaved();
        }
        ImGui::Separator();
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

    // Ctrl+S: 이 탭이 보이고 있을 때만 (레벨 에디터 등 다른 탭과 겹치지 않게)
    if (bActive && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && AnimEditor.IsDirty())
    {
        AnimEditor.Save();
    }

    DrawUnsavedChangesPrompt();
}

void FAnimSequenceEditorTab::DrawUnsavedChangesPrompt()
{
    // 닫기가 막히면 에디터가 요청을 남긴다. 팝업은 이 탭의 ImGui 문맥에서 연다.
    if (AnimEditor.IsUnsavedPromptRequested())
    {
        AnimEditor.ClearUnsavedPromptRequest();
        ImGui::OpenPopup("Unsaved Notify Changes");
    }

    // 창 가운데에 띄운다
    const ImGuiViewport* Viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(Viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Unsaved Notify Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s has unsaved notify changes.", AnimEditor.GetAssetPath().c_str());
        ImGui::TextDisabled("Closing without saving reverts them (the sequence is shared at runtime).");
        ImGui::Spacing();

        if (ImGui::Button("Save and Close"))
        {
            // 저장에 실패하면 닫지 않고 메시지를 보여 준다
            if (AnimEditor.Save())
            {
                ImGui::CloseCurrentPopup();
                AnimEditor.CloseWithoutPrompt();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard and Close"))
        {
            AnimEditor.RevertToSaved();
            ImGui::CloseCurrentPopup();
            AnimEditor.CloseWithoutPrompt();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            ImGui::CloseCurrentPopup();
        }

        if (!AnimEditor.GetSaveMessage().empty() && AnimEditor.GetSaveMessage().rfind("Save failed", 0) == 0)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "%s", AnimEditor.GetSaveMessage().c_str());
        }
        ImGui::EndPopup();
    }
}
