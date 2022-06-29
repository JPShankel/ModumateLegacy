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
#include "UI/RightMenu/GeneralListItemObject.h"
#include "UI/RightMenu/GeneralListItemMenuBlock.h"
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
	UpdateCutPlaneExportableByType(EGeneralListType::Horizontal, IsChecked);
}

void UCutPlaneMenuBlockExport::OnCheckBoxVerticalChanged(bool IsChecked)
{
	UpdateCutPlaneExportableByType(EGeneralListType::Vertical, IsChecked);
}

void UCutPlaneMenuBlockExport::OnCheckBoxOtherChanged(bool IsChecked)
{
	UpdateCutPlaneExportableByType(EGeneralListType::Other, IsChecked);
}

void UCutPlaneMenuBlockExport::OnCheckBoxAllChanged(bool IsChecked)
{
	UpdateCutPlaneExportableByType(EGeneralListType::None, IsChecked);
}

void UCutPlaneMenuBlockExport::OnButtonExportReleased()
{
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
	for (auto curItem : Controller->EditModelUserWidget->CutPlaneMenu->CutPlaneMenuBlockHorizontal->GeneralItemsList->GetListItems())
	{
		ListHorizontal->AddItem(curItem);
	}
	for (auto curItem : Controller->EditModelUserWidget->CutPlaneMenu->CutPlaneMenuBlockVertical->GeneralItemsList->GetListItems())
	{
		ListVertical->AddItem(curItem);
	}
	for (auto curItem : Controller->EditModelUserWidget->CutPlaneMenu->CutPlaneMenuBlockOther->GeneralItemsList->GetListItems())
	{
		ListOther->AddItem(curItem);
	}

	UpdateCategoryCheckboxes();
}

void UCutPlaneMenuBlockExport::UpdateCategoryCheckboxes()
{
}

void UCutPlaneMenuBlockExport::UpdateCutPlaneExportable(const TArray<int32>& IDs, bool NewExportable)
{
}

void UCutPlaneMenuBlockExport::UpdateCutPlaneExportableByType(EGeneralListType Type, bool NewExportable)
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