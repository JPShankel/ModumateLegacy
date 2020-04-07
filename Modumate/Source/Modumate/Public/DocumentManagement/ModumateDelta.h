// Copyright 2020 Modumate, Inc,  All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
namespace Modumate
{
	class FModumateDocument;

	class MODUMATE_API FDelta : public TSharedFromThis<FDelta>
	{
	public:
		FDelta();
		virtual ~FDelta();

		virtual bool ApplyTo(FModumateDocument *doc, UWorld *world) const = 0;
		virtual TSharedPtr<FDelta> MakeInverse() const = 0;
		
		// TODO: potentially, int32 ID if it is useful here
	};
}