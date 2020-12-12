// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Database/ModumateObjectEnums.h"

class MODUMATE_API FMOIFactory
{
public:
	static UClass* GetMOIClass(EObjectType ObjectType);
};
