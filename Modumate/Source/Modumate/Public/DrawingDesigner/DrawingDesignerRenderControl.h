// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UModumateDocument;
class ADrawingDesignerRender;
class UStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class AMOICutPlane;
class ALineActor;
struct FDrawingDesignerSnap;
struct FDrawingDesignerView;
enum class EObjectType: uint8;

class MODUMATE_API FDrawingDesignerRenderControl
{
public:
	FDrawingDesignerRenderControl(UModumateDocument* InDoc)
		: Doc(InDoc) { }
	~FDrawingDesignerRenderControl();

	FString GetViewList() const;
	bool GetView(const FString& JsonRequest, FString& OutJsonResponse);
	bool GetMoiFromView(FVector2D uv, FDrawingDesignerView& view, int32& OutMoiId) const;

	void AddSceneLines(const FVector& ViewDirection, float MinLength, ADrawingDesignerRender* Render);
	ALineActor* GetLineActor();
	void FreeLineActor(ALineActor* LineActor);

private:

	FName MOITraceTag = FName(TEXT("MOITrace"));

	bool GetViewAxis(FDrawingDesignerView& View, FVector& OutXAxis, FVector& OutYAxis, FVector& OutZAxis, FVector& OutOrigin, FVector2D& OutSize) const;
	void RestorePortalMaterials();  // unused
	void GetSnapPoints(int32 viewId, TMap<FString, FDrawingDesignerSnap>& OutSnapPoints) const;
	void DestroyLineActors();
	bool IsFloorplan(const FDrawingDesignerView& View) const;

	FVector CachedXAxis;
	FVector CachedYAxis;
	FVector CachedOrigin;
	FVector2D CachedSize;

	UModumateDocument* Doc = nullptr;

	using StaticMaterialKey = TPair<UStaticMeshComponent*, int32>;
	TMap<StaticMaterialKey, UMaterialInterface*> SceneStaticMaterialMap;
	using ProcMaterialKey = TPair<UProceduralMeshComponent*, int32>;
	TMap< ProcMaterialKey, UMaterialInterface*> SceneProcMaterialMap;
	static constexpr int32 LinePoolMaxSize = 40 * 1000;
	TArray<ALineActor*> LinePool;
};
