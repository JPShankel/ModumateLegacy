// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockNCPNavigator.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Components/Wrapbox.h"
#include "UI/HUDDrawWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/WrapBoxSlot.h"
#include "UI/Custom/NCPNavigatorButton.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"


UBIMBlockNCPNavigator::UBIMBlockNCPNavigator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockNCPNavigator::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UBIMBlockNCPNavigator::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController>();
}

void UBIMBlockNCPNavigator::BuildNCPNavigator(const FBIMTagPath& InNCP)
{
	CurrentNCP = InNCP;

	// Release NCPItemChildren
	for (auto curWidget : WrapBoxNCP->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			Controller->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	WrapBoxNCP->ClearChildren();

	for (int32 i = 0; i < CurrentNCP.Tags.Num(); ++i)
	{
		// Navigator button represents part of the parent tag path
		UNCPNavigatorButton* newButton = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UNCPNavigatorButton>(NCPTextButtonClass);
		FBIMTagPath partialNCP;
		CurrentNCP.GetPartialPath(i + 1, partialNCP);
		newButton->BuildButton(partialNCP, CurrentNCP.Tags[i]);
		WrapBoxNCP->AddChildToWrapBox(newButton);

		if (i < CurrentNCP.Tags.Num() - 1)
		{
			// Separators are visual widget between navigator buttons
			UModumateTextBlockUserWidget* newSeparator = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UModumateTextBlockUserWidget>(TextSeparatorClass);
			newSeparator->ChangeText(SeparatorText);
			WrapBoxNCP->AddChildToWrapBox(newSeparator);
			UWrapBoxSlot* wrapBoxSlot = UWidgetLayoutLibrary::SlotAsWrapBoxSlot(newSeparator);
			if (wrapBoxSlot)
			{
				wrapBoxSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
}
