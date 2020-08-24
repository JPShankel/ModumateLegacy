// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneDimListItem.h"
#include "UI/RightMenu/CutPlaneDimListItemObject.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "ModumateCore/ModumateDimensionStatics.h"


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

	TextTitle->ChangeText(FText::FromString(cutPlaneItemObject->DisplayName));
	switch (cutPlaneItemObject->CutPlaneType)
	{
	case ECutPlaneType::Horizontal:
		BuildAsHorizontalCutPlaneItem(cutPlaneItemObject->Rotation);
		break;
	case ECutPlaneType::Vertical:
		BuildAsVerticalCutPlaneItem(cutPlaneItemObject->Location);
		break;
	case ECutPlaneType::Other:
	default:
		TextDimension->ChangeText(FText::GetEmpty());
		break;
	}
	
}

void UCutPlaneDimListItem::BuildAsVerticalCutPlaneItem(const FVector &Location)
{
	TArray<int32> imperialsInches;
	UModumateDimensionStatics::CentimetersToImperialInches(Location.Z, imperialsInches);
	TextDimension->ChangeText(UModumateDimensionStatics::ImperialInchesToDimensionStringText(imperialsInches));
}

void UCutPlaneDimListItem::BuildAsHorizontalCutPlaneItem(const FQuat &Rotation)
{
	float angle = Rotation.GetAngle();
	TextDimension->ChangeText(FText::AsNumber(angle));
}

