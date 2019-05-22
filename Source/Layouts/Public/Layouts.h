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
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void LoadButtonClicked();
	void SaveButtonClicked();
	
private:

	void AddMenuExtension(FMenuBuilder& Builder);

	FString DefaultDirectory = FPaths::GetPath(GEditorLayoutIni);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
};
