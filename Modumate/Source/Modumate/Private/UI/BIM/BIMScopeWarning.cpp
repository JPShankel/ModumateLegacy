// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMScopeWarning.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/EditModelUserWidget.h"
#include "UI/BIM/BIMDesigner.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "UI/BIM/BIMScopeWarningList.h"
#include "UI/BIM/BIMScopeWarningListItem.h"



UBIMScopeWarning::UBIMScopeWarning(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMScopeWarning::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonConfirm && ButtonDismiss))
	{
		return false;
	}

	ButtonConfirm->ModumateButton->OnReleased.AddDynamic(this, &UBIMScopeWarning::ConfirmScopeWarning);
	ButtonDismiss->ModumateButton->OnReleased.AddDynamic(this, &UBIMScopeWarning::DismissScopeWarning);
	return true;
}

void UBIMScopeWarning::NativeConstruct()
{
	Super::NativeConstruct();

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
}

void UBIMScopeWarning::ConfirmScopeWarning()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (ensureAlways(Controller && Controller->EditModelUserWidget && Controller->EditModelUserWidget->BIMDesigner))
	{
		Controller->EditModelUserWidget->BIMDesigner->ConfirmSavePendingPreset();
	}
}

void UBIMScopeWarning::DismissScopeWarning()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBIMScopeWarning::BuildScopeWarning(const TArray<FGuid>& InAffectedGuid)
{
	SetVisibility(ESlateVisibility::Visible);

	if (!ensureAlways(Controller))
	{
		return;
	}

	// Release previous widget pool
	VerticalBoxList->ClearChildren();
	for (auto& curListWidget : ListWidgets)
	{
		curListWidget->RemoveFromParent();
		Controller->HUDDrawWidget->UserWidgetPool.Release(curListWidget);
	}
	ListWidgets.Empty();

	for (auto& curListItemWidget : ListItemWidgets)
	{
		curListItemWidget->RemoveFromParent();
		Controller->HUDDrawWidget->UserWidgetPool.Release(curListItemWidget);
	}
	ListItemWidgets.Empty();

	// Create map of list based on preset type definition
	TMap<FString, UBIMScopeWarningList*> typeToList;

	for (int32 i = 0; i < InAffectedGuid.Num(); ++i)
	{
		const auto affected = Controller->GetDocument()->GetPresetCollection().PresetFromGUID(InAffectedGuid[i]);
		if (affected)
		{
			// Create item widget
			UBIMScopeWarningListItem* newItem = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMScopeWarningListItem>(BIMScopeWarningTypeListItemClass);
			if (newItem)
			{
				FString nameString = affected->DisplayName.ToString();
				newItem->BuildItem(nameString);
				ListItemWidgets.Add(newItem);

				// Create vertical list for the new item widget to place in
				FString objectTypeString = UModumateTypeStatics::GetTextForObjectType(affected->ObjectType).ToString();
				FString scopeString = BIMNameFromValueScope(affected->NodeScope).ToString();
				FString definitionString = affected->NodeScope == EBIMValueScope::Assembly ? objectTypeString : scopeString;

				UBIMScopeWarningList* listWidget = typeToList.FindRef(definitionString);
				if (listWidget)
				{
					listWidget->AddItemToVerticalList(newItem, ListItemPadding);
				}
				else
				{
					UBIMScopeWarningList* newListWidget = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMScopeWarningList>(BIMScopeWarningTypeListClass);
					if (newListWidget)
					{
						ListWidgets.Add(newListWidget);
						typeToList.Add(definitionString, newListWidget);
						VerticalBoxList->AddChildToVerticalBox(newListWidget);
						newListWidget->BuildPresetListTitle(definitionString);
						newListWidget->AddItemToVerticalList(newItem, ListItemPadding);
					}
				}
			}
		}
	}
}
