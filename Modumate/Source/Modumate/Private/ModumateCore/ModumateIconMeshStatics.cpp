// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateIconMeshStatics.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateCraftingWidget_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelGameState_CPP.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "ModumateDimensionStatics.h"

using namespace Modumate;

bool UModumateIconMeshStatics::GetWallIconParamsFromShopItem(AActor* IconGenerator, UModumateCraftingWidget_CPP* CraftingWidget, const FName &AssemblyKey, FVector RefCP1, FVector RefCP2, TArray<FProceduralMeshParams>& ProceduralMeshParam, TArray<UMaterialInterface*>& Materials, float& WallThickness, float& LayerHeight, FVector& CP1, FVector& CP2, EToolMode mode, bool ManualLayerCPs, float UVScale, bool bMarketplaceAsm)
{
	TArray<FModumateObjectAssemblyLayer> wallLayers;
	if (CraftingWidget)
	{
		if (AssemblyKey == TEXT("Preview"))
		{
			wallLayers = CraftingWidget->PreviewAssembly.Layers;
		}
		else
		{
			wallLayers = CraftingWidget->CraftingAssembly.Layers;
		}
	}
	else
	{
		AEditModelPlayerController_CPP *controller = Cast<AEditModelPlayerController_CPP>(IconGenerator->GetWorld()->GetFirstPlayerController());
		ModumateObjectDatabase *obDB = controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
		FModumateDocument *doc = &controller->GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

		const FModumateObjectAssembly *assembly = bMarketplaceAsm ? obDB->PresetManager.GetAssemblyByKey(mode, AssemblyKey) : doc->PresetManager.GetAssemblyByKey(mode, AssemblyKey);

		if (AssemblyKey == TEXT("temp"))
		{
			const FModumateObjectAssembly *tempAssembly = controller->EMPlayerState->GetTemporaryAssembly(mode);
			if (ensureAlways(tempAssembly != nullptr))
			{
				wallLayers = tempAssembly->Layers;
			}
		}
		else if (assembly != nullptr)
		{
			wallLayers = assembly->Layers;
		}
		else
		{
			return false;
		}
	}

	if (wallLayers.Num() > 0)
	{
		float worldWallThickness = 0.f;
		for (auto curLayer : wallLayers)
		{
			worldWallThickness += curLayer.Thickness.AsWorldCentimeters();
		}

		float worldLayerHeight = 121.92;
		float halfWorldLayerLength = 91.44f;
		LayerHeight = worldLayerHeight;
		WallThickness = worldWallThickness;
		FVector originLocation = (RefCP1 + RefCP2) * 0.5f;
		//(To - From).GetSafeNormal()
		FVector worldCp1 = ((RefCP1 - RefCP2).GetSafeNormal() * halfWorldLayerLength) + originLocation;
		FVector worldCp2 = ((RefCP2 - RefCP1).GetSafeNormal() * halfWorldLayerLength * 2.f) + worldCp1;
		CP1 = worldCp1; CP2 = worldCp2;

		// Get Material here with but let blueprint get material via FindLayer
		TArray<UMaterialInterface*> wallMaterials;
		for (auto curLayer : wallLayers)
		{
			TWeakObjectPtr<UMaterialInterface> curWallMaterialPtr = curLayer.Material.EngineMaterial;
			if (curWallMaterialPtr.IsValid())
			{
				wallMaterials.Add(curWallMaterialPtr.Get());
			}
		}
		Materials = wallMaterials;

		TArray<FProceduralMeshParams> returnParams;
		TArray<FVector> controlPoints = { worldCp1, worldCp2 };
		TArray<FWallAssemblyLayerControlPoints> wallLayerCPs = UModumateFunctionLibrary::GetWallAssemblyLayerControlPoints(controlPoints, wallLayers, worldLayerHeight, ManualLayerCPs);

		for (int32 i = 0; i < wallLayerCPs.Num(); ++i)
		{
			TArray<FVector> returnVerts;
			TArray<int32> returnTris;
			TArray<FVector> returnNormals;
			TArray<FVector2D> returnUVs;
			UModumateFunctionLibrary::GetWallVertsParamFromAssemblyLayerControlPoints(worldCp1, wallLayerCPs[i], IconGenerator, returnVerts, returnTris, returnNormals, returnUVs);
			if (UVScale != 1.f)
			{
				for (int32 j = 0; j < returnUVs.Num(); ++j)
				{
					returnUVs[j] = returnUVs[j] * UVScale;
				}
			}
			FProceduralMeshParams curParam = FProceduralMeshParams{ returnVerts, returnTris, returnNormals, returnUVs };
			returnParams.Add(curParam);
		}
		ProceduralMeshParam = returnParams;
		return true;
	}
	else
	{
		return false;
	}
}

bool UModumateIconMeshStatics::GetFloorIconParamsFromShopItem(AActor* IconGenerator, EToolMode FromToolMode, UModumateCraftingWidget_CPP* CraftingWidget, const FName &AssemblyKey, FVector Offset, float FloorStartHeight, TArray<FProceduralMeshParams>& ProceduralMeshParam, TArray<UMaterialInterface*>& Materials, float& FloorThickness, float& LayerDepth, FVector& CP1, FVector& CP2, FVector& CP3, FVector& CP4, bool ManualLayerCPs, bool bMarketplaceAsm)
{
	TArray<FModumateObjectAssemblyLayer> floorLayers;
	TArray<FModumateObjectAssemblyLayer> tempFloorLayers;
	if (CraftingWidget)
	{
		if (AssemblyKey == TEXT("Preview"))
		{
			tempFloorLayers = CraftingWidget->PreviewAssembly.Layers;
		}
		else
		{
			tempFloorLayers = CraftingWidget->CraftingAssembly.Layers;
		}
	}
	else
	{
		AEditModelPlayerController_CPP *controller = Cast<AEditModelPlayerController_CPP>(IconGenerator->GetWorld()->GetFirstPlayerController());

		Modumate::ModumateObjectDatabase *db = controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
		Modumate::FModumateDocument &doc = controller->GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

		const FModumateObjectAssembly *assembly = bMarketplaceAsm ? db->PresetManager.GetAssemblyByKey(FromToolMode, AssemblyKey) : doc.PresetManager.GetAssemblyByKey(FromToolMode, AssemblyKey);

		if (AssemblyKey == TEXT("temp"))
		{
			const FModumateObjectAssembly *tempAssembly = controller->EMPlayerState->GetTemporaryAssembly(FromToolMode);
			if (tempAssembly != nullptr)
			{
				tempFloorLayers = tempAssembly->Layers;
			}
		}
		else if (assembly != nullptr)
		{
			tempFloorLayers = assembly->Layers;
		}
		else
		{
			return false;
		}
	}
	if (tempFloorLayers.Num() > 0)
	{
		float sumThickness = 0.f;
		TArray<float> allThickenss;
		for (int32 i = 0; i < tempFloorLayers.Num(); ++i)
		{
			allThickenss.Add(tempFloorLayers[i].Thickness.AsWorldCentimeters());
		}
		// min. thickness for floor layers, creating both original layer thickness and new thickness
		int32 mostThickID = 0;
		FMath::Max(allThickenss, &mostThickID);
		if (tempFloorLayers.Num() == 1)
		{
			FloorThickness = FMath::Clamp(tempFloorLayers[0].Thickness.AsWorldCentimeters(), 2.54f, 304.8f); // Clamp thickness
			tempFloorLayers[0].Thickness = Units::FUnitValue::WorldCentimeters(FloorThickness);
			floorLayers.Add(tempFloorLayers[0]);
			if (FromToolMode == EToolMode::VE_COUNTERTOP)
			{
				LayerDepth = 60.96f;
				float floorLength = 60.96f;
				CP1 = FVector(floorLength*-1.f, floorLength*-1.f, FloorStartHeight) + Offset;
				CP2 = FVector(floorLength, floorLength*-1.f, FloorStartHeight) + Offset;
				CP3 = FVector(floorLength, floorLength, FloorStartHeight) + Offset;
				CP4 = FVector(floorLength*-1.f, floorLength, FloorStartHeight) + Offset;
			}
			else
			{
				LayerDepth = FloorThickness * 2.f;
				float floorLength = 30.48f;
				CP1 = FVector(floorLength*-2.f, floorLength*-2.f, 0.f) + Offset;
				CP2 = FVector(floorLength*2.f, floorLength*-2.f, 0.f) + Offset;
				CP3 = FVector(floorLength*2.f, floorLength, 0.f) + Offset;
				CP4 = FVector(floorLength*-2.f, floorLength, 0.f) + Offset;
			}
		}
		else
		{
			floorLayers = tempFloorLayers;
			FloorThickness = sumThickness;
			LayerDepth = FloorThickness * 2.f;
			float floorLength = 30.48f;
			CP1 = FVector(floorLength*-2.f, floorLength*-2.f, 0.f) + Offset;
			CP2 = FVector(floorLength*2.f, floorLength*-2.f, 0.f) + Offset;
			CP3 = FVector(floorLength*2.f, floorLength, 0.f) + Offset;
			CP4 = FVector(floorLength*-2.f, floorLength, 0.f) + Offset;
		}

		// Save floor materials for blueprint to apply on meshes
		TArray<UMaterialInterface*> floorMaterials;
		if (ManualLayerCPs)
		{
			TArray<FModumateObjectAssemblyLayer> reversedfloorLayers;
			for (int32 i = floorLayers.Num() - 1; i >= 0; i--)
			{
				reversedfloorLayers.Add(floorLayers[i]);
			}
			floorLayers = reversedfloorLayers;
		}
		for (int32 i = floorLayers.Num() - 1; i >= 0; i--)
		{
			TWeakObjectPtr<UMaterialInterface> curFloorMaterialPtr = floorLayers[i].Material.EngineMaterial;
			if (curFloorMaterialPtr.IsValid())
			{
				floorMaterials.Add(curFloorMaterialPtr.Get());
			}
		}
		Materials = floorMaterials;

		TArray<FProceduralMeshParams> returnParams;
		TArray<FVector> floorPoints = { CP1, CP2, CP3, CP4 };
		TArray<FFloorAssemblyLayerControlPoints> floorLayerCPs;
		UModumateFunctionLibrary::GetFloorAssemblyLayerControlPoints(floorPoints, floorLayers, floorLayerCPs, true, ManualLayerCPs);
		for (int32 i = 0; i < floorLayerCPs.Num(); ++i)
		{
			TArray<FVector> returnVerts;
			TArray<int32> returnTris;
			TArray<int32> returnTopTris;
			TArray<FVector> returnNormals;
			TArray<FVector2D> returnUVs;
			UModumateFunctionLibrary::CalculateFloorParam(IconGenerator->GetActorLocation(), floorLayerCPs[i].TopLayerControlPoints, IconGenerator->GetActorLocation(), FRotator::ZeroRotator, floorLayerCPs[i].TopLayerControlPoints, floorLayerCPs[i].BottomLayerControlPoints,
				returnVerts, returnTris, returnNormals, returnUVs, returnTopTris);
			FProceduralMeshParams curParam = FProceduralMeshParams{ returnVerts, returnTris, returnNormals, returnUVs };
			returnParams.Add(curParam);
		}
		ProceduralMeshParam = returnParams;
		return true;
	}
	else
	{
		return false;
	}
}

bool UModumateIconMeshStatics::MakeCompoundMeshFromShoppingItem(AActor* IconGenerator, const FName &AssemblyKey, EToolMode FromToolMode, float& TotalWidth, UModumateCraftingWidget_CPP* CraftingWidget, bool bMarketplaceAsm)
{
	FModumateObjectAssembly obAsm;
	if (CraftingWidget)
	{
		if (AssemblyKey == TEXT("Preview"))
		{
			obAsm = CraftingWidget->PreviewAssembly;
		}
		else
		{
			obAsm = CraftingWidget->CraftingAssembly;
		}
	}
	else
	{
		UWorld *world = IconGenerator ? IconGenerator->GetWorld() : nullptr;
		AEditModelGameState_CPP *gameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
		if (gameState == nullptr)
		{
			return false;
		}

		ModumateObjectDatabase *obDB = world->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;

		const FModumateObjectAssembly *obAsmPtr = bMarketplaceAsm ?
			obDB->PresetManager.GetAssemblyByKey(FromToolMode, AssemblyKey) :
			gameState->GetAssemblyByKey_DEPRECATED(FromToolMode, AssemblyKey);

		if (obAsmPtr)
		{
			obAsm = *obAsmPtr;
		}
		else
		{
			if (AssemblyKey == TEXT("temp"))
			{
				AEditModelPlayerController_CPP *controller = Cast<AEditModelPlayerController_CPP>(IconGenerator->GetWorld()->GetFirstPlayerController());
				const FModumateObjectAssembly *pAsm = controller->EMPlayerState->GetTemporaryAssembly(FromToolMode);
				if (ensureAlways(pAsm != nullptr))
				{
					obAsm = *pAsm;
				}
			}
			else
			{
				return false;
			}
		}
	}


	ACompoundMeshActor *attachedCompoundMeshActor = nullptr;
	TArray<AActor*> attachedActors;
	IconGenerator->GetAttachedActors(attachedActors);

	for (AActor *attachedActor : attachedActors)
	{
		attachedCompoundMeshActor = Cast<ACompoundMeshActor>(attachedActor);
		if (attachedCompoundMeshActor)
		{
			break;
		}
	}

	if (attachedCompoundMeshActor)
	{
		attachedCompoundMeshActor->MakeFromAssembly(obAsm, FVector::OneVector, false, false);
		return true;
	}

	return false;
}

bool UModumateIconMeshStatics::MakeCabinetFromShoppingItem(AActor* IconGenerator, const FName &AsmKey, TArray<FVector>& CabinetPointRefs, UModumateCraftingWidget_CPP* CraftingWidget, bool bMarketplaceAsm)
{
	FModumateObjectAssembly obAsm;

	if (CraftingWidget)
	{
		if (AsmKey == TEXT("Preview"))
		{
			obAsm = CraftingWidget->PreviewAssembly;
		}
		else
		{
			obAsm = CraftingWidget->CraftingAssembly;
		}
	}
	else
	{
		UWorld *world = IconGenerator ? IconGenerator->GetWorld() : nullptr;
		AEditModelGameState_CPP *gameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
		if (gameState == nullptr)
		{
			return false;
		}

		const EToolMode toolMode = EToolMode::VE_CABINET;
		FName key = AsmKey;

		Modumate::ModumateObjectDatabase *obDB = world->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;

		const FModumateObjectAssembly *obAsmPtr = bMarketplaceAsm ?
			obDB->PresetManager.GetAssemblyByKey(toolMode, AsmKey) :
			gameState->GetAssemblyByKey_DEPRECATED(toolMode, AsmKey);
		if (obAsmPtr)
		{
			obAsm = *obAsmPtr;
		}
		else
		{
			return false;
		}

		if (obAsm.ObjectType != EObjectType::OTCabinet)
		{
			return false;
		}
	}

	ACompoundMeshActor *compoundMeshActor = nullptr;
	ADynamicMeshActor *dynamicMeshActor = nullptr;
	TArray<AActor*> attachedActors;
	IconGenerator->GetAttachedActors(attachedActors);

	for (AActor *attachedActor : attachedActors)
	{
		if (compoundMeshActor == nullptr)
		{
			if (ACompoundMeshActor *attachedCompoundMeshActor = Cast<ACompoundMeshActor>(attachedActor))
			{
				compoundMeshActor = attachedCompoundMeshActor;
			}
		}

		if (dynamicMeshActor == nullptr)
		{
			if (ADynamicMeshActor *attachedDynamicMeshActor = Cast<ADynamicMeshActor>(attachedActor))
			{
				dynamicMeshActor = attachedDynamicMeshActor;
			}
		}
	}

	if ((compoundMeshActor == nullptr) || (dynamicMeshActor == nullptr))
	{
		return false;
	}

	// Now that we have a cabinet assembly, a DynamicMeshActor, and CompoundMeshActor,
	// we can make a fake cabinet for icon generation the same way that FMOICabinetImpl does.

	// Get the toe kick dimensions from the assembly
	FVector2D toeKickDimensions;
	UModumateFunctionLibrary::GetCabinetToeKickDimensions(obAsm, toeKickDimensions);

	// Get the exterior finish material for the cabinet
	static const FName cabinetGeomMatName(TEXT("Cabinet_Exterior_Finish"));
	FArchitecturalMaterial materialData;
	if (obAsm.PortalConfiguration.IsValid())
	{
		materialData = obAsm.PortalConfiguration.MaterialsPerChannel.FindRef(cabinetGeomMatName);
	}

	// Get the cabinet dimensions
	Units::FUnitValue cabinetWidthUnit(30.0f, Units::EUnitType::WorldInches);
	Units::FUnitValue cabinetHeightUnit(34.5f, Units::EUnitType::WorldInches);
	Units::FUnitValue cabinetDepthUnit(24.0f, Units::EUnitType::WorldInches);

	if (obAsm.PortalConfiguration.IsValid())
	{
		cabinetWidthUnit = obAsm.PortalConfiguration.ReferencePlanes[FPortalConfiguration::RefPlaneNameMaxX].FixedValue;
		cabinetHeightUnit = obAsm.PortalConfiguration.ReferencePlanes[FPortalConfiguration::RefPlaneNameMaxZ].FixedValue;
	}

	float cabinetWidth = cabinetWidthUnit.AsWorldCentimeters();
	float cabinetHeight = cabinetHeightUnit.AsWorldCentimeters();
	float cabinetDepth = cabinetDepthUnit.AsWorldCentimeters();

	// Make the cabinet base + box geometry
	TArray<FVector> cabinetCPs = {
		FVector::ZeroVector,
		FVector(cabinetWidth, 0.0f, 0.0f),
		FVector(cabinetWidth, cabinetDepth, 0.0f),
		FVector(0.0f, cabinetDepth, 0.0f),
	};
	CabinetPointRefs = cabinetCPs;
	dynamicMeshActor->SetupCabinetGeometry(cabinetCPs, cabinetHeight, materialData, toeKickDimensions, 2);

	// Now make the cabinet portal geometry
	if (obAsm.PortalConfiguration.IsValid())
	{
		// Update the reference planes so the portal 9-slicing is correct
		TMap<FName, FPortalReferencePlane> &refPlanes = obAsm.PortalConfiguration.ReferencePlanes;
		refPlanes[FPortalConfiguration::RefPlaneNameMinX].FixedValue = Units::FUnitValue::WorldCentimeters(0.0f);
		refPlanes[FPortalConfiguration::RefPlaneNameMaxX].FixedValue = Units::FUnitValue::WorldCentimeters(cabinetWidth);
		refPlanes[FPortalConfiguration::RefPlaneNameMinZ].FixedValue = Units::FUnitValue::WorldCentimeters(0.0f);
		refPlanes[FPortalConfiguration::RefPlaneNameMaxZ].FixedValue = Units::FUnitValue::WorldCentimeters(cabinetHeight - toeKickDimensions.Y);
		obAsm.PortalConfiguration.CacheRefPlaneValues();
	}

	compoundMeshActor->MakeFromAssembly(obAsm, FVector::OneVector, false, false);

	if (obAsm.PortalConfiguration.IsValid())
	{
		// Now position the portal where it's supposed to go
		FVector portalOrigin = cabinetCPs[3] + (toeKickDimensions.Y * FVector::UpVector);
		FQuat portalRot = FRotationMatrix::MakeFromYZ(FVector(0.0f, 1.0f, 0.0f), FVector::UpVector).ToQuat();

		compoundMeshActor->SetActorLocationAndRotation(portalOrigin, portalRot);
	}

	return true;
}

bool UModumateIconMeshStatics::MakeExtrudedPolyFromShoppingItem(AActor* iconGenerator, EToolMode fromToolMode, const FName &asmKey, const FVector &rootLoation, UModumateCraftingWidget_CPP* CraftingWidget, float length, bool bMarketplaceAsm)
{
	FModumateObjectAssembly obAsm;

	if (CraftingWidget)
	{
		if (asmKey == TEXT("Preview"))
		{
			obAsm = CraftingWidget->PreviewAssembly;
		}
		else
		{
			obAsm = CraftingWidget->CraftingAssembly;
		}
	}
	else
	{
		UWorld *world = iconGenerator ? iconGenerator->GetWorld() : nullptr;
		AEditModelGameState_CPP *gameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
		if (gameState == nullptr)
		{
			return false;
		}

		Modumate::ModumateObjectDatabase *obDB = world->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
		const FModumateObjectAssembly *obAsmPtr = bMarketplaceAsm ? obDB->PresetManager.GetAssemblyByKey(fromToolMode, asmKey) : gameState->GetAssemblyByKey_DEPRECATED(fromToolMode, asmKey);

		if (obAsmPtr)
		{
			obAsm = *obAsmPtr;
		}
		else
		{
			return false;
		}
	}

	ACompoundMeshActor *compoundMeshActor = nullptr;
	ADynamicMeshActor *dynamicMeshActor = nullptr;
	TArray<AActor*> attachedActors;
	iconGenerator->GetAttachedActors(attachedActors);

	for (AActor *attachedActor : attachedActors)
	{
		if (compoundMeshActor == nullptr)
		{
			if (ACompoundMeshActor *attachedCompoundMeshActor = Cast<ACompoundMeshActor>(attachedActor))
			{
				compoundMeshActor = attachedCompoundMeshActor;
			}
		}

		if (dynamicMeshActor == nullptr)
		{
			if (ADynamicMeshActor *attachedDynamicMeshActor = Cast<ADynamicMeshActor>(attachedActor))
			{
				dynamicMeshActor = attachedDynamicMeshActor;
			}
		}
	}

	if ((compoundMeshActor == nullptr) || (dynamicMeshActor == nullptr))
	{
		return false;
	}

	// Now that we have an assembly, a DynamicMeshActor, and CompoundMeshActor,
	// we can make a mesh for icon generation.
	FVector meshStartPos = FVector(length, 0.f, 0.f) + rootLoation;
	FVector meshEndPos = FVector((length * -1.f), 0.f, 0.f) + rootLoation;
	FVector meshNormal = fromToolMode == EToolMode::VE_STRUCTURELINE ? FVector::LeftVector : FVector::RightVector;
	FVector meshUp = fromToolMode == EToolMode::VE_STRUCTURELINE ? FVector::DownVector : FVector::UpVector;

	FVector2D upperExtensions = FVector2D::ZeroVector;
	FVector2D outerExtensions = FVector2D::ZeroVector;

	FVector scaleVector;
	if (!obAsm.TryGetProperty(BIM::Parameters::Scale, scaleVector))
	{
		scaleVector = FVector::OneVector;
	}

	dynamicMeshActor->SetupExtrudedPolyGeometry(obAsm, meshStartPos, meshEndPos,
		meshNormal, meshUp, upperExtensions, outerExtensions, scaleVector, true, false);

	return true;
}

bool UModumateIconMeshStatics::GetModuleGapIconParamsFromCrafting(
	AActor* IconGenerator,
	UModumateCraftingWidget_CPP* CraftingWidget,
	EToolMode FromToolMode,
	const FString &AssemblyKey,
	FVector &ModuleExtent,
	FVector &GapExtent,
	UMaterialInterface* &ModuleMaterial,
	FCustomColor &ModuleColor,
	FCustomColor &GapColor,
	bool bMarketplaceAsm)
{
	FModumateObjectAssembly obAsm;
	if (CraftingWidget)
	{
		// Since modules don't appear in 3D crafting, use PreviewAssembly only
		obAsm = CraftingWidget->PreviewAssembly;
		if (obAsm.Layers.Num() > 0)
		{
			if (obAsm.Layers[0].Modules.Num() > 0)
			{
				ModuleExtent = obAsm.Layers[0].Modules[0].ModuleExtents;
				
				GapExtent = FVector(obAsm.Layers[0].Gap.GapExtents.X, obAsm.Layers[0].Gap.GapExtents.Y, 0.f);

				// Colors
				ModuleColor = obAsm.Layers[0].BaseColor;
				GapColor = obAsm.Layers[0].Gap.BaseColor;

				// Materials
				if (obAsm.Layers[0].Material.EngineMaterial.IsValid())
				{
					ModuleMaterial = obAsm.Layers[0].Material.EngineMaterial.Get();
				}
				return true;
			}
		}
	}
	return false;
}

bool UModumateIconMeshStatics::GetMeshesFromShoppingItem(AEditModelPlayerController_CPP *Controller, const FName &AsmKey, EToolMode FromToolMode, TArray<UStaticMesh*>& TargetComps, bool bMarketplaceAsm)
{
	ModumateObjectDatabase *obDB = Controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
	const FModumateObjectAssembly *obAsm = bMarketplaceAsm ?
		obDB->PresetManager.GetAssemblyByKey(FromToolMode, AsmKey) :
		Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>()->GetAssemblyByKey_DEPRECATED(FromToolMode, AsmKey);

	if (obAsm == nullptr)
	{
		return false;
	}
	FModumateObjectAssembly *updatedObAsmPtr = const_cast<FModumateObjectAssembly*>(obAsm);
	FModumateObjectAssembly updatedObAsm = *updatedObAsmPtr;
	TargetComps.Empty();
	for (int32 i = 0; i < updatedObAsm.Layers.Num(); ++i)
	{
		TargetComps.Add(updatedObAsm.Layers[i].Mesh.EngineMesh.Get());
	}
	return true;
}

bool UModumateIconMeshStatics::GetEngineMaterialByKey(AEditModelPlayerController_CPP *Controller, const FName &Key, UMaterialInterface* &ReturnMaterial)
{
	if (Controller != nullptr)
	{
		ModumateObjectDatabase *db = Controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
		const FArchitecturalMaterial *mat = db->GetArchitecturalMaterialByKey(Key);
		if (ensureAlways(mat != nullptr) && ensureAlways(mat->EngineMaterial.IsValid()))
		{
			ReturnMaterial = mat->EngineMaterial.Get();
			return true;
		}
	}

	return false;
}

bool UModumateIconMeshStatics::GetEngineCustomColorByKey(AEditModelPlayerController_CPP *Controller, const FName &Key, FCustomColor &ModuleColor)
{
	if (Controller != nullptr)
	{
		ModumateObjectDatabase *db = Controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
		const FCustomColor *color = db->GetCustomColorByKey(Key);
		if (ensureAlways(color != nullptr))
		{
			ModuleColor = *color;
			return true;
		}
	}

	return false;
}

bool UModumateIconMeshStatics::MakeIconMeshFromPofileKey(AEditModelPlayerController_CPP *Controller, ADynamicMeshActor *DynamicMeshActor, EToolMode FromToolMode, const FName &ProfileKey, const FVector &RootLoation, float Length)
{
	if (Controller != nullptr)
	{
		ModumateObjectDatabase *db = Controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
		const FSimpleMeshRef* meshRef = db->GetSimpleMeshByKey(ProfileKey);
		
		if (meshRef != nullptr)
		{
			// Add temp assembly that uses the profile
			FModumateObjectAssembly obAsm;
			FModumateObjectAssemblyLayer obAsmLayer;
			obAsmLayer.SimpleMeshes.Add(*meshRef);
			obAsm.Layers.Add(obAsmLayer);

			// Geometry params for setting up dynamic mesh actor
			FVector meshStartPos = FVector(Length, 0.f, 0.f) + RootLoation;
			FVector meshEndPos = FVector((Length * -1.f), 0.f, 0.f) + RootLoation;
			FVector meshNormal = FromToolMode == EToolMode::VE_STRUCTURELINE ? FVector::LeftVector : FVector::RightVector;
			FVector meshUp = FromToolMode == EToolMode::VE_STRUCTURELINE ? FVector::DownVector : FVector::UpVector;

			DynamicMeshActor->SetupExtrudedPolyGeometry(obAsm, meshStartPos, meshEndPos,
				meshNormal, meshUp, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector::OneVector, true, false);

			return true;
		}
	}
	return false;
}

bool UModumateIconMeshStatics::GetModuleIconParamsFromPreset(UObject* WorldContextObject, UModumateCraftingWidget_CPP* CraftingWidget, EToolMode FromToolMode, const FName &PresetKey, FVector &ModuleExtent, UMaterialInterface* &ModuleMaterial, FCustomColor &ModuleColor)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState_CPP *gameState = world ? Cast<AEditModelGameState_CPP>(world->GetGameState()) : nullptr;
	ModumateObjectDatabase *db = world ? world->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase : nullptr;
	if (gameState == nullptr && db == nullptr)
	{
		return false;
	}
	const FPresetManager &presetManager = gameState->Document.PresetManager;

	BIM::FModumateAssemblyPropertySpec presetSpec;
	presetManager.PresetToSpec(PresetKey, presetSpec);

	// Module extents
	FString xExtentName, yExtentName, zExtentName;
	presetSpec.RootProperties.TryGetProperty(BIM::EScope::Module, BIM::Parameters::XExtents, xExtentName);
	presetSpec.RootProperties.TryGetProperty(BIM::EScope::Module, BIM::Parameters::YExtents, yExtentName);
	presetSpec.RootProperties.TryGetProperty(BIM::EScope::Module, BIM::Parameters::ZExtents, zExtentName);

	FModumateFormattedDimension dimX = UModumateDimensionStatics::StringToFormattedDimension(xExtentName);
	FModumateFormattedDimension dimY = UModumateDimensionStatics::StringToFormattedDimension(xExtentName);
	FModumateFormattedDimension dimZ = UModumateDimensionStatics::StringToFormattedDimension(xExtentName);
	ModuleExtent = FVector(dimX.Centimeters, dimY.Centimeters, dimZ.Centimeters);

	// Material and color
	FString colorName, materialName;
	presetSpec.RootProperties.TryGetProperty(BIM::EScope::Module, BIM::Parameters::Color, colorName);
	presetSpec.RootProperties.TryGetProperty(BIM::EScope::Module, BIM::Parameters::Material, materialName);

	const FArchitecturalMaterial *mat = db->GetArchitecturalMaterialByKey(FName(*materialName));
	if (ensureAlways(mat != nullptr) && ensureAlways(mat->EngineMaterial.IsValid()))
	{
		ModuleMaterial = mat->EngineMaterial.Get();
	}
	const FCustomColor *color = db->GetCustomColorByKey(FName(*colorName));
	if (ensureAlways(color != nullptr))
	{
		ModuleColor = *color;
	}

	return true;
}
