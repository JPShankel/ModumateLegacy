// Copyright 2020 Modumate, Inc,  All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/StructDataWrapper.h"

#include "DocumentDelta.generated.h"

class UWorld;
class UModumateDocument;


USTRUCT()
struct MODUMATE_API FDocumentDelta
{
	GENERATED_BODY()

public:
	virtual ~FDocumentDelta() {}

	virtual bool ApplyTo(UModumateDocument* doc, UWorld* world) const { return false; };
	virtual TSharedPtr<FDocumentDelta> MakeInverse() const { return nullptr; };
	virtual FStructDataWrapper SerializeStruct();
	virtual void PostDeserializeStruct() {}

	// TODO: potentially, int32 ID if it is useful here
};

using FDeltaPtr = TSharedPtr<FDocumentDelta>;


USTRUCT()
struct MODUMATE_API FDeltasRecord
{
	GENERATED_BODY()

	FDeltasRecord();
	FDeltasRecord(const TArray<FDeltaPtr>& InDeltas);

	UPROPERTY()
	TArray<FStructDataWrapper> DeltaStructWrappers;
};
