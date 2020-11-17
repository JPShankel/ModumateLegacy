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

	//Not a UPROPERTY, we serialize the full string representation
	TArray<FString> Tags;

	FBIMTagPath() {};
	FBIMTagPath(const FString& InStr);

	bool MatchesExact(const FBIMTagPath &OtherPath) const;
	bool MatchesPartial(const FBIMTagPath& OtherPath) const;

	EBIMResult FromString(const FString &InString);
	EBIMResult ToString(FString &OutString) const;

	bool ExportTextItem(FString& ValueStr, FBIMTagPath const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	bool operator==(const FBIMTagPath& RHS) const;
	bool operator!=(const FBIMTagPath& RHS) const;
};

template<>
struct TStructOpsTypeTraits<FBIMTagPath> : public TStructOpsTypeTraitsBase2<FBIMTagPath>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true,
	};
};