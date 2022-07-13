// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"

#include "ModumateArchitecturalLight.generated.h"

/*
Artificial lighting fixtures
*/
USTRUCT()
struct MODUMATE_API FArchitecturalLight
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Key;

	FText DisplayName = FText::GetEmpty();

	FBIMNameType ProfilePath;

	FBIMNameType IconPath;

	FGuid UniqueKey() const { return Key; }
};

