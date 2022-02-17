// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DrawingDesignerRender.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class FDrawingDesignerLine;
class ALineActor;
class UModumateDocument;
class UMeshComponent;
class UProceduralMeshComponent;
enum class EObjectType : uint8;

UCLASS()
class MODUMATE_API ADrawingDesignerRender : public AActor
{
	GENERATED_BODY()

public:
	ADrawingDesignerRender();
	FTransform GetViewTransform() const { return ViewTransform;  }
	void SetViewTransform(const FTransform& Transform) { ViewTransform = Transform; }
	void SetDocument(const UModumateDocument* InDoc) { Doc = InDoc; }
	void AddLines(const TArray<FDrawingDesignerLine>& Lines);
	void EmptyLines();
	void AddObjects(const FVector& ViewDirection, float MinLength);

	void RestoreObjects();
	void SetupRenderTarget(int32 ImageWidth);
	void RenderFfe();
	void RenderImage(const FVector& ViewDirection, float MinLength);

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
	const UModumateDocument* Doc = nullptr;

	UPROPERTY()
	TArray<ALineActor*> LineActors;

	using StaticMaterialKey = TPair<UMeshComponent*, int32>;
	TMap<StaticMaterialKey, UMaterialInterface*> SceneStaticMaterialMap;

	TMap<UMeshComponent*, int32> SceneMeshComponents;

	// Stencil-buffer values that are coordinated with the post-process material PP_DrawingDesignerRender.
	// Also with ALineActor::ToggleForDrawingRender().
	enum EStencilValues {SVNone = 0, SVMoi = 1, SVForeground = 2};
	static const TSet<EObjectType> RenderedObjectTypes;
};
