// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "Database/ModumateObjectDatabase.h"
#include "BIMKernel/Presets/BIMPresetEditorForm.h"

EBIMResult FBIMPresetMaterialBindingSet::SetFormElements(FBIMPresetForm& RefForm) const
{
	RefForm.Elements = RefForm.Elements.FilterByPredicate(
		[](const FBIMPresetFormElement& Element)
	{
		return Element.FieldType != EBIMPresetEditorField::MaterialBinding;
	});

	for (int32 i=0;i<MaterialBindings.Num();++i)
	{
		auto& binding = MaterialBindings[i];

		if (binding.InnerMaterialGUID.IsValid())
		{
			RefForm.AddMaterialBindingElement(FText::FromString(FString::Printf(TEXT("Inner%d"), i)), binding.Channel, EMaterialChannelFields::InnerMaterial);
		}

		if (binding.SurfaceMaterialGUID.IsValid())
		{
			RefForm.AddMaterialBindingElement(FText::FromString(FString::Printf(TEXT("Surface%d"), i)), binding.Channel, EMaterialChannelFields::SurfaceMaterial);
		}

		if (!binding.ColorHexValue.IsEmpty())
		{
			RefForm.AddMaterialBindingElement(FText::FromString(FString::Printf(TEXT("Tint%d"), i)), binding.Channel, EMaterialChannelFields::ColorTint);
		}

		if (!binding.ColorTintVariationHexValue.IsEmpty())
		{
			RefForm.AddMaterialBindingElement(FText::FromString(FString::Printf(TEXT("TintVar%d"), i)), binding.Channel, EMaterialChannelFields::ColorTintVariation);
		}
	}

	return EBIMResult::Success;
}

bool FBIMPresetMaterialBinding::operator==(const FBIMPresetMaterialBinding& RHS) const
{
	return Channel == RHS.Channel &&
		InnerMaterialGUID == RHS.InnerMaterialGUID &&
		SurfaceMaterialGUID == RHS.SurfaceMaterialGUID &&
		ColorHexValue == RHS.ColorHexValue &&
		ColorTintVariationHexValue == RHS.ColorTintVariationHexValue;
}

bool FBIMPresetMaterialBinding::operator!=(const FBIMPresetMaterialBinding& RHS) const
{
	return !(*this == RHS);
}

EBIMResult FBIMPresetMaterialBinding::GetEngineMaterial(const FModumateDatabase& DB, FArchitecturalMaterial& OutMaterial) const
{
	FGuid matGuid = SurfaceMaterialGUID.IsValid() ? SurfaceMaterialGUID : InnerMaterialGUID;
	if (!matGuid.IsValid())
	{
		return EBIMResult::Error;
	}

	const FArchitecturalMaterial* material = DB.GetArchitecturalMaterialByGUID(matGuid);
	if (material == nullptr)
	{
		return EBIMResult::Error;
	}

	OutMaterial = *material;
	OutMaterial.Color = ColorHexValue.IsEmpty() ? FColor::White : FColor::FromHex(ColorHexValue);
	return EBIMResult::Success;
}
