// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DrawingDesignerRender.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class FDrawingDesignerLine;
class FDrawingDesignerRenderControl;
class ALineActor;
class UModumateDocument;
class UMeshComponent;
class UProceduralMeshComponent;
class AMOICutPlane;
class AModumateObjectInstance;
class AEditModelGameMode;
enum class EObjectType : uint8;
enum class FModumateLayerType;

UCLASS()
class MODUMATE_API ADrawingDesignerRender : public AActor
{
	GENERATED_BODY()

public:
	ADrawingDesignerRender();
	FTransform GetViewTransform() const { return ViewTransform;  }
	void SetViewTransform(const FTransform& Transform) { ViewTransform = Transform; }
	void SetDocument(UModumateDocument* InDoc, FDrawingDesignerRenderControl* RenderControl);
	void AddLines(const TArray<FDrawingDesignerLine>& Lines, bool bInPlane);
	void EmptyLines();
	void AddObjects(const FVector& ViewDirection, float MinLength);
	void AddInPlaneObjects(AMOICutPlane* CutPlane, float MinLength);

	void RestoreObjects();
	void SetupRenderTarget(int32 ImageWidth);
	void RenderFfe();
	void RenderImage(AMOICutPlane* CutPlane, float MinLength, const TArray<int32>* DesignOptions = nullptr);

	bool GetImagePNG(TArray<uint8>& OutImage) const;

	virtual void Destroyed() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USceneCaptureComponent2D* CaptureComponent = nullptr;

private:
	void RestoreFfeMaterials();

	FTransform ViewTransform;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget = nullptr;

	UPROPERTY()
	UTextureRenderTarget2D* FfeRenderTarget = nullptr;
	UModumateDocument* Doc = nullptr;
	FDrawingDesignerRenderControl* DrawingDesignerRenderControl;

	UPROPERTY()
	TArray<ALineActor*> LineActors;

	TWeakObjectPtr<const AEditModelGameMode> GameMode;
	void AddInPlaneLines(FVector P0, FVector P1, FModumateLayerType Layer);
	void FillHiddenList(const TSet<int32>* OptionsOverride = nullptr);

	using StaticMaterialKey = TPair<UMeshComponent*, int32>;
	TMap<StaticMaterialKey, UMaterialInterface*> SceneStaticMaterialMap;

	TMap<UMeshComponent*, int32> SceneMeshComponents;
	FVector InPlaneOffset;
	float LineScalefactor = 1.0f;

	TSet<AModumateObjectInstance*> HiddenObjects;

	// Stencil-buffer values that are coordinated with the post-process material PP_DrawingDesignerRender.
	// Also with ALineActor::ToggleForDrawingRender().
	enum EStencilValues {SVNone = 0, SVMoi = 1, SVForeground = 2};
	static const TSet<EObjectType> RenderedObjectTypes;
	static constexpr float CaptureActorOffset = 10.0f;
};
