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

UCLASS()
class MODUMATE_API ADrawingDesignerRender : public AActor
{
	GENERATED_BODY()

public:
	ADrawingDesignerRender();
	FTransform GetViewTransform() const { return ViewTransform;  }
	void SetViewTransform(const FTransform& Transform) { ViewTransform = Transform; }
	void AddLines(const TArray<FDrawingDesignerLine>& Lines);
	void EmptyLines();

	void RenderImage(int32 imageWidth);

	bool GetImagePNG(TArray<uint8>& OutImage) const;

	virtual void Destroyed() override;

private:
	FTransform ViewTransform;
	USceneCaptureComponent2D* CaptureComponent = nullptr;
	UTextureRenderTarget2D* RenderTarget = nullptr;

	UPROPERTY()
	TArray<ALineActor*> LineActors;
};
