// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockProperties.h"

#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ToolsAndAdjustments/Tools/EditModelMetaPlaneTool.h"
#include "ToolsAndAdjustments/Tools/EditModelPlaneHostedObjTool.h"
#include "ToolsAndAdjustments/Tools/EditModelPortalTools.h"
#include "UI/Custom/ModumateDropDownUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

UToolTrayBlockProperties::UToolTrayBlockProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UToolTrayBlockProperties::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(HorizontalBoxInstWidth && HorizontalBoxInstHeight && HorizontalBoxInstSillHeight && HorizontalBoxInstJustification &&
		EditableTextBoxInstWidth && EditableTextBoxInstHeight && EditableTextBoxInstSillHeight &&
		DropDownInstJustification))
	{
		return false;
	}

	EditableTextBoxInstWidth->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UToolTrayBlockProperties::OnInstWidthTextCommitted);
	EditableTextBoxInstHeight->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UToolTrayBlockProperties::OnInstHeightTextCommitted);
	EditableTextBoxInstSillHeight->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UToolTrayBlockProperties::OnInstSillHeightTextCommitted);
	DropDownInstJustification->ComboBoxStringJustification->OnSelectionChanged.AddDynamic(this, &UToolTrayBlockProperties::OnInstJustificationCommitted);

	return true;
}

void UToolTrayBlockProperties::ChangeBlockProperties(UEditModelToolBase* CurrentTool)
{
	bool bWidthEnabled = false, bHeightEnabled = false, bSillHeightEnabled = false, bJustificationEnabled = false;
	float widthValue = 0.0f, heightValue = 0.0f, sillHeightValue = 0.0f, justificationValue = 0.0f;

	if (auto portalTool = Cast<UPortalToolBase>(CurrentTool))
	{
		bWidthEnabled = true;
		widthValue = portalTool->GetInstanceWidth();

		bHeightEnabled = true;
		heightValue = portalTool->GetInstanceHeight();

		if (portalTool->GetToolMode() == EToolMode::VE_WINDOW)
		{
			bSillHeightEnabled = true;
			sillHeightValue = portalTool->GetInstanceBottomOffset();
		}
	}
	else if (auto metaPlaneTool = Cast<UMetaPlaneTool>(CurrentTool))
	{
		bHeightEnabled = (metaPlaneTool->GetAxisConstraint() == EAxisConstraint::AxisZ);
		heightValue = metaPlaneTool->GetInstanceHeight();

		if (auto layeredTool = Cast<UPlaneHostedObjTool>(CurrentTool))
		{
			bJustificationEnabled = true;
			justificationValue = layeredTool->GetInstanceJustification();
		}
	}

	HorizontalBoxInstWidth->SetVisibility(bWidthEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bWidthEnabled)
	{
		FString widthString = UModumateDimensionStatics::DecimalToFractionString(widthValue * Modumate::CentimetersToInches, true);
		EditableTextBoxInstWidth->ModumateEditableTextBox->SetText(FText::FromString(widthString));
	}

	HorizontalBoxInstHeight->SetVisibility(bHeightEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bHeightEnabled)
	{
		FString heightString = UModumateDimensionStatics::DecimalToFractionString(heightValue * Modumate::CentimetersToInches, true);
		EditableTextBoxInstHeight->ModumateEditableTextBox->SetText(FText::FromString(heightString));
	}

	HorizontalBoxInstSillHeight->SetVisibility(bSillHeightEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bSillHeightEnabled)
	{
		FString sillHeightString = UModumateDimensionStatics::DecimalToFractionString(sillHeightValue * Modumate::CentimetersToInches, true);
		EditableTextBoxInstSillHeight->ModumateEditableTextBox->SetText(FText::FromString(sillHeightString));
	}

	HorizontalBoxInstJustification->SetVisibility(bJustificationEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bJustificationEnabled)
	{
		FString justificationString = (justificationValue == 0.0f) ? FString(TEXT("Exterior")) :
			((justificationValue == 1.0f) ? FString(TEXT("Interior")) : FString(TEXT("Centerline")));
		DropDownInstJustification->ComboBoxStringJustification->SetSelectedOption(justificationString);
	}
}

void UToolTrayBlockProperties::NativeConstruct()
{
	Super::NativeConstruct();
}

void UToolTrayBlockProperties::OnInstWidthTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
		auto currentTool = controller ? controller->CurrentTool.GetObject() : nullptr;
		if (auto portalTool = Cast<UPortalToolBase>(controller->CurrentTool.GetObject()))
		{
			auto enteredDimension = UModumateDimensionStatics::StringToFormattedDimension(Text.ToString());
			portalTool->SetInstanceWidth(enteredDimension.Centimeters);
		}
	}
}

void UToolTrayBlockProperties::OnInstHeightTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
		auto currentTool = controller ? controller->CurrentTool.GetObject() : nullptr;
		auto enteredDimension = UModumateDimensionStatics::StringToFormattedDimension(Text.ToString());

		if (auto portalTool = Cast<UPortalToolBase>(currentTool))
		{
			portalTool->SetInstanceHeight(enteredDimension.Centimeters);
		}
		else if (auto metaPlaneTool = Cast<UMetaPlaneTool>(currentTool))
		{
			metaPlaneTool->SetInstanceHeight(enteredDimension.Centimeters);
		}
	}
}

void UToolTrayBlockProperties::OnInstSillHeightTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
		auto currentTool = controller ? controller->CurrentTool.GetObject() : nullptr;
		if (auto portalTool = Cast<UPortalToolBase>(currentTool))
		{
			auto enteredDimension = UModumateDimensionStatics::StringToFormattedDimension(Text.ToString());
			portalTool->SetInstanceBottomOffset(enteredDimension.Centimeters);
		}
	}
}

void UToolTrayBlockProperties::OnInstJustificationCommitted(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType == ESelectInfo::OnMouseClick)
	{
		float justificationValue = .5f;
		if (SelectedItem == "Exterior")
		{
			justificationValue = 0.f;
		}
		else if (SelectedItem == "Interior")
		{
			justificationValue = 1.f;
		}

		AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
		auto currentTool = controller ? controller->CurrentTool.GetObject() : nullptr;
		if (auto layeredTool = Cast<UPlaneHostedObjTool>(currentTool))
		{
			layeredTool->SetInstanceJustification(justificationValue);
		}
	}
}
