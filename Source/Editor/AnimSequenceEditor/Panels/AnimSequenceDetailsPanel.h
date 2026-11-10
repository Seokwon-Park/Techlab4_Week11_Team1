#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditor.h"

class FAnimSequenceDetailsPanel : public FAssetEditorPanel
{
public:
    explicit FAnimSequenceDetailsPanel(FAnimSequenceEditor& InEditor)
        : FAssetEditorPanel(InEditor, "Details")
        , Editor(InEditor)
    {
    }

protected:
    void DrawContent() override;

private:
    FAnimSequenceEditor& Editor;
};
