// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMProperties.h"

namespace Modumate {
	namespace BIM {

		struct MODUMATE_API FModumateAssemblyPropertySpec
		{
			EObjectType ObjectType = EObjectType::OTNone;
			FName RootPreset;
			FBIMPropertySheet RootProperties;
			TArray<FBIMPropertySheet> LayerProperties;
		};
	}
}