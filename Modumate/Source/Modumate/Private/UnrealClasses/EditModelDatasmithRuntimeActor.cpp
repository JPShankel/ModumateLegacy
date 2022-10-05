// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelDatasmithRuntimeActor.h"
#include "DatasmithRuntimeBlueprintLibrary.h"
#include "Components/StaticMeshComponent.h"
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

	TArray<AActor*> attachedActors;
	GetAttachedActorsRecursive(this, attachedActors);

	// Search for all StaticMeshComponents created and attached from import
	TArray<UStaticMeshComponent*> staticMeshComps;
	for (auto curActor : attachedActors)
	{
		// The attached actors can be StaticMeshActor or UActor
		// Search for StaticMeshComponents using different methods
		AStaticMeshActor* curStaticMeshActor = Cast<AStaticMeshActor>(curActor);
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
		curActor->SetActorHiddenInGame(true);
		curActor->SetActorEnableCollision(false);
	}
	// Sort and cache the StaticMeshes and their transforms
	staticMeshComps.Sort([](const UStaticMeshComponent& curMesh, const UStaticMeshComponent& nextMesh)
		{
			// Mesh order can be inferred from its name, "RuntimeMesh_15" is made before "RuntimeMesh_16" by importer
			FString runtimeMeshPrefix = TEXT("RuntimeMesh_");
			FString curMeshName = curMesh.GetStaticMesh()->GetName();
			FString nextMeshName = nextMesh.GetStaticMesh()->GetName();
			curMeshName.RemoveFromStart(runtimeMeshPrefix);
			nextMeshName.RemoveFromStart(runtimeMeshPrefix);
			int32 curId = FCString::Atoi(*curMeshName);
			int32 nextId = FCString::Atoi(*nextMeshName);
			return curId > nextId;
		});
	for (const auto curStaticMeshComp : staticMeshComps)
	{
		UStaticMesh* mesh = curStaticMeshComp->GetStaticMesh();
		if (mesh)
		{
			StaticMeshRefs.Add(mesh);
			StaticMeshTransforms.Add(curStaticMeshComp->GetComponentTransform());
			// Save original Datasmith Material, but replace with PbrOpaque if null
			TArray<UMaterialInterface*> originalMeshMaterials;
			for (const auto& originalMat : curStaticMeshComp->GetMaterials())
			{
				if (originalMat)
				{
					originalMeshMaterials.Add(originalMat);
				}
				else
				{
					originalMeshMaterials.Add(UMaterialInstanceDynamic::Create(DatasmithImporter->Original_DS_PbrOpaque, this));
				}
			}
			StaticMeshMaterialsMap.Add(mesh, originalMeshMaterials);
			StaticMeshMaterials.Append(originalMeshMaterials);
		}
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
	datasmithPath.FindLastChar(TEXT('/'), urlLastSlashIdx);
	FString metaFilePath = datasmithPath.Left(urlLastSlashIdx + 1) + TEXT("Meta.json");

	AssetFolderPath = datasmithPath.Replace(TEXT(".udatasmith"), TEXT("_Assets/"));

	FString fileJsonString;
	bool bLoadFileSuccess = FFileHelper::LoadFileToString(fileJsonString, *metaFilePath);
	if (bLoadFileSuccess)
	{
		ReadJsonGeneric<FDatasmithMetaData>(fileJsonString, &CurrentMetaData);
	}

	// Since engine runtime import does not track material name, MetaData needs to track via material ID
	int32 matIdx = 0;
	TMap<UMaterialInterface*, UMaterialInstanceDynamic*> originalToDynamicMap;
	for (const auto& meshMatPair : StaticMeshMaterialsMap)
	{
		for (const auto& staticMeshMaterial : meshMatPair.Value)
		{
			// Create dynamic material for each original material
			const auto& existingDynMat = originalToDynamicMap.FindRef(staticMeshMaterial);
			if (!existingDynMat)
			{
				// Check MetaData for current material
				bool bUseMetaData = CurrentMetaData.Materials.IsValidIndex(matIdx);
				FDatasmithMetaDataMaterial curMaterialMetaData;
				if (bUseMetaData)
				{
					curMaterialMetaData = CurrentMetaData.Materials[matIdx];
				}

				// Check for material type, and if it is overridden by MetaData
				EDatasmithMaterialType matType = DatasmithImporter->GetTypeFromOriginalDatasmithMaterial(staticMeshMaterial);
				if (curMaterialMetaData.MaterialType != EDatasmithMaterialType::None)
				{
					matType = curMaterialMetaData.MaterialType;
				}
				// Make Modumate version of Datasmith material and copy original values
				UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(DatasmithImporter->GetModumateDatasmithMaterialByType(matType), this);
				dynMat->CopyMaterialUniformParameters(staticMeshMaterial);
				originalToDynamicMap.Add(staticMeshMaterial, dynMat);
				RuntimeDatasmithMaterials.Add(dynMat);
				if (dynMat && bUseMetaData)
				{
					// Tint
					dynMat->SetVectorParameterValue(TEXT("TintColor"), FLinearColor::FromSRGBColor(FColor::FromHex(curMaterialMetaData.TintColor)));
					dynMat->SetScalarParameterValue(TEXT("TintColorFading"), curMaterialMetaData.TintColorFading);
					// Diffuse
					dynMat->SetScalarParameterValue(TEXT("DiffuseMapFading"), curMaterialMetaData.DiffuseMapFading);
					dynMat->SetVectorParameterValue(TEXT("DiffuseColor"), FLinearColor::FromSRGBColor(FColor::FromHex(curMaterialMetaData.DiffuseColor)));
					// Metallic
					dynMat->SetScalarParameterValue(TEXT("MetallicMapFading"), curMaterialMetaData.MetallicMapFading);
					dynMat->SetScalarParameterValue(TEXT("Metallic"), curMaterialMetaData.Metallic);
					// Specular
					dynMat->SetScalarParameterValue(TEXT("SpecularMapFading"), curMaterialMetaData.SpecularMapFading);
					dynMat->SetScalarParameterValue(TEXT("Specular"), curMaterialMetaData.Specular);
					// Roughness
					dynMat->SetScalarParameterValue(TEXT("RoughnessMapFading"), curMaterialMetaData.RoughnessMapFading);
					dynMat->SetScalarParameterValue(TEXT("Roughness"), curMaterialMetaData.Roughness);
					// Normal and Bump
					dynMat->SetScalarParameterValue(TEXT("NormalMapFading"), curMaterialMetaData.NormalMapFading);
					dynMat->SetScalarParameterValue(TEXT("BumpAmount"), curMaterialMetaData.BumpAmount);
					// Opacity applies to PbrTranslucent materials
					dynMat->SetScalarParameterValue(TEXT("OpacityMapFading"), curMaterialMetaData.OpacityMapFading);
					dynMat->SetScalarParameterValue(TEXT("Opacity"), curMaterialMetaData.Opacity);
					// Transparency applies to Cutout and Transparent materials only
					dynMat->SetScalarParameterValue(TEXT("TransparencyMapFading"), curMaterialMetaData.TransparencyMapFading);
					dynMat->SetScalarParameterValue(TEXT("Transparency"), curMaterialMetaData.Transparency);
					// Import textures
					if (!curMaterialMetaData.DiffuseMap.IsEmpty())
					{
						UTexture2D* diffuseMapTexture = FImageUtils::ImportFileAsTexture2D(AssetFolderPath + curMaterialMetaData.DiffuseMap);
						if (diffuseMapTexture)
						{
							dynMat->SetTextureParameterValue(TEXT("DiffuseMap"), diffuseMapTexture);
						}
					}
					if (!curMaterialMetaData.MetallicMap.IsEmpty())
					{
						UTexture2D* metallicMapTexture = FImageUtils::ImportFileAsTexture2D(AssetFolderPath + curMaterialMetaData.MetallicMap);
						if (metallicMapTexture)
						{
							dynMat->SetTextureParameterValue(TEXT("MetallicMap"), metallicMapTexture);
						}
					}
					if (!curMaterialMetaData.SpecularMap.IsEmpty())
					{
						UTexture2D* specularMapTexture = FImageUtils::ImportFileAsTexture2D(AssetFolderPath + curMaterialMetaData.SpecularMap);
						if (specularMapTexture)
						{
							dynMat->SetTextureParameterValue(TEXT("SpecularMap"), specularMapTexture);
						}
					}
					if (!curMaterialMetaData.RoughnessMap.IsEmpty())
					{
						UTexture2D* roughnessMapTexture = FImageUtils::ImportFileAsTexture2D(AssetFolderPath + curMaterialMetaData.RoughnessMap);
						if (roughnessMapTexture)
						{
							dynMat->SetTextureParameterValue(TEXT("RoughnessMap"), roughnessMapTexture);
						}
					}
					if (!curMaterialMetaData.NormalMap.IsEmpty())
					{
						UTexture2D* normalMapTexture = FImageUtils::ImportFileAsTexture2D(AssetFolderPath + curMaterialMetaData.NormalMap);
						if (normalMapTexture)
						{
							dynMat->SetTextureParameterValue(TEXT("NormalMap"), normalMapTexture);
						}
					}
					if (!curMaterialMetaData.BumpMap.IsEmpty())
					{
						UTexture2D* bumpMapTexture = FImageUtils::ImportFileAsTexture2D(AssetFolderPath + curMaterialMetaData.BumpMap);
						if (bumpMapTexture)
						{
							dynMat->SetTextureParameterValue(TEXT("BumpMap"), bumpMapTexture);
						}
					}
					// PbrTranslucent materials only
					if (!curMaterialMetaData.OpacityMap.IsEmpty())
					{
						UTexture2D* opacityMapTexture = FImageUtils::ImportFileAsTexture2D(AssetFolderPath + curMaterialMetaData.OpacityMap);
						if (opacityMapTexture)
						{
							dynMat->SetTextureParameterValue(TEXT("OpacityMap"), opacityMapTexture);
						}
					}
					// Cutout and Transparent materials only
					if (!curMaterialMetaData.TransparencyMap.IsEmpty())
					{
						UTexture2D* transparencyMapTexture = FImageUtils::ImportFileAsTexture2D(AssetFolderPath + curMaterialMetaData.TransparencyMap);
						if (transparencyMapTexture)
						{
							dynMat->SetTextureParameterValue(TEXT("TransparencyMap"), transparencyMapTexture);
						}
					}
					// Cutout and Transparent materials only
					if (!curMaterialMetaData.CutoutOpacityMap.IsEmpty())
					{
						UTexture2D* cutoutOpacityMapTexture = FImageUtils::ImportFileAsTexture2D(AssetFolderPath + curMaterialMetaData.CutoutOpacityMap);
						if (cutoutOpacityMapTexture)
						{
							dynMat->SetTextureParameterValue(TEXT("CutoutOpacityMap"), cutoutOpacityMapTexture);
						}
					}
				}
				matIdx++;
			}
		}
	}
	// Save dynamic material made from Modumate version of Datasmith
	for (const auto& meshMatPair : StaticMeshMaterialsMap)
	{
		TArray<UMaterialInstanceDynamic*> dynMats;
		for (const auto& staticMeshMaterial : meshMatPair.Value)
		{
			dynMats.Add(originalToDynamicMap.FindRef(staticMeshMaterial));
		}
		RuntimeDatasmithMaterialsMap.Add(meshMatPair.Key, dynMats);
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
