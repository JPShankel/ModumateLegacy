// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "Database/ModumateObjectDatabase.h"

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
