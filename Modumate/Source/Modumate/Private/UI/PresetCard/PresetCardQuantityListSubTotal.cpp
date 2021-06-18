// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardQuantityListSubTotal.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"

#define LOCTEXT_NAMESPACE "PresetCardQuantityListSubTotal"

UPresetCardQuantityListSubTotal::UPresetCardQuantityListSubTotal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardQuantityListSubTotal::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UPresetCardQuantityListSubTotal::NativeConstruct()
{
	Super::NativeConstruct();
}

void UPresetCardQuantityListSubTotal::BuildSubLabel(const FText& LabelText, const FQuantity& InQuantity, bool bMetric)
{
	TributaryType->ChangeText(LabelText);

	// TODO: Keep track of minimum columns needed
	if (InQuantity.Count > 0)
	{
		FieldTitleMeasurmentType1->ChangeText(LOCTEXT("CountTitle", "count"));
		FieldTitleMeasurmentType2->ChangeText(FText::GetEmpty());
	}
	else
	{
		if (bMetric)
		{
			FieldTitleMeasurmentType1->ChangeText(LOCTEXT("AreaTitleMetric", "m2"));
			FieldTitleMeasurmentType2->ChangeText(LOCTEXT("LinearTitleMetric", "m"));
		}
		else
		{
			FieldTitleMeasurmentType1->ChangeText(LOCTEXT("AreaTitle", "sq.ft."));
			FieldTitleMeasurmentType2->ChangeText(LOCTEXT("LinearTitle", "lin.ft."));
		}
	}
}

#undef LOCTEXT_NAMESPACE