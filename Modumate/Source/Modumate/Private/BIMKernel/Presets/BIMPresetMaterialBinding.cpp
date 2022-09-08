// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/BIMPresetEditorForm.h"

#define LOCTEXT_NAMESPACE "ModumatePresetMaterialBindings"

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
			RefForm.AddMaterialBindingElement(FText::Format(LOCTEXT("InnerMaterial", "Inner Material {0}"), i + 1), binding.Channel, EMaterialChannelFields::InnerMaterial);
		}

		if (binding.SurfaceMaterialGUID.IsValid())
		{
			RefForm.AddMaterialBindingElement(FText::Format(LOCTEXT("SurfaceMaterial", "Surface Material {0}"), i + 1),binding.Channel, EMaterialChannelFields::SurfaceMaterial);
		}

		if (!binding.ColorHexValue.IsEmpty())
		{
			RefForm.AddMaterialBindingElement(FText::Format(LOCTEXT("Tint", "Tint {0}"), i + 1), binding.Channel, EMaterialChannelFields::ColorTint);
			RefForm.AddMaterialBindingElement(FText::Format(LOCTEXT("TintVariation", "Tint Variation {0}"), i + 1), binding.Channel, EMaterialChannelFields::ColorTintVariation);
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
		FMath::IsNearlyEqual(ColorTintVariationPercent,RHS.ColorTintVariationPercent);
}

bool FBIMPresetMaterialBinding::operator!=(const FBIMPresetMaterialBinding& RHS) const
{
	return !(*this == RHS);
}

EBIMResult FBIMPresetMaterialBinding::GetEngineMaterial(const FBIMPresetCollectionProxy& PresetCollection, FArchitecturalMaterial& OutMaterial) const
{
	FGuid matGuid = SurfaceMaterialGUID.IsValid() ? SurfaceMaterialGUID : InnerMaterialGUID;
	if (!matGuid.IsValid())
	{
		return EBIMResult::Error;
	}

	const FArchitecturalMaterial* material = PresetCollection.GetArchitecturalMaterialByGUID(matGuid);
	if (material == nullptr)
	{
		return EBIMResult::Error;
	}

	OutMaterial = *material;
	OutMaterial.Color = ColorHexValue.IsEmpty() ? FColor::White : FColor::FromHex(ColorHexValue);
	return EBIMResult::Success;
}

#undef LOCTEXT_NAMESPACE