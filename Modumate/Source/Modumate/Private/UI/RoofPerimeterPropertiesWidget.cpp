// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RoofPerimeterPropertiesWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"


using namespace Modumate;

URoofPerimeterPropertiesWidget::URoofPerimeterPropertiesWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool URoofPerimeterPropertiesWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(RootBorder && RootVerticalBox && ControlGabled && ControlHasFace &&
		SlopeEditorFraction && SlopeEditorDegrees && OverhangEditor && ButtonCommit && ButtonCancel))
	{
		return false;
	}

	ControlGabled->OnCheckStateChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnControlChangedGabled);
	ControlHasFace->OnCheckStateChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnControlChangedHasFace);
	SlopeEditorFraction->OnTextChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnSlopeEditorFractionTextChanged);
	SlopeEditorFraction->OnTextCommitted.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnSlopeEditorFractionTextCommitted);
	SlopeEditorDegrees->OnTextChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnSlopeEditorDegreesTextChanged);
	SlopeEditorDegrees->OnTextCommitted.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnSlopeEditorDegreesTextCommitted);
	ButtonCommit->OnPressed.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnButtonPressedCommit);
	ButtonCancel->OnPressed.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnButtonPressedCancel);

	return true;
}

void URoofPerimeterPropertiesWidget::SetTarget(int32 InTargetPerimeterID, int32 InTargetEdgeID, AAdjustmentHandleActor_CPP *InOwningHandle)
{
	TargetPerimeterID = InTargetPerimeterID;
	TargetEdgeID = InTargetEdgeID;
	OwningHandle = InOwningHandle;

	UWorld *world = GetWorld();
	GameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
	if (!ensure(GameState))
	{
		return;
	}

	if ((OwningHandle == nullptr) || !OwningHandle->IsInUse())
	{
		return;
	}

	const FModumateObjectInstance *targetPerimeter = GameState->Document.GetObjectById(TargetPerimeterID);
	if (!UModumateRoofStatics::GetEdgeProperties(targetPerimeter, TargetEdgeID, InitialProperties))
	{
		return;
	}

	CurrentProperties = InitialProperties;

	ControlGabled->SetCheckedState(CurrentProperties.bHasFace ? ECheckBoxState::Unchecked : ECheckBoxState::Checked);
	ControlHasFace->SetCheckedState(CurrentProperties.bHasFace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

	float slopeNumeratorValue = CurrentProperties.Slope * 12.0f;
	SlopeEditorFraction->SetText(FText::AsNumber(slopeNumeratorValue));

	float slopeDegreesValue = FMath::RadiansToDegrees(FMath::Atan(CurrentProperties.Slope));
	SlopeEditorDegrees->SetText(FText::AsNumber(slopeDegreesValue));

	OverhangEditor->SetIsEnabled(false);
	//OverhangEditor->SetText(FText::AsNumber(CurrentProperties.Overhang));
}

void URoofPerimeterPropertiesWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void URoofPerimeterPropertiesWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const FModumateObjectInstance *targetObj = GameState->Document.GetObjectById(FMath::Abs(TargetEdgeID));
	auto controller = GetOwningPlayer();

	FVector2D targetScreenPosition;
	if (targetObj && controller && controller->ProjectWorldLocationToScreen(targetObj->GetObjectLocation(), targetScreenPosition))
	{
		FVector2D widgetSize = MyGeometry.GetAbsoluteSize();
		FVector2D widgetOffset(-0.5f * widgetSize.X, -widgetSize.Y - 16.0f);
		SetPositionInViewport(targetScreenPosition + widgetOffset);
	}
}

void URoofPerimeterPropertiesWidget::OnControlChangedGabled(bool bIsChecked)
{
	if (bIsChecked && ControlHasFace && (ControlHasFace->GetCheckedState() == ECheckBoxState::Checked))
	{
		ControlHasFace->SetCheckedState(ECheckBoxState::Unchecked);
		SetEdgeFaceControl(!bIsChecked);
	}
}

void URoofPerimeterPropertiesWidget::OnControlChangedHasFace(bool bIsChecked)
{
	if (bIsChecked && ControlGabled && (ControlGabled->GetCheckedState() == ECheckBoxState::Checked))
	{
		ControlGabled->SetCheckedState(ECheckBoxState::Unchecked);
		SetEdgeFaceControl(bIsChecked);
	}
}

void URoofPerimeterPropertiesWidget::OnSlopeEditorFractionTextChanged(const FText& NewText)
{

}

void URoofPerimeterPropertiesWidget::OnSlopeEditorFractionTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		float numeratorValue;
		if (UModumateDimensionStatics::TryParseInputNumber(Text.ToString(), numeratorValue))
		{
			float slopeValue = numeratorValue / 12.0f;
			SetEdgeSlope(slopeValue);
		}
	}
}

void URoofPerimeterPropertiesWidget::OnSlopeEditorDegreesTextChanged(const FText& NewText)
{

}

void URoofPerimeterPropertiesWidget::OnSlopeEditorDegreesTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		float degreesValue;
		if (UModumateDimensionStatics::TryParseInputNumber(Text.ToString(), degreesValue))
		{
			float slopeValue = FMath::Tan(FMath::DegreesToRadians(degreesValue));
			SetEdgeSlope(slopeValue);
		}
	}
}

void URoofPerimeterPropertiesWidget::OnButtonPressedCommit()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller && (controller->InteractionHandle == OwningHandle))
	{
		// TODO: this is a sign that we need a better tool/handle API
		controller->OnLButtonDown();
	}
}

void URoofPerimeterPropertiesWidget::OnButtonPressedCancel()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller && (controller->InteractionHandle == OwningHandle))
	{
		controller->AbortUseTool();
	}
}

void URoofPerimeterPropertiesWidget::SetEdgeFaceControl(bool bNewHasFace)
{
	CurrentProperties.bOverridden = true;
	CurrentProperties.bHasFace = bNewHasFace;
	SaveEdgeProperties();
}

void URoofPerimeterPropertiesWidget::SetEdgeSlope(float SlopeValue)
{
	CurrentProperties.bOverridden = true;
	CurrentProperties.Slope = SlopeValue;
	SaveEdgeProperties();
}

void URoofPerimeterPropertiesWidget::SaveEdgeProperties()
{


	FModumateObjectInstance *targetObj = GameState->Document.GetObjectById(TargetPerimeterID);
	if (targetObj)
	{
		UModumateRoofStatics::SetEdgeProperties(targetObj, TargetEdgeID, CurrentProperties);
	}
}
