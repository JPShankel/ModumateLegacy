// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/NCPNavigatorButton.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/BIM/BIMBlockNCPSwitcher.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/BIM/BIMBlockNCPNavigator.h"

UNCPNavigatorButton::UNCPNavigatorButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNCPNavigatorButton::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ModumateButton)
	{
		return false;
	}

	ModumateButton->OnReleased.AddDynamic(this, &UNCPNavigatorButton::OnButtonPress);

	if (ButtonText)
	{
		ButtonText->SetText(ButtonTextOverride);
	}

	return true;
}

void UNCPNavigatorButton::NativeConstruct()
{
	Super::NativeConstruct();
}

void UNCPNavigatorButton::OnButtonPress()
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller->EditModelUserWidget->BIMPresetSwap->NCPSwitcher)
	{
		FBIMTagPath newNCP = ButtonNCP;
		controller->EditModelUserWidget->BIMPresetSwap->NCPSwitcher->BuildNCPSwitcher(newNCP);
		controller->EditModelUserWidget->BIMPresetSwap->NCPNavigator->BuildNCPNavigator(newNCP);
		controller->EditModelUserWidget->BIMPresetSwap->CreatePresetListForSwapFronNCP(newNCP);
	}
}

void UNCPNavigatorButton::BuildButton(const FBIMTagPath& InButtonNCP, const FString& ButtonDisplayName)
{
	ButtonNCP = InButtonNCP;
	ButtonText->SetText(FText::FromString(ButtonDisplayName));
}
