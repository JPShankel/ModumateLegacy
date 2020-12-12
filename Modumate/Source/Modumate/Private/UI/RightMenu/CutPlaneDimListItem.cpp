// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneDimListItem.h"
#include "UI/RightMenu/CutPlaneDimListItemObject.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Components/CheckBox.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/TooltipManager.h"
#include "Objects/CutPlane.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/EditModelUserWidget.h"


UCutPlaneDimListItem::UCutPlaneDimListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UCutPlaneDimListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!(CheckBoxVisibility && ButtonEdit && TextTitleEditable))
	{
		return false;
	}
	CheckBoxVisibility->OnCheckStateChanged.AddDynamic(this, &UCutPlaneDimListItem::OnCheckBoxVisibilityChanged);
	CheckBoxVisibility->ToolTipWidgetDelegate.BindDynamic(this, &UCutPlaneDimListItem::OnCheckBoxTooltipWidget);
	ButtonEdit->ModumateButton->OnReleased.AddDynamic(this, &UCutPlaneDimListItem::OnButtonEditReleased);
	TextTitleEditable->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UCutPlaneDimListItem::OnEditableTitleCommitted);

	return true;
}

void UCutPlaneDimListItem::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCutPlaneDimListItem::OnButtonMainReleased()
{
	// TODO: Cut plane action on button released
}

void UCutPlaneDimListItem::OnCheckBoxVisibilityChanged(bool IsChecked)
{
	AEditModelGameState_CPP *gameState = Cast<AEditModelGameState_CPP>(GetWorld()->GetGameState());
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

void UCutPlaneDimListItem::OnEditableTitleCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Lose focus if this commit is not entering its value
	if (!(CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus))
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		return;
	}

	// TODO: Detect if new name already exist

	AEditModelGameState_CPP *gameState = Cast<AEditModelGameState_CPP>(GetWorld()->GetGameState());
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

void UCutPlaneDimListItem::OnButtonEditReleased()
{
	TextTitleEditable->ModumateEditableTextBox->SetKeyboardFocus();
}

void UCutPlaneDimListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UCutPlaneDimListItemObject *cutPlaneItemObject = Cast<UCutPlaneDimListItemObject>(ListItemObject);
	if (!cutPlaneItemObject)
	{
		return;
	}
	UpdateVisibilityAndName(cutPlaneItemObject->Visibility, cutPlaneItemObject->DisplayName);
	ObjID = cutPlaneItemObject->ObjId;

	switch (cutPlaneItemObject->CutPlaneType)
	{
	case ECutPlaneType::Horizontal:
		BuildAsHorizontalCutPlaneItem(cutPlaneItemObject->Location);
		break;
	case ECutPlaneType::Vertical:
		BuildAsVerticalCutPlaneItem(cutPlaneItemObject->Rotation);
		break;
	case ECutPlaneType::Other:
	default:
		TextDimension->ChangeText(FText::GetEmpty());
		break;
	}
	
}

void UCutPlaneDimListItem::BuildAsVerticalCutPlaneItem(const FQuat& Rotation)
{
	float angle = Rotation.GetAngle();
	TextDimension->ChangeText(FText::AsNumber(angle));
}

void UCutPlaneDimListItem::BuildAsHorizontalCutPlaneItem(const FVector& Location)
{
	TArray<int32> imperialsInches;
	UModumateDimensionStatics::CentimetersToImperialInches(Location.Z, imperialsInches);
	TextDimension->ChangeText(UModumateDimensionStatics::ImperialInchesToDimensionStringText(imperialsInches));
}

void UCutPlaneDimListItem::UpdateVisibilityAndName(bool NewVisible, const FString& NewName)
{
	CheckBoxVisibility->SetIsChecked(!NewVisible);
	TextTitleEditable->ChangeText(FText::FromString(NewName));
}

UWidget* UCutPlaneDimListItem::OnCheckBoxTooltipWidget()
{
	return UTooltipManager::GenerateTooltipNonInputWidget(TooltipID_CheckBoxVisibility, this);
}

