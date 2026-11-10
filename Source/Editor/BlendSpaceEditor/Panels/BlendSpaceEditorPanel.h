#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Core/EngineString.h"

class FBlendSpaceEditor;

class FBlendSpaceEditorPanel : public IEditorPanel
{
public:
    FBlendSpaceEditorPanel(FBlendSpaceEditor& InEditor, const char* InName);

    bool Init() override { return true; }
    void Tick(float DeltaTime) override {}
    void OnRender() override;

    const char* GetPanelName() const override { return PanelName.c_str(); }
    const char* GetWindowName() const { return WindowName.c_str(); }
    void SetDockClass(ImGuiID InClassId) { DockClassId = InClassId; }
    bool IsDetached(ImGuiID DockSpaceId) const;

protected:
    virtual void DrawContent() = 0;

    FBlendSpaceEditor& Editor;

private:
    FString PanelName;
    FString WindowName;
    ImGuiID DockClassId = 0;
};
