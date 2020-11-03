// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/CutPlaneExportListItem.h"
#include "UI/RightMenu/CutPlaneDimListItemObject.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UnrealClasses/TooltipManager.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateCheckBox.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "UI/RightMenu/CutPlaneMenuBlockExport.h"


UCutPlaneExportListItem::UCutPlaneExportListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UCutPlaneExportListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!ModumateButtonMain)
	{
		return false;
	}
	ToolTipWidgetDelegate.BindDynamic(this, &UCutPlaneExportListItem::OnTooltipWidget);
	ModumateButtonMain->OnReleased.AddDynamic(this, &UCutPlaneExportListItem::OnButtonMainReleased);

	return true;
}

void UCutPlaneExportListItem::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCutPlaneExportListItem::OnButtonMainReleased()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller && controller->EditModelUserWidget)
	{
		bool newExportState = CheckBoxExport->GetCheckedState() != ECheckBoxState::Checked;

		controller->EditModelUserWidget->CutPlaneMenu->CutPlaneMenuBlockExport->UpdateCutPlaneExportable(TArray<int32>{ObjID}, newExportState);
	}
}

void UCutPlaneExportListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UCutPlaneDimListItemObject *cutPlaneItemObject = Cast<UCutPlaneDimListItemObject>(ListItemObject);
	if (!cutPlaneItemObject)
	{
		return;
	}

	TextTitle->ChangeText(FText::FromString(cutPlaneItemObject->DisplayName));
	ObjID = cutPlaneItemObject->ObjId;
	CheckBoxExport->SetCheckedState(cutPlaneItemObject->CanExport ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

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

void UCutPlaneExportListItem::BuildAsVerticalCutPlaneItem(const FQuat& Rotation)
{
	float angle = Rotation.GetAngle();
	TextDimension->ChangeText(FText::AsNumber(angle));
}

void UCutPlaneExportListItem::BuildAsHorizontalCutPlaneItem(const FVector& Location)
{
	TArray<int32> imperialsInches;
	UModumateDimensionStatics::CentimetersToImperialInches(Location.Z, imperialsInches);
	TextDimension->ChangeText(UModumateDimensionStatics::ImperialInchesToDimensionStringText(imperialsInches));
}

UWidget* UCutPlaneExportListItem::OnTooltipWidget()
{
	return UTooltipManager::GenerateTooltipNonInputWidget(TooltipID, this);
}

