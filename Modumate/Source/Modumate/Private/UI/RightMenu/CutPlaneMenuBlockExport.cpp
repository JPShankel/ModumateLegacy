// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneMenuBlockExport.h"
#include "Components/ListView.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/EditModelUserWidget.h"
#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UI/RightMenu/CutPlaneMenuBlock.h"
#include "UI/RightMenu/CutPlaneDimListItemObject.h"
#include "UI/Custom/ModumateCheckBox.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Objects/CutPlane.h"

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
	if (!(CheckBoxHorizontal && CheckBoxVertical && CheckBoxOther && CheckBoxAll && ButtonExport && ButtonCancel))
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
	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
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
	AEditModelGameState_CPP* gameState = Cast<AEditModelGameState_CPP>(GetWorld()->GetGameState());
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
			FModumateObjectInstance* moi = gameState->Document.GetObjectById(curID);
			auto& newStateData = newDelta->AddMutationState(moi);

			FMOICutPlaneData newCutPlaneData;
			newStateData.CustomData.LoadStructData(newCutPlaneData);
			newCutPlaneData.bIsExported = item->CanExport;

			newStateData.CustomData.SaveStructData<FMOICutPlaneData>(newCutPlaneData);
		}
	}
	gameState->Document.ApplyDeltas({ newDelta }, GetWorld());

	Controller->OnCreateDwg();
}

void UCutPlaneMenuBlockExport::OnButtonCancelReleased()
{
	// Update CutPlaneMenu without updating its MOIs will revert list items back to their original bIsExported state
	Controller->EditModelUserWidget->CutPlaneMenu->UpdateCutPlaneMenuBlocks();
	SetExportMenuVisibility(false);
}

void UCutPlaneMenuBlockExport::SetExportMenuVisibility(bool NewVisible)
{
	if (NewVisible)
	{
		this->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UpdateExportMenu();
	}
	else
	{
		this->SetVisibility(ESlateVisibility::Collapsed);
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
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller && controller->EditModelUserWidget)
	{
		TArray<int32> idsToChange;
		Controller->EditModelUserWidget->CutPlaneMenu->GetCutPlaneIDsByType(Type, idsToChange);
		UpdateCutPlaneExportable(idsToChange, NewExportable);
	}
}
