// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerRender.h"

#include "Engine/World.h"
#include "ImageUtils.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateRayTracingSettings.h"
#include "DrawingDesigner/DrawingDesignerLine.h"
#include "DrawingDesigner/DrawingDesignerView.h"
#include "DrawingDesigner/DrawingDesignerRenderControl.h"
#include "DrawingDesigner/ModumateDDRenderDraw.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Objects/CutPlane.h"
#include "Objects/DesignOption.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/SkyActor.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/AxesActor.h"
#include "UnrealClasses/EditModelPlayerState.h"

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

void ADrawingDesignerRender::SetDocument(UModumateDocument* InDoc, FDrawingDesignerRenderControl* RenderControl)
{
	Doc = InDoc;
	DrawingDesignerRenderControl = RenderControl;
}

void ADrawingDesignerRender::AddLines(const TArray<FDrawingDesignerLine>& Lines, bool bInPlane)
{
	const FVector cameraOrigin(ViewTransform.GetLocation());
	const FVector cameraDirection(ViewTransform.TransformVector(FVector::ForwardVector).GetSafeNormal());

	for (const auto& line : Lines)
	{
		FVector P1 = line.P1;
		FVector P2 = line.P2;
		if (!bInPlane)
		{
			// Clip lines against cut plane:
			double d1 = cameraDirection | (P1 - cameraOrigin);
			double d2 = cameraDirection | (P2 - cameraOrigin);
			if (d1 < 0.0 && d2 < 0.0)
			{
				continue;
			}
			if (d1 * d2 < 0.0)
			{   // Line straddles cut plane
				if (d2 < 0.0)
				{
					P2 = P1 + (d1 / (d1 - d2)) * (P2 - P1);
				}
				else
				{
					P1 = P2 + (d2 / (d2 - d1)) * (P1 - P2);
				}
			}
		}

		ALineActor* lineActor = DrawingDesignerRenderControl->GetLineActor();
		lineActor->Point1 = P1;
		lineActor->Point2 = P2;
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

void ADrawingDesignerRender::RenderImage(AMOICutPlane* CutPlane, float MinLength, const TArray<int32>* DesignOptions /*= nullptr*/)
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
	SceneStaticMaterialMap.Empty();
	SceneMeshComponents.Empty();
	HiddenObjects.Empty();

	ExistingVisibility.Empty();

	const FVector viewDirection = CutPlane->GetNormal();
	InPlaneOffset = -0.5f * CaptureActorOffset * viewDirection;

	TSet<int32> designOptionsSet;
	if (DesignOptions)
	{
		designOptionsSet.Append(*DesignOptions);
	}

	FillHiddenList(&designOptionsSet);

	RenderFfe();
	
	AddObjects(viewDirection, MinLength);

	AddInPlaneObjects(CutPlane, MinLength);

	CaptureComponent->TextureTarget = RenderTarget;

	CaptureComponent->CaptureScene();

	if (bRayTracingEnabled)
	{	// Re-enable ray tracing.
		bRayTracingEnabled = false;
		UModumateRayTracingSettings* rtSettings = NewObject<UModumateRayTracingSettings>();
		APostProcessVolume* ppv = Cast<APostProcessVolume>(UGameplayStatics::GetActorOfClass(GetWorld(), APostProcessVolume::StaticClass()));

		if (rtSettings && ppv)
		{
			const auto* playerController = Cast<AEditModelPlayerController>(GetGameInstance<UModumateGameInstance>()->GetFirstLocalPlayerController());
			const auto* playerState = playerController->GetPlayerState<AEditModelPlayerState>();
			rtSettings->Init();
			rtSettings->SetRayTracingEnabled(ppv, true);
			rtSettings->SetExposure(ppv, playerState->RayTracingExposure);
			rtSettings->ApplyRayTraceQualitySettings(ppv, playerState->RayTracingQuality);
		}
	}

	RestoreFfeMaterials();
	RestoreObjects();

	EmptyLines();
	SceneStaticMaterialMap.Empty();
	SceneMeshComponents.Empty();
	HiddenObjects.Empty();
	ExistingVisibility.Empty();

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
	TArray<AModumateObjectInstance*> ffeObjects = Doc->GetObjectsOfType(EObjectType::OTFurniture);
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

	IConsoleVariable* rayTracingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Shadows"));
	bRayTracingEnabled = rayTracingCVar->AsVariableInt()->GetValueOnAnyThread() != 0;

	// Disable ray tracing.
	UModumateRayTracingSettings* rtSettings = NewObject<UModumateRayTracingSettings>();
	APostProcessVolume* ppv = Cast<APostProcessVolume>(UGameplayStatics::GetActorOfClass(GetWorld(), APostProcessVolume::StaticClass()));
	if (bRayTracingEnabled)
	{
		if (ensure(ppv != nullptr && rtSettings != nullptr))
		{
			rtSettings->Init();
			rtSettings->SetRayTracingEnabled(ppv, false);
		}
	}

	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	for (auto* ffe: ffeObjects)
	{
		AActor* actor = ffe->GetActor();
		actor->SetActorHiddenInGame(false);
		
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
	UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), FfeRenderTarget, TEXT("/Modumate"), TEXT("ffedepthrender.png"));
#endif
}

void ADrawingDesignerRender::RestoreFfeMaterials()
{
	TArray<AModumateObjectInstance*> ffeObjects = Doc->GetObjectsOfType(EObjectType::OTFurniture);
	for (auto* ffe: ffeObjects)
	{
		bool visible, collidable = false;
		ffe->GetUpdatedVisuals(visible, collidable);
		
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
	TArray<AModumateObjectInstance*> sceneObjects = static_cast<UModumateDocument*>(Doc)->GetObjectsOfType(RenderedObjectTypes);
	TArray<FDrawingDesignerLine> sceneLines;

	for (auto* moi : sceneObjects)
	{
		if (!HiddenObjects.Contains(moi))
		{
			ACompoundMeshActor* compoundActor = Cast<ACompoundMeshActor>(moi->GetActor());
			if (compoundActor)
			{
				ExistingVisibility.Add(compoundActor, !compoundActor->IsHidden());
				compoundActor->SetActorHiddenInGame(false);
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
				ADynamicMeshActor* dynamicActor = Cast<ADynamicMeshActor>(moi->GetActor());
				if (dynamicActor)
				{
					ExistingVisibility.Add(dynamicActor, !dynamicActor->IsHidden());
					dynamicActor->SetActorHiddenInGame(false);

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
	for (const auto& meshKvp : SceneMeshComponents)
	{
		meshKvp.Key->SetCustomDepthStencilValue(meshKvp.Value);
		meshKvp.Key->SetRenderCustomDepth(meshKvp.Value != 0);
	}
	for (const auto& meshKvp : ExistingVisibility)
	{
		meshKvp.Key->SetActorHiddenInGame(!meshKvp.Value);
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

void ADrawingDesignerRender::FillHiddenList(const TSet<int32>* OptionsOverride /*= nullptr*/)
{
	// Find all objects that should not be rendered according to current design options.
	TSet<int32> allDesignGroups;
	TSet<int32> shownDesignGroups;
	TArray<const AMOIDesignOption*> designOptions;
	Doc->GetObjectsOfTypeCasted(EObjectType::OTDesignOption, designOptions);
	for (auto* designOption : designOptions)
	{
		allDesignGroups.Append(designOption->InstanceData.groups);
		if ((!OptionsOverride && designOption->InstanceData.isShowing) || (OptionsOverride && OptionsOverride->Contains(designOption->ID)) )
		{
			shownDesignGroups.Append(designOption->InstanceData.groups);
		}
	}

	TSet<int32> hiddenDesignGroups(allDesignGroups.Difference(shownDesignGroups));

	HiddenObjects.Empty();
	for (int32 groupID : hiddenDesignGroups)
	{
		UModumateObjectStatics::GetObjectsInGroups(Doc, { groupID }, HiddenObjects);
	}

}
