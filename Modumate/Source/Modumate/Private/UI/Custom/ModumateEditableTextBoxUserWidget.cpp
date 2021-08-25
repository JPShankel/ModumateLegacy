// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/ModumateUIStatics.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "Components/SizeBox.h"

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
	OverrideSizeBox();
	return true;
}

void UModumateEditableTextBoxUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModumateEditableTextBoxUserWidget::ChangeText(const FText &NewText, bool EllipsizeText)
{
	NonEllipsizedText = NewText;

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

void UModumateEditableTextBoxUserWidget::OverrideSizeBox()
{
	if (bOverride_WidthOverride)
	{
		SizeBoxEditable->SetWidthOverride(WidthOverride);
	}
	if (bOverride_HeightOverride)
	{
		SizeBoxEditable->SetHeightOverride(HeightOverride);
	}
	if (bOverride_MinDesiredWidth)
	{
		SizeBoxEditable->SetMinDesiredWidth(MinDesiredWidth);
	}
	if (bOverride_MinDesiredHeight)
	{
		SizeBoxEditable->SetMinDesiredHeight(MinDesiredHeight);
	}
	if (bOverride_MaxDesiredWidth)
	{
		SizeBoxEditable->SetMaxDesiredWidth(MaxDesiredWidth);
	}
	if (bOverride_MaxDesiredHeight)
	{
		SizeBoxEditable->SetMaxDesiredHeight(MaxDesiredHeight);
	}
	if (bOverride_MinAspectRatio)
	{
		SizeBoxEditable->SetMinAspectRatio(MinAspectRatio);
	}
	if (bOverride_MaxAspectRatio)
	{
		SizeBoxEditable->SetMaxAspectRatio(MaxAspectRatio);
	}
}
