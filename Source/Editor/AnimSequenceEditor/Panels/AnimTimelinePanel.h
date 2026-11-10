#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditor.h"

class FAnimTimelinePanel : public TAssetEditorPanel<FAnimSequenceEditor>
{
public:
    explicit FAnimTimelinePanel(FAnimSequenceEditor& InEditor)
        : TAssetEditorPanel<FAnimSequenceEditor>(InEditor, "Timeline", "AnimSequencePanel")
    {
    }

protected:
    void DrawContent() override;

private:
    void DrawTransport();
    void DrawTrack();

    bool bWasPlayingBeforeScrub = false;
};
