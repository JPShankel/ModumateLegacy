// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/EdgeDetailObj.h"

void AMOIEdgeDetail::PostLoadInstanceData()
{
	InstanceData.UpdateConditionHash();
}
