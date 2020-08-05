// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMProperties.h"

// All needed by ObjectAssembly, first step is to replace all inclusions of the object assembly header with this one
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateArchitecturalMesh.h"
#include "Database/ModumateSimpleMesh.h"
#include "BIMKernel/BIMLegacyPattern.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "Database/ModumateObjectAssembly.h"

class FBIMPresetCollection;

struct MODUMATE_API FBIMAssemblySpec
{
	EObjectType ObjectType = EObjectType::OTNone;
	FName RootPreset;
	FBIMPropertySheet RootProperties;
	TArray<FBIMPropertySheet> LayerProperties;

	FModumateObjectAssembly CachedAssembly;

	// For DataCollection support in preset manager
	FName UniqueKey() const { return RootPreset; }

	ECraftingResult FromPreset(const FBIMPresetCollection& PresetCollection, const FName& PresetID);
};
