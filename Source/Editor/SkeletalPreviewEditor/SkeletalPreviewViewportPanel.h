#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"
#include "SkeletalPreviewEditor.h"

class FSkeletalPreviewViewportPanel : public FAssetEditorPanel
{
public:
    explicit FSkeletalPreviewViewportPanel(FSkeletalPreviewEditor& InEditor)
        : FAssetEditorPanel(InEditor, "Viewport")
        , Editor(InEditor)
    {
    }

protected:
    void DrawContent() override;

private:
    FSkeletalPreviewEditor& Editor;
};
