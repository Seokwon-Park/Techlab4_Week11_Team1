#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"
#include "Editor/AnimSequenceEditor/AnimSequenceEditor.h"

class FAnimSequenceDetailsPanel : public TAssetEditorPanel<FAnimSequenceEditor>
{
public:
    explicit FAnimSequenceDetailsPanel(FAnimSequenceEditor& InEditor)
        : TAssetEditorPanel<FAnimSequenceEditor>(InEditor, "Details", "AnimSequencePanel")
    {
    }

protected:
    void DrawContent() override;

};
