// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"


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