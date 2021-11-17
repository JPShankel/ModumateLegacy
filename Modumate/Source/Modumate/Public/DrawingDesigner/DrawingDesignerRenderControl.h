// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UModumateDocument;
class ADrawingDesignerRender;
class UStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
struct FDrawingDesignerSnap;

class MODUMATE_API FDrawingDesignerRenderControl
{
public:
	FDrawingDesignerRenderControl(const UModumateDocument* InDoc)
		: Doc(InDoc) { }
	FString GetViewList();
	bool GetView(const FString& jsonRequest, FString& OutJsonResponse);
	void AddSceneLines(const FVector& ViewDirection, float MinLength, ADrawingDesignerRender* Render);

private:
	void SwapPortalMaterials();
	void RestorePortalMaterials();
	void GetSnapPoints(TMap<FString, FDrawingDesignerSnap>& OutSnapPoints);

	FVector CachedXAxis;
	FVector CachedYAxis;
	FVector CachedOrigin;
	FVector2D CachedSize;

	const UModumateDocument *const Doc = nullptr;

	using StaticMaterialKey = TPair<UStaticMeshComponent*, int32>;
	TMap<StaticMaterialKey, UMaterialInterface*> SceneStaticMaterialMap;
	using ProcMaterialKey = TPair<UProceduralMeshComponent*, int32>;
	TMap< ProcMaterialKey, UMaterialInterface*> SceneProcMaterialMap;
};
