// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateUnits.h"
#include "ExpressionEvaluator.generated.h"

namespace Modumate
{
	namespace Expression
	{
		bool ReplaceVariable(FString &ExprString, const FString &VarName, float VarValue);
		bool Evaluate(const TMap<FString, float> &Vars, const FString &Expr, float &Result);
		bool Evaluate(const TMap<FName, FModumateUnitValue> &Vars, const FString &Expr, FModumateUnitValue& OutResult);
		bool Evaluate(const FString &Expr, float &Result);
		float Evaluate(const TMap<FString, float> &Vars, const FString &Expr);
		bool ExtractVariables(const FString &ExprString, TArray<FString>& OutVariables);
	}
}

USTRUCT()
struct MODUMATE_API FVectorExpression
{
	GENERATED_BODY()

private:

	UPROPERTY()
	FString X;

	UPROPERTY()
	FString Y;

	UPROPERTY()
	FString Z;

public:
	FVectorExpression() {}
	FVectorExpression(const FString& InX, const FString& InY, const FString& InZ) : X(InX), Y(InY), Z(InZ) {}

	bool Evaluate(const TMap<FString, float>& Vars, FVector& OutVector) const;
	bool ExtractVariables(TArray<FString>& OutVariables) const;

	bool operator==(const FVectorExpression& OtherExpression) const;
	bool operator!=(const FVectorExpression& OtherExpression) const;
};

template<>
struct TStructOpsTypeTraits<FVectorExpression> : public TStructOpsTypeTraitsBase2<FVectorExpression>
{
	enum
	{
		WithIdenticalViaEquality = true
	};
};