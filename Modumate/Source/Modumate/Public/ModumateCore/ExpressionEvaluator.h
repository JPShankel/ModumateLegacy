// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateUnits.h"

namespace Modumate
{
	namespace Expression
	{
		bool ReplaceVariable(FString &ExprString, const FString &VarName, float VarValue);
		bool Evaluate(const TMap<FString, float> &Vars, const FString &Expr, float &Result);
		bool Evaluate(const TMap<FName, Units::FUnitValue> &Vars, const FString &Expr, Units::FUnitValue &Result);
		bool Evaluate(const FString &Expr, float &Result);
		float Evaluate(const TMap<FString, float> &Vars, const FString &Expr);
		bool ExtractVariables(const FString &ExprString, TArray<FString>& OutVariables);

		class MODUMATE_API FVectorExpression
		{
		private:
			FString X, Y, Z;

		public:
			FVectorExpression() {}
			FVectorExpression(const FString &InX, const FString &InY, const FString &InZ) : X(InX), Y(InY), Z(InZ) {}

			bool Evaluate(const TMap<FString, float> &Vars, FVector& OutVector) const;
			bool ExtractVariables(TArray<FString>& OutVariables) const;
		};
	}
}
