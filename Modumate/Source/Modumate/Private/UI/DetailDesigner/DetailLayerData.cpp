// Copyright 2021 Modumate, Inc. All Rights Reserved

#include "UI/DetailDesigner/DetailLayerData.h"

#include "ModumateCore/ModumateDimensionStatics.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"

#define LOCTEXT_NAMESPACE "ModumateDetailDesigner"

bool UDetailDesignerLayerData::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(LayerName && LayerThickness && ExtensionFront && ExtensionBack))
	{
		return false;
	}

	ExtensionFront->ModumateEditableTextBox->SelectAllTextWhenFocused = true;
	ExtensionBack->ModumateEditableTextBox->SelectAllTextWhenFocused = true;

	ExtensionFront->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UDetailDesignerLayerData::OnExtensionFrontTextCommitted);
	ExtensionBack->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UDetailDesignerLayerData::OnExtensionBackTextCommitted);

	return true;
}

void UDetailDesignerLayerData::PopulateLayerData(int32 InParticipantIndex, int32 InLayerIndex, const FVector2D& InExtensions)
{
	DetailParticipantIndex = InParticipantIndex;
	DetailLayerIndex = InLayerIndex;
	CurrentExtensions = InExtensions;

	ExtensionFront->ModumateEditableTextBox->SetText(UModumateDimensionStatics::CentimetersToImperialText(CurrentExtensions.X));
	ExtensionBack->ModumateEditableTextBox->SetText(UModumateDimensionStatics::CentimetersToImperialText(CurrentExtensions.Y));
}

bool UDetailDesignerLayerData::OnExtensionTextCommitted(int32 ExtensionIdx, const FString& String)
{
	auto enteredDimension = UModumateDimensionStatics::StringToFormattedDimension(String);
	if ((enteredDimension.Format != EDimensionFormat::Error) && ensure((ExtensionIdx == 0) || (ExtensionIdx == 1)) &&
		!UModumateDimensionStatics::CentimetersToImperialText(CurrentExtensions[ExtensionIdx]).ToString().Equals(String) &&
		!FMath::IsNearlyEqual(CurrentExtensions[ExtensionIdx], enteredDimension.Centimeters))
	{
		CurrentExtensions[ExtensionIdx] = enteredDimension.Centimeters;
		OnExtensionChanged.Broadcast(DetailParticipantIndex, DetailLayerIndex, CurrentExtensions);
		return true;
	}

	return false;
}

void UDetailDesignerLayerData::OnExtensionFrontTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if ((CommitMethod == ETextCommit::OnCleared) || !OnExtensionTextCommitted(0, Text.ToString()))
	{
		PopulateLayerData(DetailParticipantIndex, DetailLayerIndex, CurrentExtensions);
	}
}

void UDetailDesignerLayerData::OnExtensionBackTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if ((CommitMethod == ETextCommit::OnCleared) || !OnExtensionTextCommitted(1, Text.ToString()))
	{
		PopulateLayerData(DetailParticipantIndex, DetailLayerIndex, CurrentExtensions);
	}
}

#undef LOCTEXT_NAMESPACE