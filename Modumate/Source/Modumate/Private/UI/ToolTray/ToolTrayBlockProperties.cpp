// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "Components/HorizontalBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UI/Custom/ModumateDropDownUserWidget.h"
#include "Components/ComboBoxString.h"

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
	if (!(EditableTextBoxInstHeight && DropDownInstJustification))
	{
		return false;
	}

	EditableTextBoxInstHeight->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UToolTrayBlockProperties::OnInstHeightTextCommitted);
	DropDownInstJustification->ComboBoxStringJustification->OnSelectionChanged.AddDynamic(this, &UToolTrayBlockProperties::OnInstJustificationCommitted);

	return true;
}

void UToolTrayBlockProperties::ChangeBlockProperties(EToolMode mode)
{
	if (!(HorizontalBoxInstHeight && HorizontalBoxInstJustification))
	{
		return;
	}

	switch (mode)
	{
	case EToolMode::VE_WALL:
	case EToolMode::VE_FLOOR:
	case EToolMode::VE_METAPLANE:
		HorizontalBoxInstHeight->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		HorizontalBoxInstJustification->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		break;
	default:
		HorizontalBoxInstHeight->SetVisibility(ESlateVisibility::Collapsed);
		HorizontalBoxInstJustification->SetVisibility(ESlateVisibility::Collapsed);
		break;
	}
}

void UToolTrayBlockProperties::NativeConstruct()
{
	Super::NativeConstruct();
}

void UToolTrayBlockProperties::OnInstHeightTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		AEditModelPlayerController_CPP *controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
		if (controller)
		{
			controller->UpdateDefaultWallHeight(UModumateDimensionStatics::StringToMetric(*Text.ToString(), true));
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

		AEditModelPlayerController_CPP *controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
		if (controller)
		{
			controller->UpdateDefaultJustificationXY(justificationValue);
			controller->UpdateDefaultJustificationZ(justificationValue);
		}
	}
}
