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
#include "Objects/Terrain.h"
#include "ModumateCore/ModumateFunctionLibrary.h"


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
	if (!(CheckBoxVisibility && ButtonEdit && TextTitleEditable && ButtonFlip))
	{
		return false;
	}
	CheckBoxVisibility->OnCheckStateChanged.AddDynamic(this, &UGeneralListItem::OnCheckBoxVisibilityChanged);
	CheckBoxVisibility->ToolTipWidgetDelegate.BindDynamic(this, &UGeneralListItem::OnCheckBoxTooltipWidget);
	ButtonEdit->ModumateButton->OnReleased.AddDynamic(this, &UGeneralListItem::OnButtonEditReleased);
	TextTitleEditable->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UGeneralListItem::OnEditableTitleCommitted);
	CheckBoxCullModel->OnCheckStateChanged.AddDynamic(this, &UGeneralListItem::OnCheckBoxCullModelChanged);
	CheckBoxCullModel->ToolTipWidgetDelegate.BindDynamic(this, &UGeneralListItem::OnCheckBoxCullModelTooltipWidget);
	ButtonFlip->ModumateButton->OnReleased.AddDynamic(this, &UGeneralListItem::OnButtonFlipReleased);

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
		// For terrain
		if (CurrentGeneralListType == EGeneralListType::Terrain)
		{
			AModumateObjectInstance* moi = gameState->Document->GetObjectById(ObjID);
			
			if (ensure(moi))
			{
				TArray<AModumateObjectInstance*> terrainMOIs = { moi };
				UModumateFunctionLibrary::SetMOIAndDescendentsHidden(terrainMOIs, IsChecked);
			}
		}
		else // For cutplanes
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

		if (CurrentGeneralListType == EGeneralListType::Terrain)
		{
			FMOITerrainData newTerrainData;
			newStateData.CustomData.LoadStructData(newTerrainData);
			newTerrainData.Name = TextTitleEditable->ModumateEditableTextBox->Text.ToString();
			newStateData.CustomData.SaveStructData<FMOITerrainData>(newTerrainData);
		}
		else
		{
			FMOICutPlaneData newCutPlaneData;
			newStateData.CustomData.LoadStructData(newCutPlaneData);
			newCutPlaneData.Name = TextTitleEditable->ModumateEditableTextBox->Text.ToString();
			newStateData.CustomData.SaveStructData<FMOICutPlaneData>(newCutPlaneData);
		}

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
		if (CurrentGeneralListType == EGeneralListType::Terrain)
		{
			AModumateObjectInstance* moi = controller->GetDocument()->GetObjectById(ObjID);
			if (moi)
			{
				AMOITerrain* terrainMOi = Cast<AMOITerrain>(moi);
				if (terrainMOi)
				{
					terrainMOi->SetIsTranslucent(IsChecked);
				}
			}
		}
		else
		{
			controller->SetCurrentCullingCutPlane(IsChecked ? ObjID : MOD_ID_NONE);
		}
	}
}

void UGeneralListItem::OnButtonEditReleased()
{
	TextTitleEditable->ModumateEditableTextBox->SetKeyboardFocus();
}

void UGeneralListItem::OnButtonFlipReleased()
{
	AEditModelGameState* gameState = Cast<AEditModelGameState>(GetWorld()->GetGameState());
	if (gameState)
	{
		AModumateObjectInstance* moi = gameState->Document->GetObjectById(ObjID);
		FMOIStateData oldStateData = moi->GetStateData();
		FMOIStateData newStateData = oldStateData;

		FMOICutPlaneData newCutPlaneData;
		newStateData.CustomData.LoadStructData(newCutPlaneData);
		FQuat newQuat = FRotationMatrix::MakeFromXY(newCutPlaneData.Rotation.GetForwardVector() * -1.f, newCutPlaneData.Rotation.GetRightVector()).ToQuat();
		newCutPlaneData.Rotation = newQuat;
		newStateData.CustomData.SaveStructData<FMOICutPlaneData>(newCutPlaneData);

		auto delta = MakeShared<FMOIDelta>();
		delta->AddMutationState(moi, oldStateData, newStateData);
		gameState->Document->ApplyDeltas({ delta }, GetWorld());
	}
}

void UGeneralListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UGeneralListItemObject* generalItemObject = Cast<UGeneralListItemObject>(ListItemObject);
	if (!generalItemObject)
	{
		CurrentGeneralListType = EGeneralListType::None;
		return;
	}

	UpdateVisibilityAndName(generalItemObject->Visibility, generalItemObject->DisplayName);
	ObjID = generalItemObject->ObjId;

	CheckBoxCullModel->SetCheckedState(
		generalItemObject->bIsCulling ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

	CurrentGeneralListType = generalItemObject->CutPlaneType;
	// Only cutplane can be flipped
	ButtonFlip->SetVisibility(CurrentGeneralListType == EGeneralListType::Terrain ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	
	switch (generalItemObject->CutPlaneType)
	{
	case EGeneralListType::Horizontal:
		BuildAsHorizontalCutPlaneItem(generalItemObject->Location);
		break;
	case EGeneralListType::Vertical:
		BuildAsVerticalCutPlaneItem(generalItemObject->Rotation);
		break;
	case EGeneralListType::Terrain:
		BuildAsTerrainItem(generalItemObject);
		break;
	case EGeneralListType::Other:
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

void UGeneralListItem::BuildAsTerrainItem(const class UGeneralListItemObject* TerrainItemObject)
{
	TextDimension->ChangeText(FText::GetEmpty());
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

