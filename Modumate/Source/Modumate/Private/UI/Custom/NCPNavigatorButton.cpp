// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/NCPNavigatorButton.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/BIM/BIMBlockNCPSwitcher.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"

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
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller->EditModelUserWidget->BIMPresetSwap->NCPSwitcher)
	{
		controller->EditModelUserWidget->BIMPresetSwap->NCPSwitcher->BuildNCPSwitcher(ButtonNCP);
	}
}

void UNCPNavigatorButton::BuildButton(const FBIMTagPath& InButtonNCP, const FString& ButtonDisplayName)
{
	ButtonNCP = InButtonNCP;
	ButtonText->SetText(FText::FromString(ButtonDisplayName));
}