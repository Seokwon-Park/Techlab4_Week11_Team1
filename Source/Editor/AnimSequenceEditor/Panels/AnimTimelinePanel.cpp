#include "EnginePCH.h"
#include "AnimTimelinePanel.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditor.h"
#include "Animation/AnimNotify.h"
#include "Animation/AnimNotifyState.h"
#include "UObject/Class.h"
#include "UObject/Property.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace
{
    constexpr float RulerHeight = 24.0f;
    constexpr float NotifyRowHeight = 22.0f;
    constexpr float DiamondRadius = 5.0f;
    constexpr float MinStateBarWidth = 4.0f;

    const ImU32 RowColorA = IM_COL32(30, 33, 36, 255);
    const ImU32 RowColorB = IM_COL32(35, 38, 42, 255);
    const ImU32 NamedNotifyColor = IM_COL32(230, 190, 80, 255);    // 이름 Notify
    const ImU32 ClassNotifyColor = IM_COL32(90, 190, 230, 255);    // UAnimNotify
    const ImU32 NotifyStateColor = IM_COL32(170, 120, 220, 255);   // UAnimNotifyState

    const UObject* NotifyObjectOf(const FAnimNotifyEvent& Event)
    {
        if (Event.NotifyStateClass) return Event.NotifyStateClass;
        return Event.Notify;
    }

    ImU32 NotifyColorOf(const FAnimNotifyEvent& Event)
    {
        if (Event.NotifyStateClass) return NotifyStateColor;
        return Event.Notify ? ClassNotifyColor : NamedNotifyColor;
    }

    ImU32 WithAlpha(ImU32 Color, int Alpha)
    {
        return (Color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(Alpha) << IM_COL32_A_SHIFT);
    }

    // "UAnimNotifyState_AttackWindow" -> "AttackWindow"
    FString ShortClassName(const UObject& Object)
    {
        FString Name = Object.GetClass()->Name;
        if (!Name.empty() && Name[0] == 'U') Name.erase(0, 1);
        for (const char* Prefix : { "AnimNotifyState_", "AnimNotify_" })
        {
            if (Name.rfind(Prefix, 0) == 0)
            {
                Name.erase(0, std::strlen(Prefix));
                break;
            }
        }
        return Name;
    }

    FString NotifyLabel(const FAnimNotifyEvent& Event)
    {
        if (auto* Named = dynamic_cast<const UAnimNotifyState_Named*>(Event.NotifyStateClass))
            return Named->NotifyName.empty() ? FString("(Unnamed)") : Named->NotifyName;

        if (auto* Sound = dynamic_cast<const UAnimNotify_PlaySound*>(Event.Notify))
        {
            const FString Stem = std::filesystem::path(Sound->SoundPath).stem().string();
            return Stem.empty() ? FString("PlaySound") : "PlaySound: " + Stem;
        }

        if (const UObject* Object = NotifyObjectOf(Event))
            return ShortClassName(*Object);

        return Event.NotifyName.IsNone() ? FString("(Unnamed)") : Event.NotifyName.ToString();
    }

    // 툴팁에 Notify 객체의 UPROPERTY 값을 보여준다 (편집은 디테일 패널 단계에서)
    void DrawPropertyLines(const UObject& Object)
    {
        for (const UStruct* Struct = Object.GetClass(); Struct; Struct = Struct->GetSuperStruct())
        {
            for (const FProperty* Property : Struct->GetProperties())
            {
                const void* Value = Property->ContainerPtrToValuePtr<void>(&Object);
                const char* Name = Property->Name.c_str();
                if (CastField<FFloatProperty>(Property))       ImGui::Text("%s: %.3f", Name, *static_cast<const float*>(Value));
                else if (CastField<FIntProperty>(Property))    ImGui::Text("%s: %d", Name, *static_cast<const int32*>(Value));
                else if (CastField<FBoolProperty>(Property))   ImGui::Text("%s: %s", Name, *static_cast<const bool*>(Value) ? "true" : "false");
                else if (CastField<FStrProperty>(Property))    ImGui::Text("%s: %s", Name, static_cast<const FString*>(Value)->c_str());
                else if (CastField<FVectorProperty>(Property))
                {
                    const FVector& V = *static_cast<const FVector*>(Value);
                    ImGui::Text("%s: (%.2f, %.2f, %.2f)", Name, V.X, V.Y, V.Z);
                }
            }
        }
    }

    void DrawNotifyTooltip(const FAnimNotifyEvent& Event, float FrameRate)
    {
        ImGui::BeginTooltip();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(NotifyColorOf(Event)), "%s", NotifyLabel(Event).c_str());

        const UObject* Object = NotifyObjectOf(Event);
        if (Event.NotifyStateClass)  ImGui::TextDisabled("NotifyState (%s)", Object->GetClass()->Name.c_str());
        else if (Event.Notify)       ImGui::TextDisabled("Notify (%s)", Object->GetClass()->Name.c_str());
        else                         ImGui::TextDisabled("Notify (Name)");

        ImGui::Separator();
        ImGui::Text("Time: %.3f s  (Frame %.1f)", Event.TriggerTime, Event.TriggerTime * FrameRate);
        if (Event.NotifyStateClass)
            ImGui::Text("Duration: %.3f s  (%.1f frames)", Event.Duration, Event.Duration * FrameRate);
        if (Event.TriggerWeightThreshold > 0.0f)
            ImGui::Text("Weight Threshold: %.2f", Event.TriggerWeightThreshold);
        ImGui::Text("Track: %d", Event.TrackIndex);

        if (Object)
        {
            ImGui::Separator();
            DrawPropertyLines(*Object);
        }
        ImGui::EndTooltip();
    }
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

    // Notify 트랙 (눈금자와 같은 X축)
    const ImVec2 TracksOrigin(Origin.x, ImGui::GetCursorScreenPos().y);
    const float TracksHeight = DrawNotifyTracks(TracksOrigin, Width, PixelsPerSecond);

    // 플레이헤드: 눈금자부터 트랙 끝까지
    const float PlayheadX = Origin.x + Editor.GetPlaybackTime() * PixelsPerSecond;
    Draw->AddLine(ImVec2(PlayheadX, Origin.y), ImVec2(PlayheadX, TracksOrigin.y + TracksHeight), IM_COL32(255, 80, 60, 255), 2.0f);
}

float FAnimTimelinePanel::DrawNotifyTracks(const ImVec2& TracksOrigin, float Width, float PixelsPerSecond)
{
    const UAnimSequence* Sequence = Editor.GetSequence();
    const TArray<FAnimNotifyEvent>& Notifies = Sequence->GetNotifies();
    const float Rate = Editor.GetFrameRate();
    const float Length = Editor.GetPlayLength();

    int32 NumRows = 1;
    for (const FAnimNotifyEvent& Event : Notifies)
        NumRows = std::max(NumRows, Event.TrackIndex + 1);

    const float Height = NumRows * NotifyRowHeight;
    ImDrawList* Draw = ImGui::GetWindowDrawList();
    const ImVec2 TracksMax(TracksOrigin.x + Width, TracksOrigin.y + Height);

    // 줄 배경
    for (int32 Row = 0; Row < NumRows; ++Row)
    {
        const float Y = TracksOrigin.y + Row * NotifyRowHeight;
        Draw->AddRectFilled(ImVec2(TracksOrigin.x, Y), ImVec2(TracksMax.x, Y + NotifyRowHeight), (Row % 2) ? RowColorB : RowColorA);
    }

    // 프레임 격자: 프레임 간격이 충분히 넓을 때만
    const float PixelsPerFrame = Rate > 0.0f ? PixelsPerSecond / Rate : 0.0f;
    if (PixelsPerFrame >= 6.0f)
    {
        const int32 NumFrames = static_cast<int32>(std::floor(Length * Rate + 0.0001f));
        for (int32 Frame = 0; Frame <= NumFrames; ++Frame)
        {
            const float X = TracksOrigin.x + Frame * PixelsPerFrame;
            Draw->AddLine(ImVec2(X, TracksOrigin.y), ImVec2(X, TracksMax.y), IM_COL32(255, 255, 255, 14));
        }
    }

    // 툴팁/선택은 스크럽 같은 다른 조작 중에는 받지 않는다
    const ImVec2 Mouse = ImGui::GetIO().MousePos;
    const bool bCanHover = ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive()
        && ImGui::IsMouseHoveringRect(TracksOrigin, TracksMax);
    const FAnimNotifyEvent* Hovered = nullptr;
    const FGuid& Selected = Editor.GetSelectedNotify();
    const ImU32 SelectedOutline = IM_COL32(255, 255, 255, 255);

    // State 막대를 먼저 그리고 단발 마름모를 위에 그린다 (짧은 마름모가 막대에 가려지지 않게)
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        for (const FAnimNotifyEvent& Event : Notifies)
        {
            const bool bIsState = Event.NotifyStateClass != nullptr;
            if (bIsState != (Pass == 0))
                continue;

            const float RowTop = TracksOrigin.y + Event.TrackIndex * NotifyRowHeight;
            const float CenterY = RowTop + NotifyRowHeight * 0.5f;
            const float StartX = TracksOrigin.x + Event.TriggerTime * PixelsPerSecond;
            const ImU32 Color = NotifyColorOf(Event);
            const FString Label = NotifyLabel(Event);
            const bool bSelected = Selected.IsValid() && Event.Guid == Selected;

            ImVec2 HitMin, HitMax;
            if (bIsState)
            {
                const float EndX = std::max(TracksOrigin.x + Event.GetEndTriggerTime() * PixelsPerSecond, StartX + MinStateBarWidth);
                const ImVec2 BarMin(StartX, RowTop + 3.0f);
                const ImVec2 BarMax(EndX, RowTop + NotifyRowHeight - 3.0f);
                Draw->AddRectFilled(BarMin, BarMax, WithAlpha(Color, bSelected ? 210 : 150), 3.0f);
                Draw->AddRect(BarMin, BarMax, bSelected ? SelectedOutline : Color, 3.0f, 0, bSelected ? 2.0f : 1.0f);

                // 라벨은 막대 안에서 잘라 그린다
                Draw->PushClipRect(BarMin, BarMax, true);
                Draw->AddText(ImVec2(BarMin.x + 4.0f, CenterY - ImGui::GetTextLineHeight() * 0.5f), IM_COL32(255, 255, 255, 230), Label.c_str());
                Draw->PopClipRect();

                HitMin = BarMin;
                HitMax = BarMax;
            }
            else
            {
                const float Radius = bSelected ? DiamondRadius + 1.5f : DiamondRadius;
                const ImVec2 Points[4] =
                {
                    ImVec2(StartX, CenterY - Radius),
                    ImVec2(StartX + Radius, CenterY),
                    ImVec2(StartX, CenterY + Radius),
                    ImVec2(StartX - Radius, CenterY),
                };
                Draw->AddConvexPolyFilled(Points, 4, Color);
                Draw->AddPolyline(Points, 4, bSelected ? SelectedOutline : IM_COL32(20, 20, 20, 255), ImDrawFlags_Closed, bSelected ? 2.0f : 1.0f);
                Draw->AddText(ImVec2(StartX + DiamondRadius + 3.0f, CenterY - ImGui::GetTextLineHeight() * 0.5f), IM_COL32(220, 220, 220, 255), Label.c_str());

                HitMin = ImVec2(StartX - DiamondRadius - 1.0f, RowTop);
                HitMax = ImVec2(StartX + DiamondRadius + 1.0f, RowTop + NotifyRowHeight);
            }

            // 나중에 그린 것(위에 보이는 것)이 우선
            if (bCanHover && Mouse.x >= HitMin.x && Mouse.x <= HitMax.x && Mouse.y >= HitMin.y && Mouse.y <= HitMax.y)
                Hovered = &Event;
        }
    }

    // 트랙 영역 전체를 버튼으로 덮어 클릭을 받는다 (레이아웃 공간도 차지해 창 스크롤이 생긴다)
    ImGui::SetCursorScreenPos(TracksOrigin);
    ImGui::InvisibleButton("##NotifyTracks", ImVec2(Width, Height));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        // 빈 곳을 누르면 선택 해제
        if (Hovered) Editor.SelectNotify(Hovered->Guid);
        else Editor.ClearNotifySelection();
    }

    if (Hovered && ImGui::IsItemHovered())
    {
        DrawNotifyTooltip(*Hovered, Rate);
    }

    return Height;
}
