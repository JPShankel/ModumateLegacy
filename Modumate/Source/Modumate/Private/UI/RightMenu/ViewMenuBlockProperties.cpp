// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/RightMenu/ViewMenuBlockProperties.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
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
	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
}

FReply UViewMenuBlockProperties::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply reply = Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	bool hasTimeChanged = false;
	FTimespan deltaTime;

	// Time change on mouse wheel direction
	if (EditableTextBox_Month->GetTickSpaceGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		FDateTime currentTime = Controller->SkyActor->GetCurrentDateTime();
		if (InMouseEvent.GetWheelDelta() > 0)
		{
			deltaTime = FTimespan::FromDays(currentTime.DaysInMonth(currentTime.GetYear(), currentTime.GetMonth()));
		}
		else
		{
			int32 lastMonth = currentTime.GetMonth() == 1 ? 12 : currentTime.GetMonth() - 1;
			int32 lastYear = lastMonth == 12 ? currentTime.GetYear() - 1 : currentTime.GetYear();
			deltaTime = FTimespan::FromDays(currentTime.DaysInMonth(FMath::Max(0, lastYear), lastMonth)) * -1;
		}
		hasTimeChanged = true;
	}
	else if (EditableTextBox_Day->GetTickSpaceGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
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
