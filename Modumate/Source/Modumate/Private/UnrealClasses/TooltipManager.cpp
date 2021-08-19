// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/TooltipManager.h"
#include "Database/ModumateDataTables.h"
#include "Components/Widget.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/TooltipWidget.h"



void UTooltipManager::Init()
{
	if (TooltipNonInputDataTable)
	{
		TooltipNonInputDataTable->ForeachRow<FTooltipNonInputDataRow>(TEXT("TooltipNonInputData"),
			[this](const FName& Key, const FTooltipNonInputDataRow& Row)
		{
			if (!Key.IsNone())
			{
				FTooltipData& data = TooltipsMap.FindOrAdd(Key);
				data.TooltipTitle = Row.TooltipTitle;
				data.TooltipText = Row.TooltipText;
			}
		});
	}
}

void UTooltipManager::Shutdown()
{
}

class UTooltipWidget* UTooltipManager::GetOrCreateTooltipWidget(TSubclassOf<class UTooltipWidget> TooltipClass)
{
	if (TooltipClass == PrimaryTooltipWidgetClass)
	{
		if (!PrimaryTooltipWidget)
		{
			PrimaryTooltipWidget = Cast<UTooltipWidget>(CreateWidget(GetWorld()->GetGameInstance(), PrimaryTooltipWidgetClass));
		}
		return PrimaryTooltipWidget;
	}
	else if (TooltipClass == SecondaryTooltipWidgetClass)
	{
		if (!SecondaryTooltipWidget)
		{
			SecondaryTooltipWidget = Cast<UTooltipWidget>(CreateWidget(GetWorld()->GetGameInstance(), SecondaryTooltipWidgetClass));
		}
		return SecondaryTooltipWidget;
	}
	return nullptr;
}

class UWidget* UTooltipManager::GenerateTooltipNonInputWidget(const FName& TooltipID, const class UWidget* FromWidget, const FText& OverrideText)
{
	UModumateGameInstance* gameInstance = FromWidget->GetWorld()->GetGameInstance<UModumateGameInstance>();
	UTooltipManager* manager = gameInstance ? gameInstance->TooltipManager : nullptr;

	if (manager)
	{
		FTooltipData tooltipData = manager->TooltipsMap.FindRef(TooltipID);

		if (!OverrideText.IsEmpty())
		{
			tooltipData.TooltipText = OverrideText;
		}

		if (!tooltipData.TooltipText.IsEmpty())
		{
			if (tooltipData.TooltipTitle.IsEmpty())
			{
				UTooltipWidget* newSecondaryTooltip = manager->GetOrCreateTooltipWidget(manager->SecondaryTooltipWidgetClass);
				if (newSecondaryTooltip)
				{
					newSecondaryTooltip->BuildSecondaryTooltip(tooltipData);
					return newSecondaryTooltip;
				}
			}
			else
			{
				UTooltipWidget* newPrimaryTooltip = manager->GetOrCreateTooltipWidget(manager->PrimaryTooltipWidgetClass);
				if (newPrimaryTooltip)
				{
					newPrimaryTooltip->BuildPrimaryTooltip(tooltipData);
					return newPrimaryTooltip;
				}
			}
		}
	}

	return nullptr;
}

class UWidget* UTooltipManager::GenerateTooltipWithInputWidget(EInputCommand InputCommand, const class UWidget* FromWidget, const FText& OverrideText)
{
	// Since input command requires input component, the EditModelController is needed
	AEditModelPlayerController* controller = FromWidget->GetOwningPlayer<AEditModelPlayerController>();
	if (!controller)
	{
		return nullptr;
	}

	UModumateGameInstance* gameInstance = FromWidget->GetWorld()->GetGameInstance<UModumateGameInstance>();
	UTooltipManager* manager = gameInstance ? gameInstance->TooltipManager : nullptr;

	FInputCommandData inputData;
	controller->InputHandlerComponent->GetInputCommandData(InputCommand, inputData);
	if (manager && inputData.Command != EInputCommand::None)
	{
		FTooltipData newTooltipData;
		newTooltipData.TooltipTitle = inputData.Title;
		newTooltipData.TooltipText = FromWidget->GetIsEnabled() ? inputData.EnabledDescription : inputData.DisabledDescription;
		newTooltipData.AllBindings = inputData.AllBindings;

		if (!OverrideText.IsEmpty())
		{
			newTooltipData.TooltipText = OverrideText;
		}

		UTooltipWidget* newTooltipWidget = manager->GetOrCreateTooltipWidget(manager->PrimaryTooltipWidgetClass);
		if (newTooltipWidget)
		{
			newTooltipWidget->BuildPrimaryTooltip(newTooltipData);
			return newTooltipWidget;
		}
	}
	return nullptr;
}
