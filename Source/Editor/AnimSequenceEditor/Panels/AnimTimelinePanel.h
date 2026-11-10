#pragma once

#include "AnimSequenceEditorPanel.h"

class FAnimTimelinePanel : public FAnimSequenceEditorPanel
{
public:
    explicit FAnimTimelinePanel(FAnimSequenceEditor& InEditor)
        : FAnimSequenceEditorPanel(InEditor, "Timeline") {
    }

protected:
    void DrawContent() override;

private:
    void DrawTransport();
    void DrawTrack();
    // 눈금자 아래 Notify 트랙들. 그린 높이를 돌려준다.
    float DrawNotifyTracks(const ImVec2& TracksOrigin, float Width, float PixelsPerSecond);

    bool bWasPlayingBeforeScrub = false;
};
