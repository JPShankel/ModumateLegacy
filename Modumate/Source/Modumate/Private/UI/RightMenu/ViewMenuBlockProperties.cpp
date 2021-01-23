// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/ViewMenuBlockProperties.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Components/CheckBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UnrealClasses/SkyActor.h"
#include "UI/Custom/ModumateButton.h"


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
	if (!(ControlGravityOn && ControlGravityOff && EditableTextBox_FOV && EditableTextBox_Month && EditableTextBox_Day && 
		EditableTextBox_Hour && EditableTextBox_Minute && ModumateButton_AM))
	{
		return false;
	}
	ControlGravityOn->OnCheckStateChanged.AddDynamic(this, &UViewMenuBlockProperties::OnControlGravityOnChanged);
	ControlGravityOff->OnCheckStateChanged.AddDynamic(this, &UViewMenuBlockProperties::OnControlGravityOffChanged);
	EditableTextBox_FOV->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UViewMenuBlockProperties::OnEditableTextBoxFOVCommitted);

	EditableTextBox_Month->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UViewMenuBlockProperties::OnEditableTextBoxMonthCommitted);
	EditableTextBox_Day->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UViewMenuBlockProperties::OnEditableTextBoxDayCommitted);
	EditableTextBox_Hour->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UViewMenuBlockProperties::OnEditableTextBoxHourCommitted);
	EditableTextBox_Minute->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UViewMenuBlockProperties::OnEditableTextBoxMinuteCommitted);
	ModumateButton_AM->OnReleased.AddDynamic(this, &UViewMenuBlockProperties::OnReleaseButtonModumateButtonAM);

	return true;
}

void UViewMenuBlockProperties::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController>();
}

FReply UViewMenuBlockProperties::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply reply = Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	bool hasTimeChanged = false;
	FTimespan deltaTime;

	// Time change on mouse wheel direction
	if (EditableTextBox_Day->GetTickSpaceGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		deltaTime = FTimespan::FromDays(1) * InMouseEvent.GetWheelDelta();
		hasTimeChanged = true;
	}
	else if (EditableTextBox_Hour->GetTickSpaceGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		deltaTime = FTimespan::FromHours(1) * InMouseEvent.GetWheelDelta();
		hasTimeChanged = true;
	}
	else if (EditableTextBox_Minute->GetTickSpaceGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		deltaTime = FTimespan::FromMinutes(1) * InMouseEvent.GetWheelDelta();
		hasTimeChanged = true;

	}
	if (hasTimeChanged)
	{
		Controller->SkyActor->AddTimespanToCurrentDateTime(deltaTime);
		SyncTextBoxesWithSkyActorCurrentTime();
	}

	// FTimespan cannot be used for changing month. Use SetCurrentDateTime instead
	if (EditableTextBox_Month->GetTickSpaceGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		FDateTime currentTime = Controller->SkyActor->GetCurrentDateTime();
		int32 newYear = currentTime.GetYear();
		int32 newMonth = (currentTime.GetMonth() + (int32)InMouseEvent.GetWheelDelta()) % 12;
		if (newMonth <= 0)
		{
			newMonth = 12;
			newYear = InMouseEvent.GetWheelDelta() > 0 ? currentTime.GetYear() + 1 : currentTime.GetYear() - 1;
		}
		int32 newDay = FMath::Clamp(currentTime.GetDay(), 1, FDateTime::DaysInMonth(newYear, newMonth));
		FDateTime newDateTime = FDateTime(newYear, newMonth, newDay, currentTime.GetHour(), currentTime.GetMinute());
		Controller->SkyActor->SetCurrentDateTime(newDateTime);
		SyncTextBoxesWithSkyActorCurrentTime();
	}

	return reply;
}

void UViewMenuBlockProperties::OnEditableTextBoxFOVCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Controller->SetFieldOfViewCommand(FCString::Atof(*Text.ToString()));
	}
}

void UViewMenuBlockProperties::OnEditableTextBoxMonthCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Controller->SkyActor->SetCurrentMonth(FCString::Atoi(*Text.ToString()));
		SyncTextBoxesWithSkyActorCurrentTime();
	}
}

void UViewMenuBlockProperties::OnEditableTextBoxDayCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Controller->SkyActor->SetCurrentDay(FCString::Atoi(*Text.ToString()));
		SyncTextBoxesWithSkyActorCurrentTime();
	}
}

void UViewMenuBlockProperties::OnEditableTextBoxHourCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Controller->SkyActor->SetCurrentHour(FCString::Atoi(*Text.ToString()));
		SyncTextBoxesWithSkyActorCurrentTime();
	}
}

void UViewMenuBlockProperties::OnEditableTextBoxMinuteCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		Controller->SkyActor->SetCurrentMinute(FCString::Atoi(*Text.ToString()));
		SyncTextBoxesWithSkyActorCurrentTime();
	}
}

void UViewMenuBlockProperties::OnReleaseButtonModumateButtonAM()
{
	Controller->SkyActor->ToggleCurrentMeridiem();
	SyncTextBoxesWithSkyActorCurrentTime();
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

void UViewMenuBlockProperties::SyncTextBoxesWithSkyActorCurrentTime()
{
	if (Controller && Controller->SkyActor)
	{
		FDateTime currentTime = Controller->SkyActor->GetCurrentDateTime();

		EditableTextBox_Month->ChangeText(FText::AsNumber(currentTime.GetMonth()));
		EditableTextBox_Day->ChangeText(FText::AsNumber(currentTime.GetDay()));
		EditableTextBox_Hour->ChangeText(FText::AsNumber(currentTime.GetHour12()));
		EditableTextBox_Minute->ChangeText(FText::AsNumber(currentTime.GetMinute()));
		FString newMeridiem = currentTime.IsMorning() ? TEXT("AM") : TEXT("PM");
		EditableTextBox_AM->SetText(FText::FromString(newMeridiem));
	}
}
