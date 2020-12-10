// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

class MODUMATE_API FMOIFactory
{
public:
	static FModumateObjectInstance *MakeMOI(EObjectType ObjectType);
};
