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

    bool bWasPlayingBeforeScrub = false;
};
