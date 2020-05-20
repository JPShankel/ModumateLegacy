// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/UI/RoofPerimeterPropertiesWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"


using namespace Modumate;

URoofPerimeterPropertiesWidget::URoofPerimeterPropertiesWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool URoofPerimeterPropertiesWidget::Initialize()
{
	if (!Super::Initialize() ||
		!ensure(RootBorder && RootVerticalBox && ControlGabled && ControlFlat &&
		SlopeEditorFraction && SlopeEditorDegrees && OverhangEditor && ButtonCommit && ButtonCancel))
	{
		return false;
	}

	ControlGabled->OnCheckStateChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnControlChangedGabled);
	ControlFlat->OnCheckStateChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnControlChangedFlat);

	return true;
}

void URoofPerimeterPropertiesWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void URoofPerimeterPropertiesWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UWorld *world = GetWorld();
	AEditModelPlayerController_CPP *controller = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
	AEditModelGameState_CPP *gameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
	const FModumateObjectInstance *targetObj = gameState ? gameState->Document.GetObjectById(TargetObjID) : nullptr;

	FVector2D targetScreenPosition;
	if (controller && targetObj && controller->ProjectWorldLocationToScreen(targetObj->GetObjectLocation(), targetScreenPosition))
	{
		FVector2D widgetSize = MyGeometry.GetAbsoluteSize();
		FVector2D widgetOffset(-0.5f * widgetSize.X, -widgetSize.Y - 16.0f);
		SetPositionInViewport(targetScreenPosition + widgetOffset);
	}
}

void URoofPerimeterPropertiesWidget::OnControlChangedGabled(bool bIsChecked)
{
	if (bIsChecked && ControlFlat && (ControlFlat->GetCheckedState() == ECheckBoxState::Checked))
	{
		ControlFlat->SetCheckedState(ECheckBoxState::Unchecked);
	}
}

void URoofPerimeterPropertiesWidget::OnControlChangedFlat(bool bIsChecked)
{
	if (bIsChecked && ControlGabled && (ControlGabled->GetCheckedState() == ECheckBoxState::Checked))
	{
		ControlGabled->SetCheckedState(ECheckBoxState::Unchecked);
	}
}
