#pragma once

#include "BIMPresetInfo.generated.h"


USTRUCT()
struct FBIMPresetInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FString Comments;

	UPROPERTY()
	FString Mark;
};