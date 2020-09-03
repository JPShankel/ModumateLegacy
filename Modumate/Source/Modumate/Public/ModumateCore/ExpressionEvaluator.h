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

		class MODUMATE_API FVectorExpression
		{
		private:
			FString X, Y, Z;

		public:
			FVectorExpression() {}
			FVectorExpression(const FString &InX, const FString &InY, const FString &InZ) : X(InX), Y(InY), Z(InZ) {}

			bool Evaluate(const TMap<FString, float> &Vars, FVector& OutVector) const;
		};
	}
}
