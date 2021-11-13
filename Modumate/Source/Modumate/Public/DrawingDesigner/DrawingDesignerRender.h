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

	void RenderImage(int32 imageWidth);

	bool GetImagePNG(TArray<uint8>& OutImage) const;

	virtual void Destroyed() override;

private:

	void RenderFfe();
	void RestoreFfeMaterials();

	FTransform ViewTransform;

	UPROPERTY()
	USceneCaptureComponent2D* CaptureComponent = nullptr;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget = nullptr;

	UPROPERTY()
	UTextureRenderTarget2D* FfeRenderTarget = nullptr;
	const UModumateDocument* Doc = nullptr;

	UPROPERTY()
	TArray<ALineActor*> LineActors;

	using StaticMaterialKey = TPair<UMeshComponent*, int32>;
	TMap<StaticMaterialKey, UMaterialInterface*> SceneStaticMaterialMap;
};
