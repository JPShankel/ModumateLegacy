// Copyright 2018 Modumate, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMKey.h"
#include "BIMKernel/BIMTagPath.h"

#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateUnits.h"

#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateArchitecturalMesh.h"

class FModumateDatabase;

/*
An assembly part slot contains a mesh and local transform
TODO: transforms will be derived from hosting plane/moi parameters...to be hard coded initially
*/
class MODUMATE_API FBIMPartSlotSpec
{
	friend class FBIMAssemblySpec;
private:
	FBIMPropertySheet Properties;
	ECraftingResult BuildFromProperties(const FModumateDatabase& InDB);

#if WITH_EDITOR
	// For debugging
	FBIMKey PresetID, SlotName;
	EBIMValueScope NodeScope;

	// Default values for fine part parameters like "jamb width" and "handle backset"
#endif

public:
	Modumate::Expression::FVectorExpression Translation, Size, Orientation;
	using FBooleanVector = bool[3];
	FBooleanVector Flip{ false, false, false };

	int32 ParentSlotIndex = 0;

	FBIMTagPath NodeCategoryPath;

	FArchitecturalMesh Mesh;
	TMap<FName, FArchitecturalMaterial> ChannelMaterials;

	static TMap<FString, Modumate::Units::FUnitValue> DefaultNamedParameterMap;
	static bool TryGetDefaultNamedParameter(const FString& Name, Modumate::Units::FUnitValue& OutVal);
};
