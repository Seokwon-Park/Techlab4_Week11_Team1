#include "EnginePCH.h"
#include "Editor/AssetEditor/AssetEditorTab.h"
#include "Editor/AssetEditor/AssetEditor.h"
#include <filesystem>

FAssetEditorTab::FAssetEditorTab(FAssetEditor& InEditor) : Editor(InEditor)
{
	const FString Filename = std::filesystem::path(Editor.GetAssetPath()).filename().string();
	const FString Id = FString(" (") + Editor.GetEditorTypeName() + ")###AssetEditor:" + Editor.GetAssetPath();
	TabName = Filename + Id;
	DirtyTabName = Filename + "*" + Id;
}

const char* FAssetEditorTab::GetTabName() const
{
	return Editor.IsDirty() ? DirtyTabName.c_str() : TabName.c_str();
}

bool FAssetEditorTab::ConfirmClose()
{
	return Editor.ConfirmClose();
}

void FAssetEditorTab::OnCloseRequested()
{
	Editor.Close();
}

void FAssetEditorTab::DrawMenuBar()
{
	// 기본 Editor Menu Bar.

	if (ImGui::BeginMenu("Editor"))
	{
		if (ImGui::MenuItem("Close Editor"))
		{
			Editor.Close();
		}

		ImGui::EndMenu();
	}
}

void FAssetEditorTab::OnRender(bool bActive)
{
}
