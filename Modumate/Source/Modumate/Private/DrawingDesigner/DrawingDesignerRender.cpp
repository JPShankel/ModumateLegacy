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
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/SkyActor.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/AxesActor.h"

const TSet<EObjectType> ADrawingDesignerRender::RenderedObjectTypes({ EObjectType::OTCabinet, EObjectType::OTCeiling,
	EObjectType::OTCountertop, EObjectType::OTDoor, EObjectType::OTEdgeHosted, EObjectType::OTFaceHosted, EObjectType::OTFinish,
	EObjectType::OTFloorSegment, EObjectType::OTMullion, EObjectType::OTPointHosted, EObjectType::OTRailSegment,
	EObjectType::OTRoofFace, EObjectType::OTStaircase, EObjectType::OTStructureLine, EObjectType::OTSystemPanel, EObjectType::OTTerrain,
	EObjectType::OTTrim, EObjectType::OTWallSegment, EObjectType::OTWindow });

ADrawingDesignerRender::ADrawingDesignerRender()
{
	static const FName componentName(TEXT("DrawingDesignerRenderCapture"));
	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(componentName);

	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->bCaptureEveryFrame = false;
	SetRootComponent(CaptureComponent);
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
		CaptureComponent->ShowOnlyActorComponents(lineActor);
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

void ADrawingDesignerRender::SetupRenderTarget(int32 ImageWidth)
{
	int32 orthoWidth = ViewTransform.GetScale3D().X;
	int32 orthoHeight = ViewTransform.GetScale3D().Y;
	if (!ensure(orthoWidth != 0)) {
		return;
	}
	int32 imageHeight = ImageWidth * orthoHeight / orthoWidth;

	if (!ensure(Doc))
	{
		return;
	}

	if (RenderTarget == nullptr || RenderTarget->GetSurfaceWidth() != ImageWidth || RenderTarget->GetSurfaceHeight() != imageHeight)
	{
		if (!RenderTarget)
		{
			RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), ImageWidth, imageHeight,
				ETextureRenderTargetFormat::RTF_RGBA8);
			if (!ensureAlways(RenderTarget))
			{
				return;
			}
			FfeRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), ImageWidth, imageHeight,
				ETextureRenderTargetFormat::RTF_R16f);
			if (!ensureAlways(FfeRenderTarget))
			{
				return;
			}
		}
		else
		{
			RenderTarget->ResizeTarget(ImageWidth, imageHeight);
			FfeRenderTarget->ResizeTarget(ImageWidth, imageHeight);
		}
	}

	FTransform cameraTransform(ViewTransform.GetRotation(), ViewTransform.GetTranslation());
	CaptureComponent->SetWorldTransform(cameraTransform);
	CaptureComponent->OrthoWidth = orthoWidth;
}

void ADrawingDesignerRender::RenderImage(const FVector& ViewDirection, float MinLength)
{
	if (!ensure(RenderTarget))
	{
		return;
	}

	RenderFfe();
	AddObjects(ViewDirection, MinLength);

	CaptureComponent->TextureTarget = RenderTarget;

	CaptureComponent->CaptureScene();

	RestoreFfeMaterials();
	RestoreObjects();
	EmptyLines();
	SceneStaticMaterialMap.Empty();
	SceneMeshComponents.Empty();

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
			SceneMeshComponents.Add(component, component->CustomDepthStencilValue);
			component->SetCustomDepthStencilValue(SVForeground);
			component->SetRenderCustomDepth(true);

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
	UMaterialInstanceDynamic* sobelMID = UMaterialInstanceDynamic::Create(sobelMaterial, this);
	sobelMID->SetTextureParameterValue(depthTextureParamName, FfeRenderTarget);

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
					component->SetMaterial(materialIndex, sobelMID);
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

void ADrawingDesignerRender::AddObjects(const FVector& ViewDirection, float MinLength)
{
	TArray<const AModumateObjectInstance*> sceneObjects = static_cast<const UModumateDocument*>(Doc)->GetObjectsOfType(RenderedObjectTypes);
	TArray<FDrawingDesignerLine> sceneLines;
	SceneMeshComponents.Empty();

	for (const auto* moi : sceneObjects)
	{
		const ACompoundMeshActor* compoundActor = Cast<ACompoundMeshActor>(moi->GetActor());
		if (compoundActor)
		{
			const int32 numComponents = compoundActor->UseSlicedMesh.Num();
			for (int32 componentIndex = 0; componentIndex < numComponents; ++componentIndex)
			{
				if (compoundActor->UseSlicedMesh[componentIndex])
				{
					const int32 sliceStart = 9 * componentIndex;
					const int32 sliceEnd = 9 * (componentIndex + 1);
					for (int32 slice = sliceStart; slice < sliceEnd; ++slice)
					{
						UProceduralMeshComponent* meshComponent = compoundActor->NineSliceComps[slice];
						if (!meshComponent)
						{
							continue;
						}

						SceneMeshComponents.Add(meshComponent, meshComponent->CustomDepthStencilValue);
						meshComponent->SetCustomDepthStencilValue(SVMoi);
						meshComponent->SetRenderCustomDepth(true);
						CaptureComponent->ShowOnlyComponent(meshComponent);
					}
				}
				else
				{
					UStaticMeshComponent* meshComponent = compoundActor->StaticMeshComps[componentIndex];
					if (!meshComponent)
					{
						continue;
					}

					SceneMeshComponents.Add(meshComponent, meshComponent->CustomDepthStencilValue);
					meshComponent->SetCustomDepthStencilValue(SVMoi);
					meshComponent->SetRenderCustomDepth(true);
					CaptureComponent->ShowOnlyComponent(meshComponent);
				}
			}
		}
		else
		{
			const ADynamicMeshActor* dynamicActor = Cast<ADynamicMeshActor>(moi->GetActor());
			if (dynamicActor)
			{
				TArray<UProceduralMeshComponent*> meshComponents({ dynamicActor->Mesh, dynamicActor->MeshCap });
				meshComponents.Append(dynamicActor->ProceduralSubLayers);
				meshComponents.Append(dynamicActor->ProceduralSubLayers);
				for (UProceduralMeshComponent* meshComponent : meshComponents)
				{
					if (meshComponent)
					{
						SceneMeshComponents.Add(meshComponent, meshComponent->CustomDepthStencilValue);
						meshComponent->SetCustomDepthStencilValue(SVMoi);
						meshComponent->SetRenderCustomDepth(true);
						CaptureComponent->ShowOnlyComponent(meshComponent);
					}
				}
			}
			else
			{
				ensure(false); // No compound or dynamic mesh.
			}
		}

		moi->GetDrawingDesignerItems(ViewDirection, sceneLines, MinLength);
	}

	AddLines(sceneLines);
}

void ADrawingDesignerRender::RestoreObjects()
{
	for (auto& meshKvp : SceneMeshComponents)
	{
		meshKvp.Key->SetCustomDepthStencilValue(meshKvp.Value);
		meshKvp.Key->SetRenderCustomDepth(meshKvp.Value != 0);
	}
}
