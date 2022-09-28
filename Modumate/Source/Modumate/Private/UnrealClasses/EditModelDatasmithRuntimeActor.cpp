// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelDatasmithRuntimeActor.h"
#include "UnrealClasses/EditModelDatasmithImporter.h"
#include "DatasmithRuntimeBlueprintLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "ImageUtils.h"

// Sets default values
AEditModelDatasmithRuntimeActor::AEditModelDatasmithRuntimeActor()
{
}

void AEditModelDatasmithRuntimeActor::OnImportEnd()
{
	// The original plugin Datasmith materials use these parameters for opacity settings
	static const FName opacityParamName(TEXT("Opacity"));
	static const FName transparencyParamName(TEXT("Transparency"));

	Super::OnImportEnd();
	const auto gameMode = GetWorld()->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
	if (!gameMode)
	{
		return;
	}

	TArray<AActor*> attachedActors;
	GetAttachedActorsRecursive(this, attachedActors);

	// Search for all StaticMeshComponents created and attached from import
	for (auto curActor : attachedActors)
	{
		// The attached actors can be StaticMeshActor or UActor
		// Search for StaticMeshComponents using different methods
		AStaticMeshActor* curStaticMeshActor = Cast<AStaticMeshActor>(curActor);
		TArray<UStaticMeshComponent*> staticMeshComps;
		if (curStaticMeshActor)
		{
			staticMeshComps.Add(curStaticMeshActor->GetStaticMeshComponent());
		}
		else
		{
			TArray<UActorComponent*> actorComps;
			curActor->GetComponents<UActorComponent>(actorComps);
			for (auto curActorComp : actorComps)
			{
				UStaticMeshComponent* asMeshComp = Cast<UStaticMeshComponent>(curActorComp);
				if (asMeshComp)
				{
					staticMeshComps.Add(asMeshComp);
				}
			}
		}

		// Cache the StaticMeshes and their transforms
		for (const auto curStaticMeshComp : staticMeshComps)
		{
			UStaticMesh* mesh = curStaticMeshComp->GetStaticMesh();
			if (mesh)
			{
				StaticMeshRefs.Add(mesh);
				StaticMeshTransforms.Add(curStaticMeshComp->GetComponentTransform());		
				for (const auto curStaticMeshMat : curStaticMeshComp->GetMaterials())
				{
					UMaterialInstanceDynamic* originalDatasmithMat = Cast<UMaterialInstanceDynamic>(curStaticMeshMat);
					if (!originalDatasmithMat)
					{
						// If missing, create opaque dynamic mat by default
						originalDatasmithMat = UMaterialInstanceDynamic::Create(gameMode->Original_DS_PbrOpaque, this);
					}
					// Replace the original Datasmith material with Modumate Datasmith material
					UMaterialInterface* modumateDatasmithParentMat = gameMode->Modumate_DS_PbrOpaque;
					if (originalDatasmithMat->Parent == gameMode->Original_DS_Cutout)
					{
						modumateDatasmithParentMat = gameMode->Modumate_DS_Cutout;
					}
					else if (originalDatasmithMat->Parent == gameMode->Original_DS_Opaque)
					{
						modumateDatasmithParentMat = gameMode->Modumate_DS_Opaque;
					}
					else if (originalDatasmithMat->Parent == gameMode->Original_DS_PbrOpaque_2Sided)
					{
						modumateDatasmithParentMat = gameMode->Modumate_DS_PbrOpaque_2Sided;
					}
					else if (originalDatasmithMat->Parent == gameMode->Original_DS_PbrTranslucent)
					{
						modumateDatasmithParentMat = gameMode->Modumate_DS_PbrTranslucent;
					}
					else if (originalDatasmithMat->Parent == gameMode->Original_DS_PbrTranslucent_2Sided)
					{
						modumateDatasmithParentMat = gameMode->Modumate_DS_PbrTranslucent_2Sided;
					}
					else if (originalDatasmithMat->Parent == gameMode->Original_DS_Transparent)
					{
						modumateDatasmithParentMat = gameMode->Modumate_DS_Transparent;
					}
					UMaterialInstanceDynamic* newDynMat = UMaterialInstanceDynamic::Create(modumateDatasmithParentMat, this);
					newDynMat->CopyMaterialUniformParameters(originalDatasmithMat);
					StaticMeshMaterials.Add(newDynMat);
				}
			}
		}
		curActor->SetActorHiddenInGame(true);
		curActor->SetActorEnableCollision(false);
	}
	GetValuesFromMetaData();

	if (DatasmithImporter.IsValid())
	{
		DatasmithImporter->OnRuntimeActorImportDone(this);
	}
}

bool AEditModelDatasmithRuntimeActor::MakeFromImportFilePath()
{
	return UDatasmithRuntimeLibrary::LoadFile(this, ImportFilePath);
}

void AEditModelDatasmithRuntimeActor::GetAttachedActorsRecursive(AActor* ParentActor, TArray<AActor*>& OutActors)
{
	TArray<AActor*> attachedActors;
	ParentActor->GetAttachedActors(attachedActors);
	for (AActor* attached : attachedActors)
	{
		if (!OutActors.Contains(attached))
		{
			GetAttachedActorsRecursive(attached, OutActors);
			OutActors.Add(attached);
		}
	}
}

bool AEditModelDatasmithRuntimeActor::GetValuesFromMetaData()
{
	// Meta file path
	FString datasmithPath = ImportFilePath;
	int32 urlLastSlashIdx;
	datasmithPath.FindLastChar(TEXT('\\'), urlLastSlashIdx);
	FString metaFilePath = datasmithPath.Left(urlLastSlashIdx + 1) + TEXT("Meta.json");
	FString metaFilePathClean = metaFilePath.Replace(TEXT("\\"), TEXT("/"));

	AssetFolderPath = datasmithPath.Replace(TEXT(".udatasmith"), TEXT("_Assets\\"));
	FString assetFolderPathClean = AssetFolderPath.Replace(TEXT("\\"), TEXT("/"));

	FString fileJsonString;
	bool bLoadFileSuccess = FFileHelper::LoadFileToString(fileJsonString, *metaFilePathClean);
	if (!bLoadFileSuccess)
	{
		return false;
	}

	auto jsonReader = TJsonReaderFactory<>::Create(fileJsonString);

	FDatasmithMetaData metaData;
	if (!ReadJsonGeneric<FDatasmithMetaData>(fileJsonString, &metaData))
	{
		return false;
	}
	CurrentMetaData = metaData;

	for (auto& curMaterial : metaData.Materials)
	{
		if (StaticMeshMaterials.IsValidIndex(curMaterial.Index))
		{
			UMaterialInstanceDynamic* dynMat = Cast<UMaterialInstanceDynamic>(StaticMeshMaterials[curMaterial.Index]);
			if (dynMat)
			{
				// Tint
				dynMat->SetVectorParameterValue(TEXT("TintColor"), FLinearColor::FromSRGBColor(FColor::FromHex(curMaterial.TintColor)));
				dynMat->SetScalarParameterValue(TEXT("TintColorFading"), curMaterial.TintColorFading);
				// Diffuse
				dynMat->SetScalarParameterValue(TEXT("DiffuseMapFading"), curMaterial.DiffuseMapFading);
				dynMat->SetVectorParameterValue(TEXT("DiffuseColor"), FLinearColor::FromSRGBColor(FColor::FromHex(curMaterial.DiffuseColor)));
				// Metallic
				dynMat->SetScalarParameterValue(TEXT("MetallicMapFading"), curMaterial.MetallicMapFading);
				dynMat->SetScalarParameterValue(TEXT("Metallic"), curMaterial.Metallic);
				// Specular
				dynMat->SetScalarParameterValue(TEXT("SpecularMapFading"), curMaterial.SpecularMapFading);
				dynMat->SetScalarParameterValue(TEXT("Specular"), curMaterial.Specular);
				// Roughness
				dynMat->SetScalarParameterValue(TEXT("RoughnessMapFading"), curMaterial.RoughnessMapFading);
				dynMat->SetScalarParameterValue(TEXT("Roughness"), curMaterial.Roughness);
				// Normal and Bump
				dynMat->SetScalarParameterValue(TEXT("NormalMapFading"), curMaterial.NormalMapFading);
				dynMat->SetScalarParameterValue(TEXT("BumpAmount"), curMaterial.BumpAmount);
				// Opacity applies to PbrTranslucent materials
				dynMat->SetScalarParameterValue(TEXT("OpacityMapFading"), curMaterial.OpacityMapFading);
				dynMat->SetScalarParameterValue(TEXT("Opacity"), curMaterial.Opacity);
				// Transparency applies to Cutout and Transparent materials only
				dynMat->SetScalarParameterValue(TEXT("TransparencyMapFading"), curMaterial.TransparencyMapFading);
				dynMat->SetScalarParameterValue(TEXT("Transparency"), curMaterial.Transparency);
				// Import textures
				if (!curMaterial.DiffuseMap.IsEmpty())
				{
					UTexture2D* diffuseMapTexture = FImageUtils::ImportFileAsTexture2D(assetFolderPathClean + curMaterial.DiffuseMap);
					if (diffuseMapTexture)
					{
						dynMat->SetTextureParameterValue(TEXT("DiffuseMap"), diffuseMapTexture);
					}
				}
				if (!curMaterial.MetallicMap.IsEmpty())
				{
					UTexture2D* metallicMapTexture = FImageUtils::ImportFileAsTexture2D(assetFolderPathClean + curMaterial.MetallicMap);
					if (metallicMapTexture)
					{
						dynMat->SetTextureParameterValue(TEXT("MetallicMap"), metallicMapTexture);
					}
				}
				if (!curMaterial.SpecularMap.IsEmpty())
				{
					UTexture2D* specularMapTexture = FImageUtils::ImportFileAsTexture2D(assetFolderPathClean + curMaterial.SpecularMap);
					if (specularMapTexture)
					{
						dynMat->SetTextureParameterValue(TEXT("SpecularMap"), specularMapTexture);
					}
				}
				if (!curMaterial.RoughnessMap.IsEmpty())
				{
					UTexture2D* roughnessMapTexture = FImageUtils::ImportFileAsTexture2D(assetFolderPathClean + curMaterial.RoughnessMap);
					if (roughnessMapTexture)
					{
						dynMat->SetTextureParameterValue(TEXT("RoughnessMap"), roughnessMapTexture);
					}
				}
				if (!curMaterial.NormalMap.IsEmpty())
				{
					UTexture2D* normalMapTexture = FImageUtils::ImportFileAsTexture2D(assetFolderPathClean + curMaterial.NormalMap);
					if (normalMapTexture)
					{
						dynMat->SetTextureParameterValue(TEXT("NormalMap"), normalMapTexture);
					}
				}
				if (!curMaterial.BumpMap.IsEmpty())
				{
					UTexture2D* bumpMapTexture = FImageUtils::ImportFileAsTexture2D(assetFolderPathClean + curMaterial.BumpMap);
					if (bumpMapTexture)
					{
						dynMat->SetTextureParameterValue(TEXT("BumpMap"), bumpMapTexture);
					}
				}
				// PbrTranslucent materials only
				if (!curMaterial.OpacityMap.IsEmpty())
				{
					UTexture2D* opacityMapTexture = FImageUtils::ImportFileAsTexture2D(assetFolderPathClean + curMaterial.OpacityMap);
					if (opacityMapTexture)
					{
						dynMat->SetTextureParameterValue(TEXT("OpacityMap"), opacityMapTexture);
					}
				}
				// Cutout and Transparent materials only
				if (!curMaterial.TransparencyMap.IsEmpty())
				{
					UTexture2D* transparencyMapTexture = FImageUtils::ImportFileAsTexture2D(assetFolderPathClean + curMaterial.TransparencyMap);
					if (transparencyMapTexture)
					{
						dynMat->SetTextureParameterValue(TEXT("TransparencyMap"), transparencyMapTexture);
					}
				}
				// Cutout and Transparent materials only
				if (!curMaterial.CutoutOpacityMap.IsEmpty())
				{
					UTexture2D* cutoutOpacityMapTexture = FImageUtils::ImportFileAsTexture2D(assetFolderPathClean + curMaterial.CutoutOpacityMap);
					if (cutoutOpacityMapTexture)
					{
						dynMat->SetTextureParameterValue(TEXT("CutoutOpacityMap"), cutoutOpacityMapTexture);
					}
				}
			}
		}
	}
	return true;
}

FString AEditModelDatasmithRuntimeActor::DatasmithColorToHex(const FColor& InColor)
{
	return InColor.ToHex();
}

FColor AEditModelDatasmithRuntimeActor::DatasmithHexToColor(const FString& InHexString)
{
	return FColor::FromHex(InHexString);
}

FString AEditModelDatasmithRuntimeActor::MakeJsonStringFromDatasmithMetaData(const FDatasmithMetaData& InData)
{
	FString jsonString;
	WriteJsonGeneric(jsonString, &InData);
	return jsonString;
}
