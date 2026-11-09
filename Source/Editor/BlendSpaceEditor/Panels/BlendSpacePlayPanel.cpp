#include "EnginePCH.h"
#include "BlendSpacePlayPanel.h"

#include "Editor/BlendSpaceEditor/BlendSpaceEditor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"

void FBlendSpacePlayPanel::DrawContent()
{
    if (ImGui::Button("Save"))
        Editor.Save();

    const FString& Message = Editor.GetMessage();
    if (!Message.empty())
        ImGui::TextWrapped("%s", Message.c_str());

    USkeletalMeshComponent* Component = Editor.GetMeshComponent();
    if (!Component)
    {
        ImGui::TextUnformatted("No preview mesh.");
        return;
    }

    UAnimSingleNodeInstance* Instance = Component->GetSingleNodeInstance();
    if (!Instance)
    {
        ImGui::TextUnformatted("No animation instance.");
        return;
    }

    if (ImGui::Checkbox("Loop", &bLoop))
        Instance->SetLooping(bLoop);

    if (ImGui::Button("Play / Resume"))
        Component->Play(bLoop);
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
        Component->Stop();

    float Rate = Instance->GetPlayRate();
    if (ImGui::SliderFloat("Play rate", &Rate, -2.0f, 2.0f))
        Instance->SetPlayRate(Rate);
    if (ImGui::DragFloat("Blend input X", &Component->BlendSpaceInput))
        Component->RefreshAnimationPose();

    const float Length = Instance->GetLength();
    float Time = Instance->GetCurrentTime();
    if (Length > 0.0f && ImGui::SliderFloat("Time", &Time, 0.0f, Length))
    {
        Component->Stop();
        Instance->SetPosition(Time);
        Component->RefreshAnimationPose();
    }

    ImGui::Text("%s | %.3f / %.3f seconds",
        Component->IsPlaying() ? "Playing" : "Stopped",
        Instance->GetCurrentTime(), Instance->GetLength());
}