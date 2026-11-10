#pragma once

#include "EnginePCH.h"
#include <Windows.h>
#include <shellapi.h>
#include "LaunchEngineLoop.h"
#include "Core/Misc/Paths.h"

// 실행 파일이 어떤 엔진 모드(UEngine 서브클래스)로 돌지 정한다. UE의 ini GameEngine= 설정 역할.
extern UClass* GetEngineClass();

// 커맨드라인에서 .hproject를 못 찾았을 때 실행 파일이 프로젝트를 정한다.
// 에디터는 프로젝트 브라우저 창을 띄우고, 게임은 exe 옆이나 엔진 폴더에서 찾는다.
// FromCommandLine은 커맨드라인으로 받은 경로(없으면 빈 경로). 쓸 .hproject 경로를 돌려주고, 빈 경로면 종료한다.
extern std::filesystem::path ResolveProjectFile(HINSTANCE hInstance, const std::filesystem::path& FromCommandLine);

// UE처럼 첫 인자로 프로젝트 파일을 받는다. "Path/To/Game.hproject" 또는 -project=Path/To/Game.hproject
inline std::filesystem::path FindProjectInCommandLine()
{
	int32 Count = 0;
	LPWSTR* Args = CommandLineToArgvW(GetCommandLineW(), &Count);
	if (!Args) return {};

	std::filesystem::path Result;
	for (int32 i = 1; i < Count && Result.empty(); ++i)
	{
		std::wstring Arg = Args[i];
		constexpr std::wstring_view Prefix = L"-project=";
		if (Arg.size() > Prefix.size() && _wcsnicmp(Arg.c_str(), Prefix.data(), Prefix.size()) == 0)
			Arg = Arg.substr(Prefix.size());
		const std::filesystem::path Candidate(Arg);
		if (_wcsicmp(Candidate.extension().c_str(), FPaths::ProjectExtension) == 0)
			Result = Candidate;
	}
	LocalFree(Args);
	return Result;
}

// 엔진 폴더와 프로젝트를 정하고 작업 디렉터리를 프로젝트 폴더로 옮긴다.
inline bool InitProjectPaths(HINSTANCE hInstance)
{
	if (!FPaths::InitEngineDir())
	{
		MessageBoxW(nullptr, L"Cannot find the engine folder (Resources/Shader).", L"Hitori", MB_OK | MB_ICONERROR);
		return false;
	}

	const std::filesystem::path ProjectFile = ResolveProjectFile(hInstance, FindProjectInCommandLine());
	if (ProjectFile.empty()) return false;

	FString Error;
	if (!FPaths::SetProject(ProjectFile, &Error))
	{
		MessageBoxA(nullptr, ("Cannot open project:\n" + Error).c_str(), "Hitori", MB_OK | MB_ICONERROR);
		return false;
	}
	return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	if (!InitProjectPaths(hInstance)) return -1;
	{
		FEngineLoop EngineLoop;
		if (!EngineLoop.PreInit(hInstance, GetEngineClass())) return -1;

		while (EngineLoop.Tick())
		{
		}

		EngineLoop.Exit();
	}

	if (FEngineStatics::TotalAllocationCount > 0)
		OutputDebugStringA("leaked UObjects\n");

	return 0;
}
