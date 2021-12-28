// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UModumateDocument;
class ADrawingDesignerRender;
class UStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class AMOICutPlane;
struct FDrawingDesignerSnap;
struct FDrawingDesignerView;

class MODUMATE_API FDrawingDesignerRenderControl
{
public:
	FDrawingDesignerRenderControl(UModumateDocument* InDoc)
		: Doc(InDoc) { }
	FString GetViewList();
	bool GetView(const FString& jsonRequest, FString& OutJsonResponse);
	bool GetMoiFromView(FVector2D uv, FDrawingDesignerView& view, int32& OutMoiId);
	void AddSceneLines(const FVector& ViewDirection, float MinLength, ADrawingDesignerRender* Render);

private:

	void SwapPortalMaterials(AMOICutPlane* CutPlane);
	void RestorePortalMaterials();
	void GetSnapPoints(TMap<FString, FDrawingDesignerSnap>& OutSnapPoints);

	FVector CachedXAxis;
	FVector CachedYAxis;
	FVector CachedOrigin;
	FVector2D CachedSize;

	UModumateDocument *const Doc = nullptr;

	using StaticMaterialKey = TPair<UStaticMeshComponent*, int32>;
	TMap<StaticMaterialKey, UMaterialInterface*> SceneStaticMaterialMap;
	using ProcMaterialKey = TPair<UProceduralMeshComponent*, int32>;
	TMap< ProcMaterialKey, UMaterialInterface*> SceneProcMaterialMap;
};
