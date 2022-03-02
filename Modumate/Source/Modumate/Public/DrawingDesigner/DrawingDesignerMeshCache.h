// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DrawingDesigner/DrawingDesignerLine.h"
#include "DrawingDesignerMeshCache.generated.h"


struct FBIMAssemblySpec;

UCLASS()
class MODUMATE_API UDrawingDesignerMeshCache: public UObject
{
	GENERATED_BODY()

public:
	UDrawingDesignerMeshCache();
	void Shutdown();

	// Get designer lines for a preset at a specific scale, only evaluating entire mesh the first time.
	// bLateralInvert appears to be currently unused.
	bool GetDesignerLines(const FBIMAssemblySpec& ObAsm, const FVector& Scale, bool bLateralInvert,
		const FVector& ViewDirection, TArray<FDrawingDesignerLined>& OutLines, float MinLength = 0.0f);
	bool GetDesignerLines(const FBIMAssemblySpec& ObAsm, const FVector& Scale, bool bLateralInvert, TArray<FDrawingDesignerLined>& OutLines);

private:
	struct CacheItem
	{
		FVector Scale;
		TArray<FDrawingDesignerLined> Mesh;
	};
	TMap<FGuid, TArray<CacheItem>> Cache;
	
	bool GetLinesForAssembly(const FBIMAssemblySpec& Assembly, const FVector& Scale, TArray<FDrawingDesignerLined>& OutLines);
};
