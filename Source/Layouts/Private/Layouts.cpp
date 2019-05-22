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
			
			if (EAppReturnType::Ok != OpenMsgDlgInt(EAppMsgType::OkCancel, LOCTEXT("ActionRestartMsg", "This action requires the editor to restart. you will be prompted to save any changes. Continue?"), LOCTEXT("ResetUILayout_Title", "Reset UI Layout")))
			{
				return;
			}

			//Backup our User Settings
			GetMutableDefault<UEditorPerProjectUserSettings>()->SaveConfig();

			FString BackupEditorLayoutIni = FString::Printf(TEXT("%s_Backup.ini"), *FPaths::GetBaseFilename(GEditorLayoutIni, false));

			//Copied from FMainFrameActionCallbacks::ResetLayout()

			if (COPY_Fail == IFileManager::Get().Copy(*GEditorLayoutIni, *OpenedFiles[0]))
			{
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

			//Restart the Editor
			FUnrealEdMisc::Get().AllowSavingLayoutOnClose(false);
			FUnrealEdMisc::Get().CB_RefreshEditor();
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

void FLayoutsModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FLayoutsCommands::Get().ImportLayout);
	Builder.AddMenuEntry(FLayoutsCommands::Get().ExportLayout);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLayoutsModule, Layouts)