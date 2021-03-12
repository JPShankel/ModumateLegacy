// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneMenuBlockExport.h"

#include "Components/ListView.h"
#include "Components/RichTextBlock.h"
#include "Components/VerticalBox.h"
#include "Objects/CutPlane.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateCheckBox.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/EditModelUserWidget.h"
#include "UI/RightMenu/CutPlaneDimListItemObject.h"
#include "UI/RightMenu/CutPlaneMenuBlock.h"
#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"

#define LOCTEXT_NAMESPACE "ModumateCutPlane"

UCutPlaneMenuBlockExport::UCutPlaneMenuBlockExport(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UCutPlaneMenuBlockExport::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(CheckBoxHorizontal && CheckBoxVertical && CheckBoxOther && CheckBoxAll && ButtonExport && ButtonCancel &&
		LimitedExportBox && LimitedExportText && UpgradeText))
	{
		return false;
	}

	CheckBoxHorizontal->OnCheckStateChanged.AddDynamic(this, &UCutPlaneMenuBlockExport::OnCheckBoxHorizontalChanged);
	CheckBoxVertical->OnCheckStateChanged.AddDynamic(this, &UCutPlaneMenuBlockExport::OnCheckBoxVerticalChanged);
	CheckBoxOther->OnCheckStateChanged.AddDynamic(this, &UCutPlaneMenuBlockExport::OnCheckBoxOtherChanged);
	CheckBoxAll->OnCheckStateChanged.AddDynamic(this, &UCutPlaneMenuBlockExport::OnCheckBoxAllChanged);
	ButtonExport->ModumateButton->OnReleased.AddDynamic(this, &UCutPlaneMenuBlockExport::OnButtonExportReleased);
	ButtonCancel->ModumateButton->OnReleased.AddDynamic(this, &UCutPlaneMenuBlockExport::OnButtonCancelReleased);

	return true;
}

void UCutPlaneMenuBlockExport::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController>();
}

void UCutPlaneMenuBlockExport::OnCheckBoxHorizontalChanged(bool IsChecked)
{
	UpdateCutPlaneExportableByType(ECutPlaneType::Horizontal, IsChecked);
}

void UCutPlaneMenuBlockExport::OnCheckBoxVerticalChanged(bool IsChecked)
{
	UpdateCutPlaneExportableByType(ECutPlaneType::Vertical, IsChecked);
}

void UCutPlaneMenuBlockExport::OnCheckBoxOtherChanged(bool IsChecked)
{
	UpdateCutPlaneExportableByType(ECutPlaneType::Other, IsChecked);
}

void UCutPlaneMenuBlockExport::OnCheckBoxAllChanged(bool IsChecked)
{
	UpdateCutPlaneExportableByType(ECutPlaneType::None, IsChecked);
}

void UCutPlaneMenuBlockExport::OnButtonExportReleased()
{
	AEditModelGameState* gameState = Cast<AEditModelGameState>(GetWorld()->GetGameState());
	if (!gameState)
	{
		return;
	}
	TArray<int32> cutPlaneIds;
	if (!Controller->EditModelUserWidget->CutPlaneMenu->GetCutPlaneIDsByType(ECutPlaneType::None, cutPlaneIds) || cutPlaneIds.Num() == 0)
	{
		return;
	}

	auto newDelta = MakeShared<FMOIDelta>();

	// Get all CutPlanes regardless of type by specifying ECutPlaneType::None
	for (auto curID : cutPlaneIds)
	{
		UCutPlaneDimListItemObject* item = Controller->EditModelUserWidget->CutPlaneMenu->GetListItemFromObjID(curID);
		if (item)
		{
			AModumateObjectInstance* moi = gameState->Document->GetObjectById(curID);
			auto& newStateData = newDelta->AddMutationState(moi);

			FMOICutPlaneData newCutPlaneData;
			newStateData.CustomData.LoadStructData(newCutPlaneData);
			newCutPlaneData.bIsExported = item->CanExport;

			newStateData.CustomData.SaveStructData<FMOICutPlaneData>(newCutPlaneData);
		}
	}
	gameState->Document->ApplyDeltas({ newDelta }, GetWorld());

	static const FString analyticsEventName(TEXT("ExportDWG"));
	UModumateAnalyticsStatics::RecordEventSimple(Controller, EModumateAnalyticsCategory::View, analyticsEventName);

	bool bStartedExportingDWGs = Controller->OnCreateDwg();

	// TODO: we don't have a callback for when all of the DWG requests have come back, so for now assume that the request when through,
	// and for limited exports we'll want to update the status of the request button and text boxes.
	if (bStartedExportingDWGs && bExportsLimited && (ExportsRemaining > 0))
	{
		ExportsRemaining--;
		UpdateCategoryCheckboxes();
	}
}

void UCutPlaneMenuBlockExport::OnButtonCancelReleased()
{
	// Update CutPlaneMenu without updating its MOIs will revert list items back to their original bIsExported state
	Controller->EditModelUserWidget->CutPlaneMenu->UpdateCutPlaneMenuBlocks();
	SetExportMenuVisibility(false);
}

void UCutPlaneMenuBlockExport::SetExportMenuVisibility(bool NewVisible)
{
	if (NewVisible && !IsVisible())
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		CalculateRemainingExports();
		UpdateExportMenu();
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCutPlaneMenuBlockExport::UpdateExportMenu()
{
	ListHorizontal->ClearListItems();
	ListVertical->ClearListItems();
	ListOther->ClearListItems();

	// Export menu is similar to its parent CutPlaneMenu. Since the only difference is to show bIsExported, they can use the same list items.
	for (auto curItem : Controller->EditModelUserWidget->CutPlaneMenu->CutPlaneMenuBlockHorizontal->CutPlanesList->GetListItems())
	{
		ListHorizontal->AddItem(curItem);
	}
	for (auto curItem : Controller->EditModelUserWidget->CutPlaneMenu->CutPlaneMenuBlockVertical->CutPlanesList->GetListItems())
	{
		ListVertical->AddItem(curItem);
	}
	for (auto curItem : Controller->EditModelUserWidget->CutPlaneMenu->CutPlaneMenuBlockOther->CutPlanesList->GetListItems())
	{
		ListOther->AddItem(curItem);
	}

	UpdateCategoryCheckboxes();
}

void UCutPlaneMenuBlockExport::UpdateCategoryCheckboxes()
{
	int32 numCanExportHorizontal = 0;
	int32 numCannotExportHorizontal = 0;
	int32 numCanExportVertical = 0;
	int32 numCannotExportVertical = 0;
	int32 numCanExportOther = 0;
	int32 numCannotExportOther = 0;

	TArray<int32> cutPlaneIDs;
	Controller->EditModelUserWidget->CutPlaneMenu->GetCutPlaneIDsByType(ECutPlaneType::None, cutPlaneIDs);

	for (auto curId : cutPlaneIDs)
	{
		UCutPlaneDimListItemObject* item = Controller->EditModelUserWidget->CutPlaneMenu->GetListItemFromObjID(curId);
		if (item)
		{
			switch (item->CutPlaneType)
			{
			case ECutPlaneType::Horizontal:
				item->CanExport ? numCanExportHorizontal++ : numCannotExportHorizontal++;
				break;
			case ECutPlaneType::Vertical:
				item->CanExport ? numCanExportVertical++ : numCannotExportVertical++;
				break;
			case ECutPlaneType::Other:
				item->CanExport ? numCanExportOther++ : numCannotExportOther++;
				break;
			default:
				break;
			}
		}
	}

	// Export horizontal condition
	if (numCanExportHorizontal == 0)
	{
		CheckBoxHorizontal->SetCheckedState(ECheckBoxState::Unchecked);
	}
	else if (numCannotExportHorizontal == 0)
	{
		CheckBoxHorizontal->SetCheckedState(ECheckBoxState::Checked);
	}
	else
	{
		CheckBoxHorizontal->SetCheckedState(ECheckBoxState::Undetermined);
	}

	// Export vertical condition
	if (numCanExportVertical == 0)
	{
		CheckBoxVertical->SetCheckedState(ECheckBoxState::Unchecked);
	}
	else if (numCannotExportVertical == 0)
	{
		CheckBoxVertical->SetCheckedState(ECheckBoxState::Checked);
	}
	else
	{
		CheckBoxVertical->SetCheckedState(ECheckBoxState::Undetermined);
	}

	// Export others condition
	if (numCanExportOther == 0)
	{
		CheckBoxOther->SetCheckedState(ECheckBoxState::Unchecked);
	}
	else if (numCannotExportOther == 0)
	{
		CheckBoxOther->SetCheckedState(ECheckBoxState::Checked);
	}
	else
	{
		CheckBoxOther->SetCheckedState(ECheckBoxState::Undetermined);
	}

	// Export all Condition
	if (numCanExportHorizontal == 0 && numCanExportVertical == 0 && numCanExportOther == 0)
	{
		CheckBoxAll->SetCheckedState(ECheckBoxState::Unchecked);
	}
	else if (numCannotExportHorizontal == 0 && numCannotExportVertical == 0 && numCannotExportOther == 0)
	{
		CheckBoxAll->SetCheckedState(ECheckBoxState::Checked);
	}
	else
	{
		CheckBoxAll->SetCheckedState(ECheckBoxState::Undetermined);
	}

	int32 totalNum = numCanExportHorizontal + numCanExportVertical + numCanExportOther;
	FText cutPlaneExportFormat = (totalNum == 0) ?
		LOCTEXT("CutPlaneExportButtonNone", "No Cut Planes Selected") :
		LOCTEXT("CutPlaneExportButtonSome", "Export ({0}) Cut Planes");
	FText cutPlaneNumberText = FText::Format(cutPlaneExportFormat, FText::AsNumber(totalNum));
	ButtonExport->SetIsEnabled(bExportsEnabled && (totalNum > 0));
	ButtonExport->ButtonText->SetText(cutPlaneNumberText);

	if (bCalculatingExports)
	{
		LimitedExportBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		LimitedExportText->ModumateTextBlock->SetText(LOCTEXT("CutPlaneExportsTextCalculating", "Calculating remaining exports."));
		UpgradeText->SetVisibility(ESlateVisibility::Collapsed);
	}
	else if (bExportsLimited)
	{
		LimitedExportBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UpgradeText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		if (bExportsEnabled)
		{
			LimitedExportText->ModumateTextBlock->SetText(FText::Format(
				LOCTEXT("CutPlaneExportsTextSome", "This will use {0} of {1} remaining exports."), 1, ExportsRemaining));
		}
		else
		{
			LimitedExportText->ModumateTextBlock->SetText(LOCTEXT("CutPlaneExportsTextNone", "You have no exports remaining."));
		}
	}
	else if (bExportsEnabled)
	{
		LimitedExportBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		LimitedExportBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		LimitedExportText->ModumateTextBlock->SetText(LOCTEXT("CutPlaneExportsTextError", "Exports are unavailable."));
		UpgradeText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCutPlaneMenuBlockExport::UpdateCutPlaneExportable(const TArray<int32>& IDs, bool NewExportable)
{
	for (auto curID : IDs)
	{
		UCutPlaneDimListItemObject* item = Controller->EditModelUserWidget->CutPlaneMenu->GetListItemFromObjID(curID);
		if (item)
		{
			item->CanExport = NewExportable;
		}
	}
	// Ask CutPlaneExportListItem widget to rebuild
	ListHorizontal->RegenerateAllEntries();
	ListVertical->RegenerateAllEntries();
	ListOther->RegenerateAllEntries();

	UpdateCategoryCheckboxes();
}

void UCutPlaneMenuBlockExport::UpdateCutPlaneExportableByType(ECutPlaneType Type, bool NewExportable)
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller && controller->EditModelUserWidget)
	{
		TArray<int32> idsToChange;
		Controller->EditModelUserWidget->CutPlaneMenu->GetCutPlaneIDsByType(Type, idsToChange);
		UpdateCutPlaneExportable(idsToChange, NewExportable);
	}
}

void UCutPlaneMenuBlockExport::CalculateRemainingExports()
{
	auto gameInstance = Controller ? Controller->GetGameInstance<UModumateGameInstance>() : nullptr;
	auto accountManager = gameInstance ? gameInstance->GetAccountManager() : nullptr;
	if (!accountManager.IsValid())
	{
		return;
	}

	UpgradeText->SetText(Controller->EditModelUserWidget->GetPlanUpgradeRichText());
	UpgradeText->SetVisibility(ESlateVisibility::Collapsed);

	bExportsLimited = false;
	bExportsEnabled = false;
	ExportsRemaining = 0;

	auto weakThis = MakeWeakObjectPtr<UCutPlaneMenuBlockExport>(this);
	bCalculatingExports = accountManager->RequestServiceRemaining(FModumateAccountManager::ServiceDwg,
		[weakThis](FString ServiceName, bool bSuccess, bool bLimited, int32 Remaining)
		{
			if (!weakThis.IsValid())
			{
				return;
			}

			weakThis->bCalculatingExports = false;
			weakThis->bExportsLimited = bLimited;
			weakThis->bExportsEnabled = bSuccess && (!bLimited || (Remaining > 0));
			weakThis->ExportsRemaining = Remaining;
			weakThis->UpdateExportMenu();
		});
}

#undef LOCTEXT_NAMESPACE