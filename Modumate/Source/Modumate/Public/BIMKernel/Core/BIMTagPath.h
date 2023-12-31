// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMTagPath.generated.h"

USTRUCT()
struct FBIMTagPath
{
public:
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> Tags;

	FBIMTagPath() {};
	FBIMTagPath(const FString& InStr);

	bool MatchesExact(const FBIMTagPath &OtherPath) const;
	bool MatchesPartial(const FBIMTagPath& OtherPath) const;
	bool Contains(const FString& Tag) const;

	EBIMResult FromString(const FString &InString);
	EBIMResult ToString(FString &OutString) const;
	EBIMResult GetPartialPath(int32 Count, FBIMTagPath& PartialPath) const;

	// Custom Export/Import functions so that it doesn't serialize in JSON as an array of strings, but rather as a single string.
	// Note: it will still use UPROPERTY-based serialization for non-JSON-based serialization backends, like over the network and for CBOR.
	bool ExportTextItem(FString& ValueStr, FBIMTagPath const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	bool operator==(const FBIMTagPath& RHS) const;
	bool operator!=(const FBIMTagPath& RHS) const;
};

FORCEINLINE uint32 GetTypeHash(const FBIMTagPath& TagPath)
{
	FString stringVal;
	if (ensureAlways(TagPath.ToString(stringVal)==EBIMResult::Success))
	{
		return GetTypeHash(stringVal);
	}
	return 0;
}

template<>
struct TStructOpsTypeTraits<FBIMTagPath> : public TStructOpsTypeTraitsBase2<FBIMTagPath>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithIdenticalViaEquality = true
	};
};