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
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/SkyActor.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/AxesActor.h"

ADrawingDesignerRender::ADrawingDesignerRender()
{
	static const FName componentName(TEXT("DrawingDesignerRenderCapture"));
	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(componentName);

	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->bCaptureEveryFrame = false;
	RootComponent = CaptureComponent;
}

void ADrawingDesignerRender::AddLines(const TArray<FDrawingDesignerLine>& Lines)
{
	for (const auto& line : Lines)
	{
		ALineActor* lineActor = GetWorld()->SpawnActor<ALineActor>();
		lineActor->Point1 = line.P1;
		lineActor->Point2 = line.P2;
		lineActor->SetIsHUD(false);
		lineActor->MakeGeometry();
		lineActor->ToggleForDrawingRender(true);
		lineActor->UpdateLineVisuals(true, line.GetDDThickness(), line.GetLineShadeAsColor());
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
	if (!ensure(orthoWidth != 0)) {
		return;
	}
	int32 imageHeight = imageWidth * orthoHeight / orthoWidth;

	if (!ensure(Doc))
	{
		return;
	}

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
			FfeRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), imageWidth, imageHeight,
				ETextureRenderTargetFormat::RTF_R16f);
			if (!ensureAlways(FfeRenderTarget))
			{
				return;
			}
		}
		else
		{
			RenderTarget->ResizeTarget(imageWidth, imageHeight);
			FfeRenderTarget->ResizeTarget(imageWidth, imageHeight);
		}
	}

	auto* playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (playerController && playerController->SkyActor)
	{
		CaptureComponent->HideActorComponents(playerController->SkyActor);
		CaptureComponent->HideActorComponents(playerController->AxesActor);
	}

	FTransform cameraTransform(ViewTransform.GetRotation(), ViewTransform.GetTranslation());
	CaptureComponent->SetWorldTransform(cameraTransform);
	CaptureComponent->OrthoWidth = orthoWidth;

	RenderFfe();
	CaptureComponent->TextureTarget = RenderTarget;

	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	CaptureComponent->CaptureScene();

	RestoreFfeMaterials();
	SceneStaticMaterialMap.Empty();

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

void ADrawingDesignerRender::RenderFfe()
{
	const TArray<const AModumateObjectInstance*> ffeObjects = Doc->GetObjectsOfType(EObjectType::OTFurniture);
	if (ffeObjects.Num() == 0)
	{
		return;
	}

	auto* gameMode = GetWorld()->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
	if (!gameMode)
	{
		return;
	}

	UMaterialInterface* depthMaterial = gameMode->DepthMaterial;
	if (!ensure(depthMaterial))
	{
		return;
	}

	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	for (const auto* ffe: ffeObjects)
	{
		const AActor* actor = ffe->GetActor();
		TArray<UMeshComponent*> components;
		actor->GetComponents<UMeshComponent>(components);
		for (auto* component: components)
		{
			const int32 numMaterials = component->GetNumMaterials();
			for (int32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
			{
				UMaterialInterface* existingMaterial = component->GetMaterial(materialIndex);
				if (existingMaterial)
				{
					SceneStaticMaterialMap.Add(StaticMaterialKey(component, materialIndex), existingMaterial);
					component->SetMaterial(materialIndex, depthMaterial);
				}
			}

			CaptureComponent->ShowOnlyComponents.Add(component);
		}
	}

	CaptureComponent->TextureTarget = FfeRenderTarget;
	CaptureComponent->CaptureScene();

	UMaterialInterface* sobelMaterial = gameMode->SobelEdgeMaterial;
	if (!ensure(sobelMaterial))
	{
		return;
	}

	static const FName depthTextureParamName(TEXT("FfeDepth"));
	UMaterialInstanceDynamic* curMID = UMaterialInstanceDynamic::Create(sobelMaterial, this);
	curMID->SetTextureParameterValue(depthTextureParamName, FfeRenderTarget);

	for (const auto* ffe: ffeObjects)
	{
		const AActor* actor = ffe->GetActor();
		TArray<UMeshComponent*> components;
		actor->GetComponents<UMeshComponent>(components);
		for (auto* component: components)
		{
			const int32 numMaterials = component->GetNumMaterials();

			for (int32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
			{
				if (component->GetMaterial(materialIndex))
				{
					component->SetMaterial(materialIndex, curMID);
				}
			}
		}
	}

#if 0
	UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), FfeRenderTarget, TEXT("/Modumate"), TEXT("portaldrawing_ffe.png"));
#endif
}

void ADrawingDesignerRender::RestoreFfeMaterials()
{
	const TArray<const AModumateObjectInstance*> ffeObjects = Doc->GetObjectsOfType(EObjectType::OTFurniture);
	for (const auto* ffe: ffeObjects)
	{
		const AActor* actor = ffe->GetActor();
		TArray<UMeshComponent*> components;
		actor->GetComponents<UMeshComponent>(components);
		for (auto* component : components)
		{
			const int32 numMaterials = component->GetNumMaterials();
			for (int32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
			{
				if (component->GetMaterial(materialIndex))
				{
					UMaterialInterface** originalMaterial = SceneStaticMaterialMap.Find(StaticMaterialKey(component, materialIndex));
					component->SetMaterial(materialIndex, *originalMaterial);
				}
			}
		}
	}
}
