// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ModumateCore/ModumateTypes.h"
#include "QuantitiesDimensions.generated.h"

UENUM()
enum class EQuantitiesDimensions
{
	Count = 0x1,
	Linear = 0x2,
	Area = 0x4,
	Volume= 0x8
};
