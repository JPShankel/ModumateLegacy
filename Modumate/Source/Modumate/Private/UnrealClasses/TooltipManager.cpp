// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/TooltipManager.h"
#include "Database/ModumateDataTables.h"
#include "Components/Widget.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/EditModelUserWidget.h"
#include "UI/StartMenu/StartRootMenuWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/MainMenuController_CPP.h"
#include "UI/Custom/TooltipWidget.h"



UTooltipManager::UTooltipManager(const FObjectInitializer &ObjectInitializer)
	: Super(ObjectInitializer)
{
}

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

class UTooltipWidget* UTooltipManager::GetOrCreateTooltipWidgetFromPool(TSubclassOf<class UTooltipWidget> TooltipClass)
{
	AEditModelPlayerController_CPP* EditModelController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	if (EditModelController && EditModelController->EditModelUserWidget)
	{
		return EditModelController->GetEditModelHUD()->GetOrCreateWidgetInstance<UTooltipWidget>(TooltipClass);
	}
	else
	{
		AMainMenuController_CPP* MainMenuController = GetWorld()->GetFirstPlayerController<AMainMenuController_CPP>();
		if (MainMenuController && MainMenuController->StartRootMenuWidget)
		{
			return MainMenuController->StartRootMenuWidget->UserWidgetPool.GetOrCreateInstance<UTooltipWidget>(TooltipClass);
		}
	}
	return nullptr;
}

class UWidget* UTooltipManager::GenerateTooltipNonInputWidget(const FName& TooltipID, const class UWidget* FromWidget)
{
	UModumateGameInstance* gameInstance = FromWidget->GetWorld()->GetGameInstance<UModumateGameInstance>();
	UTooltipManager* manager = gameInstance ? gameInstance->TooltipManager : nullptr;

	if (manager)
	{
		const auto& tooltipData = manager->TooltipsMap.FindRef(TooltipID);
		if (!tooltipData.TooltipText.IsEmpty())
		{
			if (tooltipData.TooltipTitle.IsEmpty())
			{
				UTooltipWidget* newSecondaryTooltip = manager->GetOrCreateTooltipWidgetFromPool(manager->SecondaryTooltipWidgetClass);
				if (newSecondaryTooltip)
				{
					newSecondaryTooltip->BuildSecondaryTooltip(tooltipData);
					return newSecondaryTooltip;
				}
			}
			else
			{
				UTooltipWidget* newPrimaryTooltip = manager->GetOrCreateTooltipWidgetFromPool(manager->PrimaryTooltipWidgetClass);
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

class UWidget* UTooltipManager::GenerateTooltipWithInputWidget(EInputCommand InputCommand, const class UWidget* FromWidget)
{
	// Since input command requires input component, the EditModelController is needed
	AEditModelPlayerController_CPP* controller = FromWidget->GetOwningPlayer<AEditModelPlayerController_CPP>();
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
		newTooltipData.TooltipText = inputData.EnabledDescription;
		newTooltipData.AllBindings = inputData.AllBindings;

		UTooltipWidget* newTooltipWidget = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UTooltipWidget>(manager->PrimaryTooltipWidgetClass);
		if (newTooltipWidget)
		{
			newTooltipWidget->BuildPrimaryTooltip(newTooltipData);
			return newTooltipWidget;
		}
	}
	return nullptr;
}
