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
#include "DrawingDesigner/DrawingDesignerRenderControl.h"
#include "DrawingDesigner/ModumateDDRenderDraw.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Objects/CutPlane.h"
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

namespace
{
	struct
	{
		float Thickness;
		int GreyValue;
	} LayerToDDParams[] =
	{
		{1.0f,  0},		// kDefault
		{0.106f,  0},	// kSeparatorCutStructuralLayer
		{0.176f, 0},	// kSeparatorCutOuterSurface
		{0.018f, 0},	// kSeparatorCutMinorLayer
		{0.053f, 0},	// kSeparatorBeyondSurfaceEdges
		{0.018f, 0},	// kSeparatorBeyondModuleEdges
		{0.106f, 0},	// kOpeningSystemCutLine
		{0.053f, 0},	// kOpeningSystemBeyond
		{0.035f, 0},	// kOpeningSystemBehind
		{0.035f, 153},	// kOpeningSystemOperatorLine
		{0.106f, 0},	// kSeparatorCutTrim
		{0.106f, 0},	// kCabinetCutCarcass
		{0.106f, 0},	// kCabinetCutAttachment
		{0.053f, 0},	// kCabinetBeyond
		{0.035f, 0},	// kCabinetBehind
		{0.035f, 0},	// kCabinetBeyondBlockedByCountertop
		{0.106f, 0},	// kCountertopCut
		{0.053f, 0},	// kCountertopBeyond
		{0.053f, 0},	// kFfeOutline
		{0.053f, 0},	// kFfeInteriorEdges
		{0.106f, 0},	// kBeamColumnCut
		{0.053f, 0},	// kBeamColumnBeyond
		{0.106f, 0},	// kMullionCut
		{0.053f, 0},	// kMullionBeyond
		{0.106f, 0},	// kSystemPanelCut
		{0.053f, 0},	// kSystemPanelBeyond
		{0.106f, 0},	// kFinishCut
		{0.053f, 0},	// kFinishBeyond
		{0.3f,  128},	// kDebug1
		{0.3f,  192},	// kDebug2
		{0.018f, 0},	// kSeparatorCutEndCaps
		{0.088f, 32},	// kDimensionMassing
		{0.088f, 32},	// kDimensionFraming
		{0.088f, 32},	// kDimensionOpening
		{0.088f, 32},	// KDimensionReference
		{0.176f, 0},	// kTerrainCut
		{0.053f, 0},	// kTerrainBeyond
		{0.106f, 0},	// kPartPointCut
		{0.106f, 0},	// kPartEdgeCut
		{0.106f, 0},	// kPartFaceCut
		{0.053f, 0},	// kPartPointBeyond
		{0.053f, 0},	// kPartEdgeBeyond
		{0.053f, 0},	// kPartFaceBeyond
	};
}

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

void ADrawingDesignerRender::SetDocument(const UModumateDocument* InDoc, FDrawingDesignerRenderControl* RenderControl)
{
	Doc = InDoc;
	DrawingDesignerRenderControl = RenderControl;
}

void ADrawingDesignerRender::AddLines(const TArray<FDrawingDesignerLine>& Lines, bool bInPlane)
{
	for (const auto& line : Lines)
	{
		ALineActor* lineActor = DrawingDesignerRenderControl->GetLineActor();
		lineActor->Point1 = line.P1;
		lineActor->Point2 = line.P2;
		lineActor->SetIsHUD(false);
		lineActor->MakeGeometry();
		lineActor->ToggleForDrawingRender(true);
		lineActor->UpdateLineVisuals(true, line.GetDDThickness() * LineScalefactor, line.GetLineShadeAsColor());
		lineActor->GetStaticMeshComponent()->SetMaterial(0, bInPlane ? GameMode->LineUnculledMaterial : GameMode->LineMaterial);
		LineActors.Add(lineActor);
		CaptureComponent->ShowOnlyActorComponents(lineActor);
	}
}

void ADrawingDesignerRender::EmptyLines()
{
	for (auto* line: LineActors)
	{
		DrawingDesignerRenderControl->FreeLineActor(line);
	}
	LineActors.Empty();
}

void ADrawingDesignerRender::SetupRenderTarget(int32 ImageWidth)
{
	int32 orthoWidth = ViewTransform.GetScale3D().X;
	int32 orthoHeight = ViewTransform.GetScale3D().Y;
	if (!ensure(orthoWidth != 0))
	{
		return;
	}

	int32 imageHeight = ImageWidth * orthoHeight / orthoWidth;

	// Scale lines down for larger renders.
	LineScalefactor = FMath::Min(5000.0f / ImageWidth, 8.0f);

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

	FTransform cameraTransform(ViewTransform.GetRotation(), ViewTransform.GetTranslation() + -CaptureActorOffset * ViewTransform.TransformVectorNoScale(FVector::XAxisVector));
	CaptureComponent->SetWorldTransform(cameraTransform);
	CaptureComponent->OrthoWidth = orthoWidth;
}

void ADrawingDesignerRender::RenderImage(AMOICutPlane* CutPlane, float MinLength)
{
	if (!ensure(RenderTarget) || !ensure(DrawingDesignerRenderControl))
	{
		return;
	}

	AEditModelPlayerController* controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (!ensure(controller))
	{
		return;
	}

	if (!GameMode.IsValid())
	{
		GameMode = controller->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
	}

	CaptureComponent->ClearShowOnlyComponents();

	const FVector viewDirection = CutPlane->GetNormal();
	InPlaneOffset = -0.5f * CaptureActorOffset * viewDirection;
	RenderFfe();
	
	AddObjects(viewDirection, MinLength);

	AddInPlaneObjects(CutPlane, MinLength);

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
				meshComponents.Append(dynamicActor->ProceduralSubLayerCaps);
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

	AddLines(sceneLines, false);
}

void ADrawingDesignerRender::AddInPlaneObjects(AMOICutPlane* CutPlane, float MinLength)
{
	if (!ensure(CutPlane->GetNumCorners() == 4))
	{
		return;
	}

	FModumateDDRenderDraw::DrawCallback lineCallback = [this](FVector Start, FVector End, FModumateLayerType Layer)
	{
		AddInPlaneLines(Start, End, Layer);
	};

	FModumateDDRenderDraw lineCreator(CutPlane->DrawingInterface, lineCallback);
	if (!CutPlane->PreviewHUDLines)
	{	// Trigger an update of the HUD lines.
		CutPlane->UpdateDraftingPreview(true);
	}

	CutPlane->PreviewHUDLines->Draw(&lineCreator);
}

void ADrawingDesignerRender::RestoreObjects()
{
	for (auto& meshKvp : SceneMeshComponents)
	{
		meshKvp.Key->SetCustomDepthStencilValue(meshKvp.Value);
		meshKvp.Key->SetRenderCustomDepth(meshKvp.Value != 0);
	}
}

void ADrawingDesignerRender::AddInPlaneLines(FVector P0, FVector P1, FModumateLayerType Layer)
{
	FDrawingDesignerLine line(P0 + InPlaneOffset, P1 + InPlaneOffset);
	int32 intLayer = int32(Layer);
	intLayer = FMath::Clamp(intLayer, 0, int32(sizeof(LayerToDDParams) / sizeof(LayerToDDParams[0]) ));
	line.Thickness = LayerToDDParams[intLayer].Thickness;
	line.GreyValue = LayerToDDParams[intLayer].GreyValue / 255.0;
	AddLines({ line }, true);
}
