// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneDimListItem.h"
#include "UI/RightMenu/CutPlaneDimListItemObject.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Components/CheckBox.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/TooltipManager.h"


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
	if (!CheckBoxVisibility)
	{
		return false;
	}
	CheckBoxVisibility->OnCheckStateChanged.AddDynamic(this, &UCutPlaneDimListItem::OnCheckBoxVisibilityChanged);
	CheckBoxVisibility->ToolTipWidgetDelegate.BindDynamic(this, &UCutPlaneDimListItem::OnCheckBoxTooltipWidget);

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

void UCutPlaneDimListItem::OnButtonSaveReleased()
{
	// TODO: Export cut plane
}

void UCutPlaneDimListItem::OnCheckBoxVisibilityChanged(bool IsChecked)
{
	AEditModelGameState_CPP *gameState = Cast<AEditModelGameState_CPP>(GetWorld()->GetGameState());
	if (gameState)
	{
		TArray<int32> objIDs = { ObjID };
		if (IsChecked)
		{
			gameState->Document.AddHideObjectsById(GetWorld(), objIDs);
		}
		else
		{
			gameState->Document.UnhideObjectsById(GetWorld(), objIDs);
		}
	}

}

void UCutPlaneDimListItem::OnButtonEditReleased()
{
	// TODO: Edit cut plane name
}

void UCutPlaneDimListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UCutPlaneDimListItemObject *cutPlaneItemObject = Cast<UCutPlaneDimListItemObject>(ListItemObject);
	if (!cutPlaneItemObject)
	{
		return;
	}

	UpdateCheckBoxVisibility(cutPlaneItemObject->Visibility);
	ObjID = cutPlaneItemObject->ObjId;
	TextTitle->ChangeText(FText::FromString(cutPlaneItemObject->DisplayName));
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

void UCutPlaneDimListItem::BuildAsVerticalCutPlaneItem(const FQuat &Rotation)
{
	float angle = Rotation.GetAngle();
	TextDimension->ChangeText(FText::AsNumber(angle));
}

void UCutPlaneDimListItem::BuildAsHorizontalCutPlaneItem(const FVector &Location)
{
	TArray<int32> imperialsInches;
	UModumateDimensionStatics::CentimetersToImperialInches(Location.Z, imperialsInches);
	TextDimension->ChangeText(UModumateDimensionStatics::ImperialInchesToDimensionStringText(imperialsInches));
}

void UCutPlaneDimListItem::UpdateCheckBoxVisibility(bool NewVisible)
{
	CheckBoxVisibility->SetIsChecked(!NewVisible);
}

UWidget* UCutPlaneDimListItem::OnCheckBoxTooltipWidget()
{
	return UTooltipManager::GenerateTooltipNonInputWidget(TooltipID_CheckBoxVisibility, this);
}

