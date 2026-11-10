#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditor.h"

class FAnimTimelinePanel : public FAssetEditorPanel
{
public:
    explicit FAnimTimelinePanel(FAnimSequenceEditor& InEditor)
        : FAssetEditorPanel(InEditor, "Timeline")
        , Editor(InEditor)
    {
    }

protected:
    void DrawContent() override;

private:
    FAnimSequenceEditor& Editor;

    void DrawTransport();
    void DrawTrack();

    bool bWasPlayingBeforeScrub = false;
};
