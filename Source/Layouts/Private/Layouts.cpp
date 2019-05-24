// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Layouts.h"
#include "LayoutsStyle.h"
#include "LayoutsCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Dialogs.h"
#include "DesktopPlatform/Public/IDesktopPlatform.h"
#include "SlateApplication.h"
#include "DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "FileManager.h"
#include "MessageLog.h"
#include "MainFrame/Private/Frame/MainFrameActions.h"
#include "SNotificationList.h"
#include "EditorStyleSet.h"
#include "NotificationManager.h"
#include "EditorDirectories.h"
#include "FileHelper.h"
#include "TabManager.h"
#include "CoreGlobals.h"

DEFINE_LOG_CATEGORY(LogLayouts);

static const FName LayoutsTabName("Layouts");

#define LOCTEXT_NAMESPACE "FLayoutsModule"

void FLayoutsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FLayoutsStyle::Initialize();
	FLayoutsStyle::ReloadTextures();

	FLayoutsCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FLayoutsCommands::Get().ImportLayout,
		FExecuteAction::CreateRaw(this, &FLayoutsModule::LoadButtonClicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FLayoutsCommands::Get().ExportLayout,
		FExecuteAction::CreateRaw(this, &FLayoutsModule::SaveButtonClicked),
		FCanExecuteAction());

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FLayoutsModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}

	if (!GConfig) return;

	//Initialize our Layout Section if it doesn't exist.
	if (!GConfig->DoesSectionExist(*LayoutsSection, GEditorSettingsIni))
	{
		for (int i = 0; i < RecentListMax; i++)
		{
			GConfig->SetString(
				*LayoutsSection,
				*FString::Printf(TEXT("RecentLayout%d"), i),
				TEXT(""),
				GEditorSettingsIni
			);
		}
	}
	
	////Push our initialized config update
	GConfig->Flush(false, GEditorSettingsIni);
}

void FLayoutsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FLayoutsStyle::Shutdown();

	FLayoutsCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LayoutsTabName);
}


void FLayoutsModule::LoadButtonClicked()
{
	//FGlobalTabmanager::Get()->InvokeTab(LayoutsTabName);
	
	TArray<FString> OpenedFiles;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		//Call a dialog box to load the ini file.
		bool Opened = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("LoadLayout", "Choose a layout to use").ToString(),
			DefaultDirectory,
			TEXT(""),
			TEXT("Config File |*.ini|"),
			EFileDialogFlags::None,
			OpenedFiles
		);

		if (Opened && OpenedFiles.Num() > 0)
		{
			LoadLayout(OpenedFiles[0]);
		}
	}
}

void FLayoutsModule::SaveButtonClicked()
{
	TArray<FString> SavedFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	
	bool Saved = false;
	if (DesktopPlatform)
	{
		//Call a dialog box to save the ini file.
		Saved = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("LoadLayout", "Choose a layout to use").ToString(),
			*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT)),
			TEXT(""),
			TEXT("Config File |*.ini|"),
			EFileDialogFlags::None,
			SavedFilenames
		);

		if (Saved)
		{
			FString ExportFilename = SavedFilenames[0];
			FString FileName = SavedFilenames[0];

			FString EditorLayoutContent;

			//Serializes all tabs and updates visual states across the editor
			FGlobalTabmanager::Get()->SaveAllVisualState();

			// Write the saved state's config to disk
			GConfig->Flush(false, GEditorLayoutIni);
			
			if (!FFileHelper::LoadFileToString(EditorLayoutContent, *GEditorLayoutIni, FFileHelper::EHashOptions::None))
			{
				FMessageLog EditorErrors("EditorErrors");
				EditorErrors.Warning(FText::FromString("Unable to load Layout Config file."));
			}

			//Fix Path by flipping slashes
			FString NewSavePath = SavedFilenames[0].Replace(TEXT("/"), TEXT("\\"), ESearchCase::IgnoreCase);

			//Write our file
			FFileHelper::SaveStringToFile(*EditorLayoutContent, *NewSavePath);
		}
	}
}

void FLayoutsModule::LoadLayout(const FString& Path)
{
	UE_LOG(LogLayouts, Log, TEXT("Loading Layout: %s"), *Path);

	//Warn user about restart
	if (EAppReturnType::Ok != OpenMsgDlgInt(EAppMsgType::OkCancel, LOCTEXT("ActionRestartMsg", "This action requires the editor to restart. you will be prompted to save any changes. Continue?"), LOCTEXT("ResetUILayout_Title", "Reset UI Layout")))
	{
		return;
	}

	//Backup our User Settings
	GetMutableDefault<UEditorPerProjectUserSettings>()->SaveConfig();
	FString BackupEditorLayoutIni = FString::Printf(TEXT("%s_Backup.ini"), *FPaths::GetBaseFilename(GEditorLayoutIni, false));

	if (COPY_Fail == IFileManager::Get().Copy(*GEditorLayoutIni, *Path))
	{
		//Copy pasta from UE4's Reset Layout - just some error handling.

		FMessageLog EditorErrors("EditorErrors");
		if (!FPaths::FileExists(GEditorLayoutIni))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("FileName"), FText::FromString(GEditorLayoutIni));
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_NoExist_Notification", "Unsuccessful backup! {FileName} does not exist!"), Arguments));
			UE_LOG(LogLayouts, Log, TEXT("Invalid because File Doesn't exist, yo"));
		}
		else if (IFileManager::Get().IsReadOnly(*BackupEditorLayoutIni))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("FileName"), FText::FromString(FPaths::ConvertRelativePathToFull(BackupEditorLayoutIni)));
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_ReadOnly_Notification", "Unsuccessful backup! {FileName} is read-only!"), Arguments));
			UE_LOG(LogLayouts, Log, TEXT("File is read only bitch!"));
		}
		else
		{
			// We don't specifically know why it failed, this is a fallback.
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("SourceFileName"), FText::FromString(GEditorLayoutIni));
			Arguments.Add(TEXT("BackupFileName"), FText::FromString(FPaths::ConvertRelativePathToFull(BackupEditorLayoutIni)));
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_Fallback_Notification", "Unsuccessful backup of {SourceFileName} to {BackupFileName}"), Arguments));
			UE_LOG(LogLayouts, Log, TEXT("Some other reason, we don't know why."));
		}
		EditorErrors.Notify(LOCTEXT("BackupUnsuccessful_Title", "Backup Unsuccessful!"));
	}

	//Update our recent list with the new path.
	UpdateRecentList();

	//Restart the Editor
	FUnrealEdMisc::Get().AllowSavingLayoutOnClose(false);
	FUnrealEdMisc::Get().RestartEditor(false);
}

void FLayoutsModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.BeginSection("LayoutsPlugin", FText::FromString("Layouts Plugin"));
	{
		Builder.AddMenuEntry(FLayoutsCommands::Get().ImportLayout);
		Builder.AddMenuEntry(FLayoutsCommands::Get().ExportLayout);
	
		Builder.AddSubMenu(FText::FromString("Recent Layouts"),
			FText::FromString(""),
			FNewMenuDelegate::CreateRaw(this, &FLayoutsModule::FillRecentMenu));
	}

	Builder.EndSection();
}

void FLayoutsModule::FillRecentMenu(FMenuBuilder& Builder)
{
	for (int i = 0; i < RecentListMax; i++)
	{
		FString CurrentPath;

		GConfig->GetString(
			*LayoutsSection,
			*FString::Printf(TEXT("RecentLayout%d"), i),
			CurrentPath,
			GEditorSettingsIni
		);

		FString Name = FPaths::GetCleanFilename(CurrentPath);

		Builder.AddMenuEntry(
			FText::FromString(*Name),
			FText::FromString(*CurrentPath),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, CurrentPath] {
				LoadLayout(*CurrentPath);
			}))
		);
	}
}

void FLayoutsModule::UpdateRecentList()
{
	for (int i = RecentListMax; i > 0; i--)
	{
		//Shift all of our paths to the right

		FString currentString;

		GConfig->GetString(
			*LayoutsSection,
			*FString::Printf(TEXT("RecentLayout%d"), i - 1),
			currentString,
			GEditorSettingsIni
		);


		GConfig->SetString(
			*LayoutsSection,
			*FString::Printf(TEXT("RecentLayout%d"), i),
			*currentString,
			GEditorSettingsIni
		);
	}

	//Set our first as most recently imported
	GConfig->SetString(
		*LayoutsSection,
		*FString::Printf(TEXT("RecentLayout%d"), 0),
		*Path,
		GEditorSettingsIni
	);

	////Push our initialized config update
	GConfig->Flush(false, GEditorSettingsIni);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLayoutsModule, Layouts)