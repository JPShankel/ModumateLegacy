// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateIconMeshStatics.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"

using namespace Modumate;

bool UModumateIconMeshStatics::GetMeshesFromShoppingItem(AEditModelPlayerController_CPP *Controller, const FBIMKey& AsmKey, EToolMode FromToolMode, TArray<UStaticMesh*>& TargetComps, bool bMarketplaceAsm)
{
	FModumateDatabase *obDB = Controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;

	AEditModelGameState_CPP* gameState = Cast<AEditModelGameState_CPP>(Controller->GetWorld()->GetGameState());
	const FPresetManager& presetManager = gameState->Document.PresetManager;

	const FBIMAssemblySpec* assembly = presetManager.GetAssemblyByKey(FromToolMode, AsmKey);

	if (assembly == nullptr)
	{
		return false;
	}

	TargetComps.Empty();
	for (int32 i = 0; i < assembly->Parts.Num(); ++i)
	{
		TargetComps.Add(assembly->Parts[i].Mesh.EngineMesh.Get());
	}
	return true;
}

bool UModumateIconMeshStatics::GetEngineMaterialByKey(AEditModelPlayerController_CPP *Controller, const FBIMKey& Key, UMaterialInterface* &ReturnMaterial)
{
	if (Controller != nullptr)
	{
		FModumateDatabase *db = Controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
		const FArchitecturalMaterial *mat = db->GetArchitecturalMaterialByKey(Key);
		if (ensureAlways(mat != nullptr) && ensureAlways(mat->EngineMaterial.IsValid()))
		{
			ReturnMaterial = mat->EngineMaterial.Get();
			return true;
		}
	}

	return false;
}

bool UModumateIconMeshStatics::GetEngineCustomColorByKey(AEditModelPlayerController_CPP *Controller, const FBIMKey& Key, FCustomColor &ModuleColor)
{
	if (Controller != nullptr)
	{
		FModumateDatabase *db = Controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
		const FCustomColor *color = db->GetCustomColorByKey(Key);
		if (ensureAlways(color != nullptr))
		{
			ModuleColor = *color;
			return true;
		}
	}

	return false;
}

bool UModumateIconMeshStatics::GetEngineStaticIconTextureByKey(AEditModelPlayerController_CPP *Controller, const FBIMKey& Key, FStaticIconTexture &StaticIcon)
{
	if (Controller != nullptr)
	{
		FModumateDatabase *db = Controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
		const FStaticIconTexture *icon = db->GetStaticIconTextureByKey(Key);
		if (ensureAlways(icon != nullptr))
		{
			StaticIcon = *icon;
			return true;
		}
	}

	return false;
}

bool UModumateIconMeshStatics::MakeIconMeshFromPofileKey(AEditModelPlayerController_CPP *Controller, ADynamicMeshActor *DynamicMeshActor, EToolMode FromToolMode, const FBIMKey& ProfileKey, const FVector &RootLoation, float Length)
{
	if (Controller != nullptr)
	{
		FModumateDatabase *db = Controller->GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase;
		const FSimpleMeshRef* meshRef = db->GetSimpleMeshByKey(ProfileKey);
		
		if (meshRef != nullptr)
		{
			// Add temp assembly that uses the profile
			FBIMAssemblySpec obAsm;
			FBIMExtrusionSpec obAsmExtrusion;
			obAsmExtrusion.SimpleMeshes.Add(*meshRef);
			obAsm.Extrusions.Add(obAsmExtrusion);

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

bool UModumateIconMeshStatics::GetEngineMaterialByPresetKey(UObject* WorldContextObject, const FBIMKey& PresetKey, UMaterialInterface* &ModuleMaterial, FCustomColor &ModuleColor)
{
	if (PresetKey.IsNone())
	{
		return false;
	}

	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState_CPP *gameState = world ? Cast<AEditModelGameState_CPP>(world->GetGameState()) : nullptr;
	FModumateDatabase *db = world ? world->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase : nullptr;
	if (gameState == nullptr && db == nullptr)
	{
		return false;
	}
	const FPresetManager &presetManager = gameState->Document.PresetManager;

	FBIMAssemblySpec presetSpec;
	presetSpec.FromPreset(*db,presetManager.CraftingNodePresets, PresetKey);

	FString materialName, colorName;

	if (presetSpec.RootProperties.TryGetProperty(EBIMValueScope::Assembly, BIMPropertyNames::MaterialKey, materialName))
	{
		const FArchitecturalMaterial *mat = db->GetArchitecturalMaterialByKey(FBIMKey(materialName));
		if (ensureAlways(mat != nullptr) && ensureAlways(mat->EngineMaterial.IsValid()))
		{
			ModuleMaterial = mat->EngineMaterial.Get();
		}
	}

	if (presetSpec.RootProperties.TryGetProperty(EBIMValueScope::Assembly, BIMPropertyNames::Color, colorName))
	{
		const FCustomColor *color = db->GetCustomColorByKey(FBIMKey(colorName));
		if (ensureAlways(color != nullptr))
		{
			ModuleColor = *color;
		}
	}

	return true;
}