#include "EnginePCH.h"
#include "Editor/EditorUI/EditorTitleBar.h"

#include "Core/Window.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace
{
	// 마지막으로 그린 프레임의 제목 표시줄 정보. 좌표는 메인 창 클라이언트 기준 픽셀이다.
	// WM_NCHITTEST는 ImGui 프레임 사이에 오므로 직전 프레임 값으로 판단한다. 둘 다 메인 스레드에서만 쓴다.
	struct FCaptionArea
	{
		float Height = 0.0f;
		float DragMinX = 0.0f;
		float DragMaxX = 0.0f;
		// 메뉴 팝업이 열려 있으면 빈 곳 클릭이 팝업을 닫아야 하므로 끌기를 막는다.
		bool bBlocked = true;
	};

	bool GInstalled = false;
	HWND GHwnd = nullptr;
	FString GTitle;
	ImTextureID GLogo = ImTextureID_Invalid;
	FCaptionArea GCaption;

	FString ReadWindowTitle(HWND Hwnd)
	{
		const int Length = GetWindowTextLengthW(Hwnd);
		if (Length <= 0) return {};
		std::wstring Wide(static_cast<size_t>(Length) + 1, L'\0');
		GetWindowTextW(Hwnd, Wide.data(), Length + 1);
		Wide.resize(static_cast<size_t>(Length));

		const int Size = WideCharToMultiByte(CP_UTF8, 0, Wide.c_str(), Length, nullptr, 0, nullptr, nullptr);
		FString Utf8(static_cast<size_t>(Size), '\0');
		WideCharToMultiByte(CP_UTF8, 0, Wide.c_str(), Length, Utf8.data(), Size, nullptr, nullptr);
		return Utf8;
	}

	enum class ECaptionButton { Minimize, Maximize, Close };

	// 버튼 하나를 그리고 눌렸으면 true. 모양은 Windows 11 제목 표시줄 버튼을 따라 선으로 그린다.
	bool CaptionButton(const char* Id, ECaptionButton Kind, const ImVec2& Min, const ImVec2& Size, bool bMaximized)
	{
		ImGui::SetCursorScreenPos(Min);
		const bool bPressed = ImGui::InvisibleButton(Id, Size);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 Max(Min.x + Size.x, Min.y + Size.y);
		if (bHovered || bHeld)
		{
			ImU32 Background;
			if (Kind == ECaptionButton::Close)
				Background = bHeld ? IM_COL32(241, 112, 122, 255) : IM_COL32(232, 17, 35, 255);
			else
				Background = bHeld ? IM_COL32(255, 255, 255, 40) : IM_COL32(255, 255, 255, 24);
			DrawList->AddRectFilled(Min, Max, Background);
		}

		const ImU32 Color = ImGui::GetColorU32(ImGuiCol_Text);
		const float Thickness = ImMax(1.0f, ImFloor(ImGui::GetStyle().FontScaleDpi));
		// 아이콘 크기는 버튼 높이에 비례시켜 DPI 배율을 따라간다.
		const float Half = ImFloor(Size.y * 0.15f);
		const ImVec2 Center(ImFloor((Min.x + Max.x) * 0.5f) + 0.5f, ImFloor((Min.y + Max.y) * 0.5f) + 0.5f);

		switch (Kind)
		{
		case ECaptionButton::Minimize:
			DrawList->AddLine(ImVec2(Center.x - Half, Center.y), ImVec2(Center.x + Half, Center.y), Color, Thickness);
			break;
		case ECaptionButton::Maximize:
			if (bMaximized)
			{
				// 복원 모양: 뒤쪽 사각형의 보이는 두 변과 앞쪽 사각형.
				const float Offset = ImFloor(Half * 0.4f);
				DrawList->AddLine(ImVec2(Center.x - Half + Offset, Center.y - Half), ImVec2(Center.x + Half, Center.y - Half), Color, Thickness);
				DrawList->AddLine(ImVec2(Center.x + Half, Center.y - Half), ImVec2(Center.x + Half, Center.y + Half - Offset), Color, Thickness);
				DrawList->AddRect(ImVec2(Center.x - Half, Center.y - Half + Offset), ImVec2(Center.x + Half - Offset, Center.y + Half), Color, 0.0f, 0, Thickness);
			}
			else
			{
				DrawList->AddRect(ImVec2(Center.x - Half, Center.y - Half), ImVec2(Center.x + Half, Center.y + Half), Color, 0.0f, 0, Thickness);
			}
			break;
		case ECaptionButton::Close:
			DrawList->AddLine(ImVec2(Center.x - Half, Center.y - Half), ImVec2(Center.x + Half, Center.y + Half), Color, Thickness);
			DrawList->AddLine(ImVec2(Center.x + Half, Center.y - Half), ImVec2(Center.x - Half, Center.y + Half), Color, Thickness);
			break;
		}
		return bPressed;
	}
}

void EditorTitleBar::Install(FWindow& Window)
{
	GHwnd = Window.GetHandle();
	GTitle = ReadWindowTitle(GHwnd);
	Window.EnableCustomTitleBar(&EditorTitleBar::IsCaption);
	GInstalled = true;
}

void EditorTitleBar::Uninstall()
{
	GInstalled = false;
	GHwnd = nullptr;
	GTitle.clear();
	GLogo = ImTextureID_Invalid;
	GCaption = FCaptionArea{};
}

void EditorTitleBar::SetLogo(ImTextureID Logo)
{
	GLogo = Logo;
}

void EditorTitleBar::Draw()
{
	if (!GInstalled) return;

	// 메인 뷰포트 맨 위에 붙는 띠 창으로 만든다. 이 높이만큼 뷰포트 작업 영역(WorkPos, WorkSize)이 줄어들어
	// 그 아래에 놓이는 창(에디터의 메뉴 바와 도킹 영역, 브라우저 본문)이 자동으로 밀려 내려간다.
	ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const float Height = ImFloor(ImGui::GetFontSize() * 2.2f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetColorU32(ImGuiCol_TitleBg));
	const bool bVisible = ImGui::BeginViewportSideBar("##WindowTitleBar", MainViewport, ImGuiDir_Up, Height,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoNavFocus);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	if (!bVisible)
	{
		GCaption.bBlocked = true;
		ImGui::End();
		return;
	}

	ImGuiWindow* Window = ImGui::GetCurrentWindow();
	const ImRect Bar = Window->Rect();

	// 왼쪽 위 정사각형 칸은 로고 자리다. 로고가 없으면 빈 칸으로 둔다.
	const ImRect LogoRect(Bar.Min, ImVec2(Bar.Min.x + Height, Bar.Max.y));
	if (GLogo != ImTextureID_Invalid)
	{
		const float Inset = ImFloor(Height * 0.15f);
		Window->DrawList->AddImage(GLogo,
			ImVec2(LogoRect.Min.x + Inset, LogoRect.Min.y + Inset),
			ImVec2(LogoRect.Max.x - Inset, LogoRect.Max.y - Inset));
	}

	const ImVec2 ButtonSize(ImFloor(Height * 1.4f), Height);
	const float ButtonsMinX = Bar.Max.x - ButtonSize.x * 3.0f;

	// 창 제목은 가운데에 두되, 로고나 버튼과 겹치면 로고 바로 뒤로 민다.
	if (!GTitle.empty())
	{
		const ImVec2 TextSize = ImGui::CalcTextSize(GTitle.c_str());
		const float Spacing = ImGui::GetStyle().ItemSpacing.x * 2.0f;
		float TextX = ImFloor((Bar.Min.x + Bar.Max.x - TextSize.x) * 0.5f);
		TextX = ImMax(TextX, LogoRect.Max.x + Spacing);
		if (TextX + TextSize.x <= ButtonsMinX - Spacing)
		{
			const float TextY = ImFloor(Bar.Min.y + (Height - TextSize.y) * 0.5f);
			Window->DrawList->AddText(ImVec2(TextX, TextY), ImGui::GetColorU32(ImGuiCol_TextDisabled), GTitle.c_str());
		}
	}

	// 버튼은 바로 실행하지 않고 메시지로 보낸다. 프레임 도중에 창 크기가 바뀌지 않고, 닫기는 평소의 WM_CLOSE 흐름을 탄다.
	const bool bMaximized = IsZoomed(GHwnd) != FALSE;
	ImGui::PushID("##TitleBarButtons");
	if (CaptionButton("Minimize", ECaptionButton::Minimize, ImVec2(ButtonsMinX, Bar.Min.y), ButtonSize, bMaximized))
		PostMessageW(GHwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
	if (CaptionButton("Maximize", ECaptionButton::Maximize, ImVec2(ButtonsMinX + ButtonSize.x, Bar.Min.y), ButtonSize, bMaximized))
		PostMessageW(GHwnd, WM_SYSCOMMAND, bMaximized ? SC_RESTORE : SC_MAXIMIZE, 0);
	if (CaptionButton("Close", ECaptionButton::Close, ImVec2(ButtonsMinX + ButtonSize.x * 2.0f, Bar.Min.y), ButtonSize, bMaximized))
		PostMessageW(GHwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
	ImGui::PopID();

	// 버튼을 뺀 줄 전체(로고 칸 포함)로 창을 끈다.
	// ImGui 좌표는 화면 기준이고 메인 뷰포트 위치가 클라이언트 원점이다.
	GCaption.Height = Bar.Max.y - MainViewport->Pos.y;
	GCaption.DragMinX = Bar.Min.x - MainViewport->Pos.x;
	GCaption.DragMaxX = ButtonsMinX - MainViewport->Pos.x;
	GCaption.bBlocked = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

	ImGui::End();
}

bool EditorTitleBar::IsCaption(int X, int Y)
{
	if (!GInstalled || GCaption.bBlocked) return false;
	const float PointX = static_cast<float>(X);
	const float PointY = static_cast<float>(Y);
	return PointY >= 0.0f && PointY < GCaption.Height &&
		PointX >= GCaption.DragMinX && PointX < GCaption.DragMaxX;
}
