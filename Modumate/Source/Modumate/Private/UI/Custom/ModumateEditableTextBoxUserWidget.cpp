// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/ModumateUIStatics.h"
#include "UI/Custom/ModumateEditableTextBox.h"

UModumateEditableTextBoxUserWidget::UModumateEditableTextBoxUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModumateEditableTextBoxUserWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	ChangeText(TextOverride);
	ChangeHint(HintOverride);
	return true;
}

void UModumateEditableTextBoxUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModumateEditableTextBoxUserWidget::ChangeText(const FText &NewText, bool EllipsizeText)
{
	if (!ModumateEditableTextBox)
	{
		return;
	}

	if (EllipsizeText)
	{
		FString textEllipsized = UModumateUIStatics::GetEllipsizeString(NewText.ToString(), EllipsizeWordAt);
		ModumateEditableTextBox->SetText(FText::FromString(textEllipsized)); // Potential loss of auto localization
	}
	else
	{
		ModumateEditableTextBox->SetText(NewText);
	}
}

void UModumateEditableTextBoxUserWidget::ChangeHint(const FText &NewHint, bool EllipsizeHint)
{
	if (!ModumateEditableTextBox)
	{
		return;
	}

	if (EllipsizeHint)
	{
		FString hintEllipsized = UModumateUIStatics::GetEllipsizeString(NewHint.ToString(), EllipsizeWordAt);
		ModumateEditableTextBox->SetHintText(FText::FromString(hintEllipsized)); // Potential loss of auto localization
	}
	else
	{
		ModumateEditableTextBox->SetHintText(NewHint);
	}
}
