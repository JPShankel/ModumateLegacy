// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardQuantityListTotal.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "ModumateCore/ModumateDimensionStatics.h"

#define LOCTEXT_NAMESPACE "PresetCardQuantityListTotal"

UPresetCardQuantityListTotal::UPresetCardQuantityListTotal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardQuantityListTotal::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UPresetCardQuantityListTotal::NativeConstruct()
{
	Super::NativeConstruct();
}

void UPresetCardQuantityListTotal::BuildTotalLabel(const FQuantity& InQuantity, bool bUseMetric)
{
	if (InQuantity.Count > 0)
	{
		FieldTitleMeasurmentType1->ChangeText(LOCTEXT("CountTitle", "count"));
		FieldTitleMeasurmentType2->ChangeText(FText::GetEmpty());
		Quantity1->ChangeText(FText::AsNumber(InQuantity.Count));
		Quantity2->ChangeText(FText::GetEmpty());
	}
	else
	{
		if (bUseMetric)
		{
			FieldTitleMeasurmentType1->ChangeText(LOCTEXT("AreaTitleMetric", "m2"));
			FieldTitleMeasurmentType2->ChangeText(LOCTEXT("LinearTitleMetric", "m"));
			Quantity1->ChangeText(FText::AsNumber(InQuantity.Area * UModumateDimensionStatics::SquareCentimetersToSquareMeters));
			Quantity2->ChangeText(FText::AsNumber(InQuantity.Linear * 0.01));

		}
		else
		{
			FieldTitleMeasurmentType1->ChangeText(LOCTEXT("AreaTitle", "sq.ft."));
			FieldTitleMeasurmentType2->ChangeText(LOCTEXT("LinearTitle", "lin.ft."));
			Quantity1->ChangeText(FText::AsNumber(InQuantity.Area * UModumateDimensionStatics::SquareCentimetersToSquareFeet));
			Quantity2->ChangeText(FText::AsNumber(InQuantity.Linear * UModumateDimensionStatics::CentimetersToFeet));
		}
	}
}

#undef LOCTEXT_NAMESPACE
