#include "EnginePCH.h"
#include "AnimTimelinePanel.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr float RulerHeight = 24.0f;
}

void FAnimTimelinePanel::DrawContent()
{
    if (!Editor.GetSequence())
    {
        ImGui::TextDisabled("No sequence");
        return;
    }

    DrawTransport();
    ImGui::Separator();
    DrawTrack();
}

void FAnimTimelinePanel::DrawTransport()
{
    const float Length = Editor.GetPlayLength();
    const float Rate = Editor.GetFrameRate();

    if (ImGui::Button("|<")) Editor.SetPlaybackTime(0.0f);
    ImGui::SameLine();
    if (ImGui::Button("<")) Editor.StepFrame(-1);
    ImGui::SameLine();
    if (ImGui::Button(Editor.IsPlaying() ? "Pause" : "Play"))
    {
        if (Editor.IsPlaying()) Editor.Pause();
        else Editor.Play();
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) Editor.StepFrame(+1);
    ImGui::SameLine();
    if (ImGui::Button(">|")) Editor.SetPlaybackTime(Length);

    ImGui::SameLine();
    ImGui::Checkbox("Loop", &Editor.bLooping);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::DragFloat("Rate", &Editor.PlayRate, 0.01f, -2.0f, 2.0f, "%.2fx");

    const float Time = Editor.GetPlaybackTime();
    ImGui::SameLine();
    ImGui::Text("%.2f / %.2f s   Frame %d / %d", Time, Length,
        static_cast<int>(std::lround(Time * Rate)), Editor.GetNumFrames());
}

void FAnimTimelinePanel::DrawTrack()
{
    const float Length = Editor.GetPlayLength();
    const float Rate = Editor.GetFrameRate();

    ImDrawList* Draw = ImGui::GetWindowDrawList();
    const ImGuiIO& IO = ImGui::GetIO();
    const ImVec2 Origin = ImGui::GetCursorScreenPos();
    const float Width = std::max(ImGui::GetContentRegionAvail().x, 50.0f);
    const float PixelsPerSecond = Length > 0.0f ? Width / Length : 1.0f;   // 클립 전체를 한 화면에

    Draw->AddRectFilled(Origin, ImVec2(Origin.x + Width, Origin.y + RulerHeight), IM_COL32(40, 44, 48, 255));

    // 눈금: 60픽셀 이상 간격이 나오는 가장 작은 단위를 고른다 (SequencerPanel과 같은 방식)
    const float Steps[] = { 1.0f / Rate, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f };
    float Step = Steps[IM_ARRAYSIZE(Steps) - 1];
    for (float Candidate : Steps)
    {
        if (Candidate * PixelsPerSecond >= 60.0f) { Step = Candidate; break; }
    }
    for (float T = 0.0f; T <= Length + 0.0001f; T += Step)
    {
        const float X = Origin.x + T * PixelsPerSecond;
        Draw->AddLine(ImVec2(X, Origin.y + RulerHeight - 6.0f), ImVec2(X, Origin.y + RulerHeight), IM_COL32(150, 150, 150, 255));
        char Label[16];
        std::snprintf(Label, sizeof(Label), "%.2f", T);
        Draw->AddText(ImVec2(X + 3.0f, Origin.y + 3.0f), IM_COL32(190, 190, 190, 255), Label);
    }

    // 스크럽: 누른 동안 마우스를 잡아 두므로 영역 밖으로 끌어도 계속된다
    ImGui::InvisibleButton("##Scrub", ImVec2(Width, RulerHeight));
    if (ImGui::IsItemActivated())
    {
        bWasPlayingBeforeScrub = Editor.IsPlaying();
    }
    if (ImGui::IsItemActive())
    {
        Editor.SetPlaybackTime(std::clamp((IO.MousePos.x - Origin.x) / PixelsPerSecond, 0.0f, Length));
    }
    if (ImGui::IsItemDeactivated() && bWasPlayingBeforeScrub)
    {
        Editor.Play();
    }

    // 플레이헤드
    const float PlayheadX = Origin.x + Editor.GetPlaybackTime() * PixelsPerSecond;
    Draw->AddLine(ImVec2(PlayheadX, Origin.y), ImVec2(PlayheadX, Origin.y + RulerHeight), IM_COL32(255, 80, 60, 255), 2.0f);
}
