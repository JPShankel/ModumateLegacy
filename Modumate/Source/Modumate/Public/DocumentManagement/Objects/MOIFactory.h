// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/ModumateObjectInstance.h"

class MODUMATE_API FMOIFactory
{
public:
	static IModumateObjectInstanceImpl *MakeMOIImplementation(EObjectType ObjectType, FModumateObjectInstance *MOI);
};
