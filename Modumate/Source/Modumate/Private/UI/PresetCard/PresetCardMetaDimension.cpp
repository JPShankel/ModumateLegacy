// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardMetaDimension.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "DocumentManagement/ModumateDocument.h"

#define LOCTEXT_NAMESPACE "PresetCardMetaDimension"

UPresetCardMetaDimension::UPresetCardMetaDimension(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardMetaDimension::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UPresetCardMetaDimension::NativeConstruct()
{
	Super::NativeConstruct();
}

void UPresetCardMetaDimension::BuildAsMetaDimension(EObjectType ObjectType, double InDimensionValue)
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	EDimensionUnits unitType;
	EUnit defaultUnit;
	if (controller)
	{
		unitType = controller->GetDocument()->GetCurrentSettings().DimensionType;
		defaultUnit = controller->GetDocument()->GetCurrentSettings().DimensionUnit;
	}
	else
	{
		unitType = EDimensionUnits::DU_Imperial;
		defaultUnit = EUnit::Feet;
	}

	switch (ObjectType)
	{
		case EObjectType::OTMetaEdge:
		{
			MetaObjectType->ChangeText(LOCTEXT("Units", "Total Length"));
			DimensionValue->ChangeText(UModumateDimensionStatics::CentimetersToDisplayText(InDimensionValue,1,unitType,defaultUnit));
		}
		break;

		case EObjectType::OTMetaPlane:
		{
			MetaObjectType->ChangeText(LOCTEXT("Units", "Total Area"));
			DimensionValue->ChangeText(UModumateDimensionStatics::CentimetersToDisplayText(InDimensionValue, 2,unitType,defaultUnit));
		}
		break;

		default:
			ensureAlways(false);
		break;
	};
}

#undef LOCTEXT_NAMESPACE