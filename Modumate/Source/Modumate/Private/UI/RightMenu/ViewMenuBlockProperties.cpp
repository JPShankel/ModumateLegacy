// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/ViewMenuBlockProperties.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Components/CheckBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"


UViewMenuBlockProperties::UViewMenuBlockProperties(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UViewMenuBlockProperties::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!(ControlGravityOn && ControlGravityOff && EditableTextBox_FOV))
	{
		return false;
	}
	ControlGravityOn->OnCheckStateChanged.AddDynamic(this, &UViewMenuBlockProperties::OnControlGravityOnChanged);
	ControlGravityOff->OnCheckStateChanged.AddDynamic(this, &UViewMenuBlockProperties::OnControlGravityOffChanged);
	EditableTextBox_FOV->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UViewMenuBlockProperties::OnEditableTextBoxFOVCommitted);

	return true;
}

void UViewMenuBlockProperties::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
}

void UViewMenuBlockProperties::OnEditableTextBoxFOVCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Controller->SetFieldOfViewCommand(FCString::Atof(*Text.ToString()));
	}
}

void UViewMenuBlockProperties::OnControlGravityOnChanged(bool IsChecked)
{
	ToggleGravityCheckboxes(true);
}

void UViewMenuBlockProperties::OnControlGravityOffChanged(bool IsChecked)
{
	ToggleGravityCheckboxes(false);
}

void UViewMenuBlockProperties::ToggleGravityCheckboxes(bool NewEnable)
{
	Controller->ToggleGravityPawn();
	ControlGravityOn->SetIsChecked(NewEnable);
	ControlGravityOff->SetIsChecked(!NewEnable);

	if (NewEnable)
	{
		ControlGravityOn->SetVisibility(ESlateVisibility::HitTestInvisible);
		ControlGravityOff->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		ControlGravityOn->SetVisibility(ESlateVisibility::Visible);
		ControlGravityOff->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
