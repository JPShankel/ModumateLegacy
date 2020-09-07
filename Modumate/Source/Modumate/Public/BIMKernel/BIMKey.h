// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct MODUMATE_API FBIMKey
{
	friend uint32 GetTypeHash(const FBIMKey& BIMKey);
private:
	FString StringValue;
	uint32 HashValue;

public:
	FBIMKey();
	FBIMKey(const FString& InStr);
	bool operator==(const FBIMKey& RHS) const;
	bool operator!=(const FBIMKey& RHS) const;
};

FORCEINLINE uint32 GetTypeHash(const FBIMKey& BIMKey)
{
	return BIMKey.HashValue;
}