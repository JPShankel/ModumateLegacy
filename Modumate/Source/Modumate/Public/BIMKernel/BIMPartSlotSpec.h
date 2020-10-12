// Copyright 2018 Modumate, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMKey.h"

#include "ModumateCore/ExpressionEvaluator.h"

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


public:
	Modumate::Expression::FVectorExpression Translation, Size, Orientation;
	using FBooleanVector = bool[3];
	FBooleanVector Flip{ false, false, false };

	int32 ParentSlotIndex = 0;

	FArchitecturalMesh Mesh;
	TMap<FName, FArchitecturalMaterial> ChannelMaterials;
};