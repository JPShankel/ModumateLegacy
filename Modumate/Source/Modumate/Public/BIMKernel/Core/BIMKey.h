// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKey.generated.h"

USTRUCT(BlueprintType)
struct MODUMATE_API FBIMKey
{
	GENERATED_BODY()
	friend uint32 GetTypeHash(const FBIMKey& BIMKey);

	FBIMKey();
	FBIMKey(FString InStr);
	bool operator==(const FBIMKey& RHS) const;
	bool operator!=(const FBIMKey& RHS) const;

	UPROPERTY(BlueprintReadOnly)
	FString StringValue;

	FString ToString() const { return StringValue; }

	bool IsNone() const { return StringValue.IsEmpty(); }

	bool ExportTextItem(FString& ValueStr, FBIMKey const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

private:
	uint32 HashValue;
	void UpdateHash();
};

FBIMKey operator+(const FBIMKey& LHS, const FBIMKey& RHS);

FORCEINLINE uint32 GetTypeHash(const FBIMKey& BIMKey)
{
	return BIMKey.HashValue;
}

template<>
struct TStructOpsTypeTraits<FBIMKey> : public TStructOpsTypeTraitsBase2<FBIMKey>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithIdenticalViaEquality = true
	};
};
