// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/GeneralListItem.h"
#include "UI/RightMenu/GeneralListItemObject.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Components/CheckBox.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/TooltipManager.h"
#include "Objects/CutPlane.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateCheckBox.h"


UGeneralListItem::UGeneralListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UGeneralListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!(CheckBoxVisibility && ButtonEdit && TextTitleEditable))
	{
		return false;
	}
	CheckBoxVisibility->OnCheckStateChanged.AddDynamic(this, &UGeneralListItem::OnCheckBoxVisibilityChanged);
	CheckBoxVisibility->ToolTipWidgetDelegate.BindDynamic(this, &UGeneralListItem::OnCheckBoxTooltipWidget);
	ButtonEdit->ModumateButton->OnReleased.AddDynamic(this, &UGeneralListItem::OnButtonEditReleased);
	TextTitleEditable->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UGeneralListItem::OnEditableTitleCommitted);
	CheckBoxCullModel->OnCheckStateChanged.AddDynamic(this, &UGeneralListItem::OnCheckBoxCullModelChanged);
	CheckBoxCullModel->ToolTipWidgetDelegate.BindDynamic(this, &UGeneralListItem::OnCheckBoxCullModelTooltipWidget);

	return true;
}

void UGeneralListItem::NativeConstruct()
{
	Super::NativeConstruct();
}

void UGeneralListItem::OnButtonMainReleased()
{
	// TODO: Cut plane action on button released
}

void UGeneralListItem::OnCheckBoxVisibilityChanged(bool IsChecked)
{
	AEditModelGameState *gameState = Cast<AEditModelGameState>(GetWorld()->GetGameState());
	if (gameState)
	{
		TArray<int32> objIDs = { ObjID };
		if (IsChecked)
		{
			gameState->Document->AddHideObjectsById(GetWorld(), objIDs);
		}
		else
		{
			gameState->Document->UnhideObjectsById(GetWorld(), objIDs);
		}
	}

}

void UGeneralListItem::OnEditableTitleCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Lose focus if this commit is not entering its value
	if (!(CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus))
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		return;
	}

	// TODO: Detect if new name already exist

	AEditModelGameState *gameState = Cast<AEditModelGameState>(GetWorld()->GetGameState());
	if (gameState)
	{
		AModumateObjectInstance* moi = gameState->Document->GetObjectById(ObjID);
		FMOIStateData oldStateData = moi->GetStateData();
		FMOIStateData newStateData = oldStateData;

		FMOICutPlaneData newCutPlaneData;
		newStateData.CustomData.LoadStructData(newCutPlaneData);
		newCutPlaneData.Name = TextTitleEditable->ModumateEditableTextBox->Text.ToString();

		newStateData.CustomData.SaveStructData<FMOICutPlaneData>(newCutPlaneData);
		auto delta = MakeShared<FMOIDelta>();
		delta->AddMutationState(moi, oldStateData, newStateData);

		gameState->Document->ApplyDeltas({ delta }, GetWorld());
	}
}

void UGeneralListItem::OnCheckBoxCullModelChanged(bool IsChecked)
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller)
	{
		controller->SetCurrentCullingCutPlane(IsChecked ? ObjID: MOD_ID_NONE);
	}
}

void UGeneralListItem::OnButtonEditReleased()
{
	TextTitleEditable->ModumateEditableTextBox->SetKeyboardFocus();
}

void UGeneralListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UGeneralListItemObject *cutPlaneItemObject = Cast<UGeneralListItemObject>(ListItemObject);
	if (!cutPlaneItemObject)
	{
		return;
	}
	UpdateVisibilityAndName(cutPlaneItemObject->Visibility, cutPlaneItemObject->DisplayName);
	ObjID = cutPlaneItemObject->ObjId;
	CheckBoxCullModel->SetCheckedState(
		cutPlaneItemObject->bIsCulling ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

	switch (cutPlaneItemObject->CutPlaneType)
	{
	case EGeneralListType::Horizontal:
		BuildAsHorizontalCutPlaneItem(cutPlaneItemObject->Location);
		break;
	case EGeneralListType::Vertical:
		BuildAsVerticalCutPlaneItem(cutPlaneItemObject->Rotation);
		break;
	case EGeneralListType::Other:
	case EGeneralListType::Terrain:
	default:
		TextDimension->ChangeText(FText::GetEmpty());
		break;
	}
	
}

void UGeneralListItem::BuildAsVerticalCutPlaneItem(const FQuat& Rotation)
{
	// TODO: This will be in static function for converting from ue4 axis to Modumate axis
	int32 degree = AMOICutPlane::GetCutPlaneVerticalDegree(Rotation);
	TextDimension->ChangeText(FText::AsNumber(degree));
}

void UGeneralListItem::BuildAsHorizontalCutPlaneItem(const FVector& Location)
{
	TextDimension->ChangeText(UModumateDimensionStatics::CentimetersToDisplayText(Location.Z));
}

void UGeneralListItem::UpdateVisibilityAndName(bool NewVisible, const FString& NewName)
{
	CheckBoxVisibility->SetIsChecked(!NewVisible);
	TextTitleEditable->ChangeText(FText::FromString(NewName));
}

UWidget* UGeneralListItem::OnCheckBoxTooltipWidget()
{
	return UTooltipManager::GenerateTooltipNonInputWidget(TooltipID_CheckBoxVisibility, this);
}

UWidget* UGeneralListItem::OnCheckBoxCullModelTooltipWidget()
{
	return UTooltipManager::GenerateTooltipNonInputWidget(TooltipID_CheckBoxCullModel, this);
}

