// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/ModumateObjectInstance.h"

namespace Modumate
{
	class MODUMATE_API FMOIFactory
	{
	public:
		static IModumateObjectInstanceImpl *MakeMOIImplementation(EObjectType ObjectType, FModumateObjectInstance *MOI);
	};
}
