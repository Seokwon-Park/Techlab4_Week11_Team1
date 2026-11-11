#pragma once

#include "Editor/Viewports/EditorViewportClient.h"
#include "Editor/Rendering/GridRenderer.h"

// 레벨 에디터 뷰포트의 클라이언트. 레벨을 그리고 그 위에 그리드, 선택 외곽선, 기즈모 같은 레벨 편집용 요소를 올린다.
// UE의 FLevelEditorViewportClient와 같은 자리이고, UE도 LevelEditorViewport.h / .cpp에 둔다.
class FLevelEditorViewportClient : public FEditorViewportClient
{
public:
	FLevelEditorViewportClient();
	~FLevelEditorViewportClient() override;

	void Draw(FRenderer* Renderer) override;
	void Draw(const FSceneView* View, FScenePassCallbacks& Callbacks) override;

	// 이 뷰포트의 그리드와 월드 축을 그리는 도구. 뷰포트마다 하나씩 갖는다.
	// UE의 FEditorViewportClient::DrawHelper와 같은 자리이고 UE에서도 public 멤버다.
	// UE의 타입은 FEditorCommonDrawHelper이고 기반 클래스에 있다. 이 엔진은 그리드를 레벨 뷰포트만 그리므로 여기에 둔다.
	FGridRenderer DrawHelper;
};

// 지금 편집 중인(활성) 레벨 뷰포트. 선택 외곽선과 기즈모는 이 뷰포트에만 그린다.
// UE의 GCurrentLevelEditingViewportClient와 같다. UE는 뷰포트가 포커스를 받을 때 바꾸고,
// 이 엔진은 UEditorEngine::RenderViewports가 그리기 전에 뷰포트 패널의 활성 인덱스로 맞춘다.
extern FLevelEditorViewportClient* GCurrentLevelEditingViewportClient;
