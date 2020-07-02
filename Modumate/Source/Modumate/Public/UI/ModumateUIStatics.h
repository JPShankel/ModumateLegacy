// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ModumateUIStatics.generated.h"

// Helper functions for accessing / editing UI related data
UCLASS(BlueprintType)
class MODUMATE_API UModumateUIStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "Modumate | UI")
 	static FString GetEllipsizeString(const FString& InString, const int32 StringLength = 10);
};
