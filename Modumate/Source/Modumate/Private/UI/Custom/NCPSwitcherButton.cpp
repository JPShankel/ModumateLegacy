// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/NCPSwitcherButton.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/BIM/BIMBlockNCPSwitcher.h"

UNCPSwitcherButton::UNCPSwitcherButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNCPSwitcherButton::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ModumateButton)
	{
		return false;
	}

	ModumateButton->OnReleased.AddDynamic(this, &UNCPSwitcherButton::OnButtonPress);

	if (ButtonText)
	{
		ButtonText->SetText(ButtonTextOverride);
	}

	return true;
}

void UNCPSwitcherButton::NativeConstruct()
{
	Super::NativeConstruct();

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
}

void UNCPSwitcherButton::OnButtonPress()
{
	Controller->EditModelUserWidget->BIMPresetSwap->CreatePresetListForSwapFronNCP(NCPTag);
	Controller->EditModelUserWidget->BIMPresetSwap->NCPSwitcher->BuildNCPSwitcher(NCPTag);
}

void UNCPSwitcherButton::BuildButton(const FBIMTagPath& InNCP)
{
	NCPTag = InNCP;

	if (ensureAlways(NCPTag.Tags.Num() > 0))
	{
		FString lastTagString = InNCP.Tags[InNCP.Tags.Num() - 1];
		ButtonText->SetText(FText::FromString(lastTagString));
	}
}
