// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "LayoutsStyle.h"

class FLayoutsCommands : public TCommands<FLayoutsCommands>
{
public:

	FLayoutsCommands()
		: TCommands<FLayoutsCommands>(TEXT("Layouts"), NSLOCTEXT("Contexts", "Layouts", "Layouts Plugin"), NAME_None, FLayoutsStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;
	
public:
	TSharedPtr< FUICommandInfo > ImportLayout;
	TSharedPtr< FUICommandInfo > ExportLayout;
};