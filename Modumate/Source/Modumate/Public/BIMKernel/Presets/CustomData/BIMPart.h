#pragma once

#include "BIMPart.generated.h"


USTRUCT()
struct FBIMPartConfig
{
	GENERATED_BODY()

	UPROPERTY()
	FString Normal;

	UPROPERTY()
	FString Tangent;
	
	UPROPERTY()
	bool Zalign;
};