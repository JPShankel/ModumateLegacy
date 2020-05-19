// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateUnits.h"

namespace Modumate
{
	namespace Expression
	{
		bool ReplaceVariable(FString &exprString, const FString &varName, float varValue);
		bool Evaluate(const TMap<FString, float> &vars, const FString &expr, float &result);
		bool Evaluate(const TMap<FName, Units::FUnitValue> &vars, const FString &expr, Units::FUnitValue &result);
		bool Evaluate(const FString &fexpr, float &result);
		float Evaluate(const TMap<FString, float> &vars, const FString &expr);
	}
}
