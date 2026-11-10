#include "EnginePCH.h"
#include "AssetEditorPanel.h"
#include "Editor/AssetEditor/AssetEditor.h"

FAssetEditorPanel::FAssetEditorPanel(const FAssetEditor& InEditor, const char* InName)
    : PanelName(InName)
    // 기존 Sequence/BlendSpace 패널의 저장된 ImGui 배치를 유지한다.
    , WindowName(PanelName + "###AnimSequencePanel:" + InName + ":" + InEditor.GetAssetPath())
{
}

void FAssetEditorPanel::OnRender()
{
    ImGuiWindowClass WindowClass;
    WindowClass.ClassId = DockClassId;
    WindowClass.DockingAllowUnclassed = false;
    WindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoCloseButton;
    ImGui::SetNextWindowClass(&WindowClass);
    ImGui::SetNextWindowSize(ImVec2(480.0f, 320.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(GetWindowName()))
    {
        DrawContent();
    }
    ImGui::End();
}

bool FAssetEditorPanel::IsDetached(ImGuiID DockSpaceId) const
{
    const ImGuiWindow* Window = ImGui::FindWindowByName(GetWindowName());
    return Window && (!Window->DockNode || ImGui::DockNodeGetRootNode(Window->DockNode)->ID != DockSpaceId);
}