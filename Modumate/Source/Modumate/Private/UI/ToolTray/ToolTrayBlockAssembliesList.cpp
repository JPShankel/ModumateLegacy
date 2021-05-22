// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UI/ComponentPresetListItem.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "UI/PresetCard/PresetCardItemObject.h"
#include "UI/LeftMenu/NCPNavigator.h"

UToolTrayBlockAssembliesList::UToolTrayBlockAssembliesList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UToolTrayBlockAssembliesList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!ButtonAdd)
	{
		return false;
	}
	ButtonAdd->ModumateButton->OnReleased.AddDynamic(this, &UToolTrayBlockAssembliesList::OnButtonAddReleased);

	return true;
}

void UToolTrayBlockAssembliesList::NativeConstruct()
{
	Super::NativeConstruct();

	Controller = GetOwningPlayer<AEditModelPlayerController>();
	GameState = GetWorld()->GetGameState<AEditModelGameState>();
}

void UToolTrayBlockAssembliesList::RefreshNCPNavigatorAssembliesList(bool bScrollToSelected /*= false*/)
{
	if (!Controller || !Controller->CurrentTool)
	{
		return;
	}

	NCPNavigatorAssembliesList->BuildNCPNavigator(EPresetCardType::AssembliesList);
	if (bScrollToSelected)
	{
		FGuid toolGuid = Controller->CurrentTool->GetAssemblyGUID();
		FBIMTagPath ncpForSwap;
		Controller->GetDocument()->GetPresetCollection().GetNCPForPreset(toolGuid, ncpForSwap);
		if (ncpForSwap.Tags.Num() > 0)
		{
			NCPNavigatorAssembliesList->ResetSelectedAndSearchTag();
			NCPNavigatorAssembliesList->SetNCPTagPathAsSelected(ncpForSwap);
			NCPNavigatorAssembliesList->ScrollPresetToView(toolGuid);
		}
	}
}

void UToolTrayBlockAssembliesList::ResetSearchBox()
{
	NCPNavigatorAssembliesList->ResetSelectedAndSearchTag();
}

void UToolTrayBlockAssembliesList::OnButtonAddReleased()
{
	if (Controller && Controller->EditModelUserWidget)
	{
		Controller->EditModelUserWidget->EventNewCraftingAssembly(Controller->GetToolMode());
	}
}
