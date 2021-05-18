// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/Online/ModumateClientIcon.h"


bool UModumateClientIcon::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ClientName && IconImage))
	{
		return false;
	}

	return true;
}
