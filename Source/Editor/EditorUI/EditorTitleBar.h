#pragma once

#include <imgui.h>

class FWindow;

// 에디터 메인 창과 프로젝트 브라우저 창의 제목 표시줄. 한 번에 한 창에만 Install한다.
// Windows 기본 제목 표시줄을 없애고, 창 맨 위에 ImGui로 끌기용 띠를 그린다.
// 띠에는 왼쪽 위 로고 칸, 가운데 창 제목, 오른쪽 최소화·최대화·닫기 버튼이 있다. 에디터 메뉴 바는 이 띠 아래에 온다.
namespace EditorTitleBar
{
	// 창의 기본 제목 표시줄을 끈다. ImGui 초기화 뒤에 한 번 호출한다.
	void Install(FWindow& Window);
	// 창을 닫기 전에 호출한다. 다른 창(프로젝트 브라우저 다음의 에디터)이 다시 Install할 수 있다.
	void Uninstall();

	// 왼쪽 위 칸에 그릴 로고. 정하지 않으면 빈 칸으로 둔다.
	void SetLogo(ImTextureID Logo);

	// 매 프레임, 메인 뷰포트를 쓰는 다른 창보다 먼저 호출한다.
	// 띠 높이만큼 뷰포트 작업 영역이 줄어 WorkPos를 쓰는 창은 그 아래에 놓인다. Install 전에는 아무것도 하지 않는다.
	void Draw();

	// FWindow가 WM_NCHITTEST에서 부른다. 클라이언트 좌표가 버튼을 뺀 띠 위면 true.
	bool IsCaption(int X, int Y);
}
