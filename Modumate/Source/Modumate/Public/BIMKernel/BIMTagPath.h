// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMEnums.h"

typedef FName FBIMTag;

class FBIMTagGroup : public TArray<FBIMTag>
{
public:
	bool MatchesAny(const FBIMTagGroup &OtherGroup) const;
	bool MatchesAll(const FBIMTagGroup &OtherGroup) const;

	ECraftingResult FromString(const FString &InString);
	ECraftingResult ToString(FString &OutString) const;
};

class FBIMTagPath : public TArray<FBIMTagGroup>
{
public:
	bool MatchesExact(const FBIMTagPath &OtherPath) const;
	bool MatchesPartial(const FBIMTagPath& OtherPath) const;

	ECraftingResult FromString(const FString &InString);
	ECraftingResult ToString(FString &OutString) const;
};
