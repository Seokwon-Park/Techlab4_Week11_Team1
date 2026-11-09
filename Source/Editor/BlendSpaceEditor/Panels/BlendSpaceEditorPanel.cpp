#include "EnginePCH.h"
#include "BlendSpaceEditorPanel.h"
#include "Editor/BlendSpaceEditor/BlendSpaceEditor.h"

FBlendSpaceEditorPanel::FBlendSpaceEditorPanel(FBlendSpaceEditor& InEditor, const char* InName)
    : Editor(InEditor)
    , PanelName(InName)
    , WindowName(PanelName + "###AnimSequencePanel:" + InName + ":" + InEditor.GetAssetPath())
{
}

void FBlendSpaceEditorPanel::OnRender()
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

bool FBlendSpaceEditorPanel::IsDetached(ImGuiID DockSpaceId) const
{
    const ImGuiWindow* Window = ImGui::FindWindowByName(GetWindowName());
    return Window 
        && (!Window->DockNode || ImGui::DockNodeGetRootNode(Window->DockNode)->ID != DockSpaceId);
}