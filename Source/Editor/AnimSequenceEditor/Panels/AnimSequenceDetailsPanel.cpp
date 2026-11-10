#include "EnginePCH.h"
#include "AnimSequenceDetailsPanel.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditor.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceFile.h"
#include "Asset/AssetUtils.h"
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

    // ── 에셋 설정: 시퀀스에 저장되고 게임에도 적용된다 ──
    ImGui::SeparatorText("Root Motion");
    bool bEnableRootMotion = Sequence->GetEnableRootMotion();
    if (ImGui::Checkbox("Enable Root Motion", &bEnableRootMotion))
    {
        Sequence->SetEnableRootMotion(bEnableRootMotion);
        Editor.MarkPoseDirty();   // 일시정지 중에도 포즈를 다시 계산해 root 고정을 바로 보여준다
    }

    // root를 고정할 위치 (아직 기능 연결 안 됨, 항상 Anim First Frame으로 동작)
    static const char* RootLockNames[] = { "Ref Pose", "Anim First Frame", "Zero" };
    int RootLock = static_cast<int>(Sequence->GetRootMotionRootLock());
    if (ImGui::Combo("Root Motion Root Lock", &RootLock, RootLockNames, IM_ARRAYSIZE(RootLockNames)))
    {
        Sequence->SetRootMotionRootLock(static_cast<ERootMotionRootLock>(RootLock));
    }

    // 루트 모션이 꺼져 있어도 root를 고정한다 (Actor는 움직이지 않는 제자리 재생)
    bool bForceRootLock = Sequence->GetForceRootLock();
    if (ImGui::Checkbox("Force Root Lock", &bForceRootLock))
    {
        Sequence->SetForceRootLock(bForceRootLock);
        Editor.MarkPoseDirty();
    }

    // 설정을 .anim.bin에 저장한다. 같은 시퀀스를 쓰는 게임에도 바로 적용된다 (AssetManager 캐시 공유)
    if (ImGui::Button("Save"))
    {
        const FString& AssetPath = Editor.GetAssetPath();
        const FString FilePath = FAssetUtils::ResolveAssetFilePath(AssetPath);
        FString Error;
        SaveMessage = AnimSequenceFile::Write(FilePath.empty() ? AssetPath : FilePath, *Sequence, Error)
            ? "Saved." : "Save failed: " + Error;
    }
    if (!SaveMessage.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", SaveMessage.c_str());
    }

    ImGui::SeparatorText("Preview Mesh");
    ImGui::TextWrapped("%s", Sequence->GetPreviewMeshPath().c_str());
    if (USkeletalMesh* Mesh = Editor.GetMesh())
    {
        ImGui::Text("Bones: %d", Mesh->GetNumBones());
    }
}
