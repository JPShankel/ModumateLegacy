// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PresetOrigination.generated.h"

//This gets serialized with the document, so we have explicit
// values for each enumeration
UENUM()
enum class EPresetOrigination : uint8
{
	Unknown			 = 0,
	Canonical		 = 1,
	VanillaDerived   = 2,
	EditedDerived    = 3,
	Invented		 = 4,
};
