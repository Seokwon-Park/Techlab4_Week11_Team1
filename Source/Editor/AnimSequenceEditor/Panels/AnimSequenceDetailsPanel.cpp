#include "EnginePCH.h"
#include "AnimSequenceDetailsPanel.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditor.h"
#include "Animation/AnimSequence.h"
#include "Render/SkeletalMesh.h"

void FAnimSequenceDetailsPanel::DrawContent()
{
    UAnimSequence* Sequence = Editor.GetSequence();
    const UAnimDataModel* Model = Sequence ? Sequence->GetDataModel() : nullptr;
    if (!Model)
    {
        return;
    }

    ImGui::TextDisabled("Asset Path");
    ImGui::TextWrapped("%s", Editor.GetAssetPath().c_str());
    ImGui::Separator();

    ImGui::Text("Clip: %s", Sequence->GetClipName().c_str());
    ImGui::Text("Length: %.3f s", Model->GetPlayLength());
    ImGui::Text("Frame Rate: %.2f", Model->GetFrameRate().AsDecimal());
    ImGui::Text("Frames: %d (keys %d)", Model->GetNumberOfFrames(), Model->GetNumberOfKeys());
    ImGui::Text("Tracks: %d", static_cast<int>(Model->GetBoneAnimationTracks().Num()));

    ImGui::SeparatorText("Preview Mesh");
    ImGui::TextWrapped("%s", Sequence->GetPreviewMeshPath().c_str());
    if (USkeletalMesh* Mesh = Editor.GetMesh())
    {
        ImGui::Text("Bones: %d", Mesh->GetNumBones());
    }
}
