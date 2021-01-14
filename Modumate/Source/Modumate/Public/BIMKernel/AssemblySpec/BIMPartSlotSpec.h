// Copyright 2018 Modumate, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Core/BIMTagPath.h"

#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateUnits.h"

#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateArchitecturalMesh.h"

#include "BIMPartSlotSpec.generated.h"

class FModumateDatabase;

/*
An assembly part slot contains a mesh and local transform
TODO: transforms will be derived from hosting plane/moi parameters...to be hard coded initially
*/
USTRUCT()
struct MODUMATE_API FBIMPartSlotSpec
{
	GENERATED_BODY()

	friend struct FBIMAssemblySpec;
private:

	UPROPERTY()
	FBIMPropertySheet Properties;

	EBIMResult BuildFromProperties(const FModumateDatabase& InDB);

#if WITH_EDITOR
	// For debugging
	FBIMKey DEBUGPresetKey;
	FGuid DEBUGSlotGUID;
	EBIMValueScope DEBUGNodeScope;
	FGuid DEBUG_GUID;
#endif

public:

	UPROPERTY()
	FVectorExpression Translation;

	UPROPERTY()
	FVectorExpression Size;

	UPROPERTY()
	FVectorExpression Orientation;

	UPROPERTY()
	TArray<bool> Flip = { false,false,false };

	UPROPERTY()
	int32 ParentSlotIndex = 0;

	UPROPERTY()
	FBIMTagPath NodeCategoryPath;

	UPROPERTY()
	FString SlotID;

	UPROPERTY()
	FArchitecturalMesh Mesh;
	
	UPROPERTY()
	TMap<FName, FArchitecturalMaterial> ChannelMaterials;

	static TMap<FString, FModumateUnitValue> DefaultNamedParameterMap;
	static bool TryGetDefaultNamedParameter(const FString& Name, FModumateUnitValue& OutVal);

};
