// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMEnums.h"

namespace Modumate {
	namespace BIM {

		typedef FName FTag;

		class FTagGroup : public TArray<FTag>
		{
		public:
			bool MatchesAny(const FTagGroup &OtherGroup) const;
			bool MatchesAll(const FTagGroup &OtherGroup) const;

			ECraftingResult FromString(const FString &InString);
			ECraftingResult ToString(FString &OutString) const;
		};

		class FTagPath : public TArray<FTagGroup>
		{
		public:
			bool Matches(const FTagPath &OtherPath) const;

			ECraftingResult FromString(const FString &InString);
			ECraftingResult ToString(FString &OutString) const;
		};
} }