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
#include "Objects/MOIDelta.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"


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

	RootBorder->OnMouseButtonDownEvent.BindDynamic(this, &URoofPerimeterPropertiesWidget::OnClickedBorder);
	ControlGabled->OnCheckStateChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnControlChangedGabled);
	ControlHasFace->OnCheckStateChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnControlChangedHasFace);
	SlopeEditorFraction->OnTextChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnSlopeEditorFractionTextChanged);
	SlopeEditorFraction->OnTextCommitted.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnSlopeEditorFractionTextCommitted);
	SlopeEditorDegrees->OnTextChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnSlopeEditorDegreesTextChanged);
	SlopeEditorDegrees->OnTextCommitted.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnSlopeEditorDegreesTextCommitted);
	OverhangEditor->OnTextChanged.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnOverhangEditorTextChanged);
	OverhangEditor->OnTextCommitted.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnOverhangEditorTextCommitted);
	ButtonCommit->OnReleased.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnButtonPressedCommit);
	ButtonCancel->OnReleased.AddDynamic(this, &URoofPerimeterPropertiesWidget::OnButtonPressedCancel);

	return true;
}

void URoofPerimeterPropertiesWidget::SetTarget(int32 InTargetPerimeterID, FGraphSignedID InTargetEdgeID, AAdjustmentHandleActor* InOwningHandle)
{
	TargetPerimeterID = InTargetPerimeterID;
	TargetEdgeID = FMath::Abs(InTargetEdgeID);
	OwningHandle = InOwningHandle;

	UpdateEditFields();
}

void URoofPerimeterPropertiesWidget::UpdateEditFields()
{
	UWorld *world = GetWorld();
	GameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	if (!ensure(GameState))
	{
		return;
	}

	if ((OwningHandle == nullptr) || !OwningHandle->IsInUse())
	{
		return;
	}

	const AModumateObjectInstance *targetPerimeter = GameState->Document->GetObjectById(TargetPerimeterID);
	if (!ensure(targetPerimeter && targetPerimeter->GetStateData().CustomData.LoadStructData(TempInitialCombinedProperties)))
	{
		return;
	}

	InitialProperties = TempInitialCombinedProperties.EdgeProperties.FindRef(TargetEdgeID);
	CurrentProperties = InitialProperties;

	ControlGabled->SetCheckedState(CurrentProperties.bHasFace ? ECheckBoxState::Unchecked : ECheckBoxState::Checked);
	ControlHasFace->SetCheckedState(CurrentProperties.bHasFace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

	float slopeNumeratorValue = CurrentProperties.Slope * 12.0f;
	SlopeEditorFraction->SetText(FText::AsNumber(slopeNumeratorValue));

	float slopeDegreesValue = FMath::RadiansToDegrees(FMath::Atan(CurrentProperties.Slope));
	SlopeEditorDegrees->SetText(FText::AsNumber(slopeDegreesValue));

	// TODO: set the text to a formatted dimension, rather than the raw cm value
	OverhangEditor->SetText(FText::AsNumber(CurrentProperties.Overhang));
}

void URoofPerimeterPropertiesWidget::UpdateTransform()
{
	const AModumateObjectInstance *targetObj = GameState->Document->GetObjectById(TargetEdgeID);
	auto controller = GetOwningPlayer();

	FVector2D targetScreenPosition;
	FVector2D widgetSize = GetCachedGeometry().GetAbsoluteSize();
	if (!widgetSize.IsZero() && targetObj && controller && controller->ProjectWorldLocationToScreen(targetObj->GetLocation(), targetScreenPosition))
	{
		SetColorAndOpacity(FLinearColor::White);

		FVector2D widgetOffset(-0.5f * widgetSize.X, -widgetSize.Y - 16.0f);
		SetPositionInViewport(targetScreenPosition + widgetOffset);
	}
	else
	{
		SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
	}
}

void URoofPerimeterPropertiesWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

FEventReply URoofPerimeterPropertiesWidget::OnClickedBorder(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	return FEventReply(true);
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
	if (CommitMethod != ETextCommit::OnCleared)
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
	if (CommitMethod != ETextCommit::OnCleared)
	{
		float degreesValue;
		if (UModumateDimensionStatics::TryParseInputNumber(Text.ToString(), degreesValue))
		{
			float slopeValue = FMath::Tan(FMath::DegreesToRadians(degreesValue));
			SetEdgeSlope(slopeValue);
		}
	}
}

void URoofPerimeterPropertiesWidget::OnOverhangEditorTextChanged(const FText& NewText)
{

}

void URoofPerimeterPropertiesWidget::OnOverhangEditorTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnCleared)
	{
		// TODO: parse the text from a formatted dimension, rather than the raw cm value
		float OverhangValueCM;
		if (UModumateDimensionStatics::TryParseInputNumber(Text.ToString(), OverhangValueCM) && (OverhangValueCM >= 0.0f))
		{
			CurrentProperties.bOverridden = true;
			CurrentProperties.Overhang = OverhangValueCM;
			SaveEdgeProperties();
		}
	}
}

void URoofPerimeterPropertiesWidget::OnButtonPressedCommit()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller && (controller->InteractionHandle == OwningHandle))
	{
		// TODO: this is a sign that we need a better tool/handle API
		controller->InteractionHandle->EndUse();
		controller->InteractionHandle = nullptr;
	}
}

void URoofPerimeterPropertiesWidget::OnButtonPressedCancel()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
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
	AModumateObjectInstance *targetObj = GameState->Document->GetObjectById(TargetPerimeterID);
	auto controller = GetOwningPlayer<AEditModelPlayerController>();

	if (!(targetObj && controller && (controller->InteractionHandle == OwningHandle)))
	{
		return;
	}

	auto delta = MakeShared<FMOIDelta>();
	auto& modifiedState = delta->AddMutationState(targetObj);

	FMOIRoofPerimeterData roofPerimeterData;
	if (!ensure(modifiedState.CustomData.LoadStructData(roofPerimeterData)))
	{
		return;
	}

	// Save the current properties for this widget's target edge
	roofPerimeterData.EdgeProperties.Emplace(TargetEdgeID, CurrentProperties);

	// Also take this opportunity to remove stale edge data
	// TODO: splitting/merging edges should cleanly transfer customized edge properties via side effects
	TArray<int32> staleEdgeIDs;
	for (auto& kvp : roofPerimeterData.EdgeProperties)
	{
		const AModumateObjectInstance* edgeMOI = GameState->Document->GetObjectById(kvp.Key);
		if ((edgeMOI == nullptr) || edgeMOI->IsDestroyed() || (edgeMOI->GetObjectType() != EObjectType::OTMetaEdge))
		{
			staleEdgeIDs.Add(kvp.Key);
		}
	}
	for (int32 staleEdgeID : staleEdgeIDs)
	{
		roofPerimeterData.EdgeProperties.Remove(staleEdgeID);
	}

	if (!ensure(modifiedState.CustomData.SaveStructData(roofPerimeterData)))
	{
		return;
	}

	controller->GetDocument()->ApplyDeltas({ delta }, controller->GetWorld());
}
