// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UserWidget.h"
#include "ModumateSlateHelper.generated.h"


/**
*
*/

UCLASS()
class MODUMATE_API UModumateSlateHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Creates a Hermite Spline
	// Points must be made of InStart, InStartDir, InEnd, InEndDir
	UFUNCTION(BlueprintCallable, Category = "Painting")
	static void DrawSplineBP(const FPaintContext& context, const TArray<FVector2D>& points, const FLinearColor& color, float thickness = 1.f);

	// Creates a Bezier Spline, must be made of 4 points
	UFUNCTION(BlueprintCallable, Category = "Painting")
	static void DrawCubicBezierSplineBP(const FPaintContext& context, const TArray<FVector2D>& points, const FLinearColor& color, float thickness = 1.f);
};
