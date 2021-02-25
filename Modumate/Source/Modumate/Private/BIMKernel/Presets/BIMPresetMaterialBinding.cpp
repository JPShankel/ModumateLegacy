// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"


bool FBIMPresetMaterialChannelBinding::operator==(const FBIMPresetMaterialChannelBinding& RHS) const
{
	return Channel == RHS.Channel &&
		InnerMaterialGUID == RHS.InnerMaterialGUID &&
		SurfaceMaterialGUID == RHS.SurfaceMaterialGUID &&
		ColorHexValue == RHS.ColorHexValue &&
		ColorTintVariationHexValue == RHS.ColorTintVariationHexValue;
}

bool FBIMPresetMaterialChannelBinding::operator!=(const FBIMPresetMaterialChannelBinding& RHS) const
{
	return !(*this == RHS);
}