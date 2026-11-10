#pragma once

#include "AnimSequenceEditorPanel.h"
#include "Core/Misc/Guid.h"

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
    // 왼쪽 열: 트랙 이름, 우클릭 메뉴(위/아래 삽입, 이름 변경, 삭제), 더블클릭 이름 변경
    void DrawTrackHeaders(const ImVec2& HeaderOrigin);
    // 드래그 중인 Notify를 마우스 위치의 트랙/시간으로 옮긴다
    void UpdateNotifyDrag(const ImVec2& TracksOrigin, float PixelsPerSecond);

    bool bWasPlayingBeforeScrub = false;

    // Notify 드래그 (누른 Notify의 Guid, 잡은 지점과 시작 시간의 차이)
    FGuid DraggingNotify;
    float DragGrabOffset = 0.0f;
    ImVec2 DragStartMouse;
    bool bDragMoved = false;

    int32 ContextTrack = -1;      // 트랙 메뉴/이름 변경 팝업이 가리키는 트랙
    char RenameBuffer[64] = {};
};
