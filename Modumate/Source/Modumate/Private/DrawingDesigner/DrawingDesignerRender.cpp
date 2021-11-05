// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerRender.h"

#include "Engine/World.h"
#include "ImageUtils.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DrawingDesigner/DrawingDesignerLine.h"
#include "DrawingDesigner/DrawingDesignerView.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/SkyActor.h"
#include "UnrealClasses/LineActor.h"

ADrawingDesignerRender::ADrawingDesignerRender()
{
	static const FName componentName(TEXT("DrawingDesignerRenderCapture"));
	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(componentName);

	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->bCaptureEveryFrame = false;
}

void ADrawingDesignerRender::AddLines(const TArray<FDrawingDesignerLine>& Lines)
{
	for (const auto& line : Lines)
	{
		ALineActor* lineActor = GetWorld()->SpawnActor<ALineActor>();
		lineActor->Point1 = line.P1;
		lineActor->Point2 = line.P2;
		lineActor->Thickness = line.Thickness;
		lineActor->SetIsHUD(false);
		lineActor->MakeGeometry();
		lineActor->UpdateLineVisuals(true);
		LineActors.Add(lineActor);
	}
}

void ADrawingDesignerRender::EmptyLines()
{
	for (auto* line: LineActors)
	{
		line->Destroy();
	}
	LineActors.Empty();
}

void ADrawingDesignerRender::RenderImage(int32 imageWidth)
{
	int32 orthoWidth = ViewTransform.GetScale3D().X;
	int32 orthoHeight = ViewTransform.GetScale3D().Y;
	int32 imageHeight = imageWidth * orthoHeight / orthoWidth;

	if (RenderTarget == nullptr || RenderTarget->GetSurfaceWidth() != imageWidth || RenderTarget->GetSurfaceHeight() != imageHeight)
	{
		if (!RenderTarget)
		{
			RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), imageWidth, imageHeight,
				ETextureRenderTargetFormat::RTF_RGBA8);
			if (!ensureAlways(RenderTarget))
			{
				return;
			}
			CaptureComponent->TextureTarget = RenderTarget;
		}
		else
		{
			RenderTarget->ResizeTarget(imageWidth, imageHeight);
		}
	}

	auto* playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (playerController && playerController->SkyActor)
	{
		CaptureComponent->HideActorComponents(playerController->SkyActor);
	}
	CaptureComponent->ShowFlags.SetOverrideDiffuseAndSpecular(true);

	FTransform cameraTransform(ViewTransform.GetRotation(), ViewTransform.GetTranslation());
	CaptureComponent->SetWorldTransform(cameraTransform);
	CaptureComponent->OrthoWidth = orthoWidth;
	CaptureComponent->CaptureScene();

#if 0
	UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), RenderTarget, TEXT("/Modumate"), TEXT("portaldrawing_test.png"));
#endif
}

bool ADrawingDesignerRender::GetImagePNG(TArray<uint8>& OutImage) const
{
	if (RenderTarget == nullptr)
	{
		return false;
	}

	FBufferArchive imageBuffer;
	if (!ensure(FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, imageBuffer)) )
	{
		return false;
	}

	OutImage = MoveTemp(imageBuffer);
	return true;
}

void ADrawingDesignerRender::Destroyed()
{
	EmptyLines();
	Super::Destroyed();
}
