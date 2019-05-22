// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LayoutsCommands.h"

#define LOCTEXT_NAMESPACE "FLayoutsModule"

void FLayoutsCommands::RegisterCommands()
{
	UI_COMMAND(ImportLayout, "Import Layout", "Imports a layout of choice.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ExportLayout, "Export Layout", "Saves a Layout.", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
