// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

DECLARE_LOG_CATEGORY_EXTERN(LogLayouts, Log, All);

class FLayoutsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	void LoadButtonClicked();

	void SaveButtonClicked();

	/*
		Validates and backs up the current layout before importing, updating recent list and restarting the editor.
	*/
	void LoadLayout(const FString& Path);

private:

	void AddMenuExtension(FMenuBuilder& Builder);

	void FillRecentMenu(FMenuBuilder& Builder);

	void UpdateRecentList();

	FString DefaultDirectory = FPaths::GetPath(GEditorLayoutIni);

	FString LayoutsSection = "LayoutsPlugin";

	int32 RecentListMax = 5;

private:
	TSharedPtr<class FUICommandList> PluginCommands;
};
