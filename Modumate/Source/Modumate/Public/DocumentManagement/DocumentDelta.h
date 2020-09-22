// Copyright 2020 Modumate, Inc,  All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class FModumateDocument;

class MODUMATE_API FDocumentDelta : public TSharedFromThis<FDocumentDelta>
{
public:
	FDocumentDelta();
	virtual ~FDocumentDelta();

	virtual bool ApplyTo(FModumateDocument *doc, UWorld *world) const = 0;
	virtual TSharedPtr<FDocumentDelta> MakeInverse() const = 0;

	// TODO: potentially, int32 ID if it is useful here
};

using FDeltaPtr = TSharedPtr<FDocumentDelta>;
