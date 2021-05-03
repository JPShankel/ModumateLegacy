// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetLayerPriority.h"
#include "BIMKernel/Presets/BIMPresetEditorForm.h"

EBIMResult FBIMPresetLayerPriority::SetFormElements(FBIMPresetForm& OutForm) const
{
	OutForm.AddLayerPriorityGroupElement();
	OutForm.AddLayerPriorityValueElement();
	return EBIMResult::Error;
}
