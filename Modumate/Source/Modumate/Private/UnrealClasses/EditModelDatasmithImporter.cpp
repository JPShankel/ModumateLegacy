// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelDatasmithImporter.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelDatasmithRuntimeActor.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "DatasmithRuntimeBlueprintLibrary.h"
#include "TimerManager.h"
#include "ModumateCore/PlatformFunctions.h"

#include "Drafting/MiniZip.h"
#include "HAL/FileManagerGeneric.h"

// Sets default values
AEditModelDatasmithImporter::AEditModelDatasmithImporter()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AEditModelDatasmithImporter::BeginPlay()
{
	Super::BeginPlay();
}

bool AEditModelDatasmithImporter::ImportDatasmithFromDialogue()
{
	auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	controller->EMPlayerState->ShowingFileDialog = true;

	FString filename;
	bool bLoadSuccess = false;

	// Open import dialog
	if (FModumatePlatform::GetOpenFilename(filename, nullptr, false))
	{
		// Create new archMesh from Datasmith
		FGuid newArchitecturalMeshKey = FGuid::NewGuid();
		controller->GetDocument()->GetPresetCollection().AddArchitecturalMeshFromDatasmith(filename, newArchitecturalMeshKey);

		// Create new preset
		int32 urlLastSlashIdx;
		filename.FindLastChar(TEXT('\\'), urlLastSlashIdx);
		FString newPresetName = filename.RightChop(urlLastSlashIdx + 1);

		FGuid newPresetID;
		controller->GetDocument()->GetPresetCollection().MakeNewPresetFromDatasmith(newPresetName, newArchitecturalMeshKey, newPresetID);

		// trigger web update to add new created preset
		TArray<FGuid> addedPresets;
		addedPresets.Add(newPresetID);
		controller->GetDocument()->TriggerWebPresetChange(EWebPresetChangeVerb::Add, addedPresets);
	}

	controller->EMPlayerState->ShowingFileDialog = false;
	return bLoadSuccess;
}

bool AEditModelDatasmithImporter::ImportDatasmithFromWeb(const FString& URL)
{
	auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (!controller || URL.IsEmpty())
	{
		return false;
	}
	// Create new archMesh from Datasmith
	FGuid newArchitecturalMeshKey = FGuid::NewGuid();
	controller->GetDocument()->GetPresetCollection().AddArchitecturalMeshFromDatasmith(URL, newArchitecturalMeshKey);

	// Create new preset
	FString newPresetName = TEXT("Downloaded Asset");

	FGuid newPresetID;
	controller->GetDocument()->GetPresetCollection().MakeNewPresetFromDatasmith(newPresetName, newArchitecturalMeshKey, newPresetID);
	controller->GetDocument()->UpdateWebPresets();
	return true;
}

bool AEditModelDatasmithImporter::RequestDownloadFromURL(const FGuid& InGUID, const FString& URL)
{
	PresetLoadStatusMap.Add(InGUID, EAssetImportLoadStatus::Downloading);

	auto Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(URL);
	Request->ProcessRequest();

	TWeakObjectPtr<AEditModelDatasmithImporter> weakThisCaptured(this);
	Request->OnProcessRequestComplete().BindLambda([weakThisCaptured, InGUID](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{ 
		auto sharedThis = weakThisCaptured.Get();
		if (sharedThis != nullptr)
		{
			sharedThis->DownloadCompleteCallback(InGUID, Request, Response, bWasSuccessful);
		}
 });
	
	return true;
}

void AEditModelDatasmithImporter::DownloadCompleteCallback(const FGuid& InGUID, FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (Request->GetStatus() == EHttpRequestStatus::Succeeded
		&& Response->GetContent().Num() != 0)
	{
		FString cacheFilePath;
		GetZipFilePathFromPresetGUID(InGUID, cacheFilePath);
		FFileHelper::SaveArrayToFile(Response->GetContent(), *cacheFilePath);

		FMiniZip miniZip;
		miniZip.ExtractFromArchive(cacheFilePath);

		ImportDatasmithFilesFromPresetFolder(InGUID);
	}
}

void AEditModelDatasmithImporter::HandleAssetRequest(const FAssetRequest& InAssetRequest)
{
	if (!ensure(InAssetRequest.Assembly.Parts.Num() != 0))
	{
		return;
	}

	// Check if this is an embedded assets first
	if (InAssetRequest.Assembly.Parts[0].Mesh.DatasmithUrl.IsEmpty())
	{
		// This is an embedded asset, save to assets list and use callback if available
		// TODO: Adding embedded assets to StaticMeshAssetMap is for testing purposes, 
		// should re-evaluate when Datasmith assets can be bundled with presets properly.
		TArray<UStaticMesh*> meshes;
		for (const auto& curPart : InAssetRequest.Assembly.Parts)
		{
			if (curPart.Mesh.EngineMesh.IsValid())
			{
				meshes.Add(curPart.Mesh.EngineMesh.Get());
			}
		}
		StaticMeshAssetMap.Add(InAssetRequest.Assembly.PresetGUID, meshes);
		if (InAssetRequest.CallbackTask)
		{
			InAssetRequest.CallbackTask();
		}
		return;
	}
	
	// If not an embedded asset, is a Datasmith asset
	const auto assets = StaticMeshAssetMap.FindRef(InAssetRequest.Assembly.PresetGUID);

	if (assets.Num() > 0)
	{
		// If there are assets, call the task if available
		if (InAssetRequest.CallbackTask)
		{
			InAssetRequest.CallbackTask();
		}
	}
	else
	{
		const auto loadStatus = PresetLoadStatusMap.FindRef(InAssetRequest.Assembly.PresetGUID);
		if (loadStatus == EAssetImportLoadStatus::None)
		{
			FString testFile = InAssetRequest.Assembly.Parts[0].Mesh.DatasmithUrl;
			if (FPaths::FileExists(testFile))
			{
				AddDatasmithRuntimeActor(InAssetRequest.Assembly.PresetGUID, testFile);
			}
			else if (testFile.StartsWith(TEXT("http")))
			{
				// Check if there are datasmith files
				if (!ImportDatasmithFilesFromPresetFolder(InAssetRequest.Assembly.PresetGUID))
				{
					RequestDownloadFromURL(InAssetRequest.Assembly.PresetGUID, testFile);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Import from path failed: %s"), *testFile);
			}
		}
	}
}

void AEditModelDatasmithImporter::OnRuntimeActorImportDone(class AEditModelDatasmithRuntimeActor* FromActor)
{
	// Cache meshes param from DatasmithActor
	PresetLoadStatusMap.Add(FromActor->PresetKey, EAssetImportLoadStatus::Loaded);
	StaticMeshAssetMap.Add(FromActor->PresetKey, FromActor->StaticMeshRefs);
	StaticMeshTransformMap.Add(FromActor->PresetKey, FromActor->StaticMeshTransforms);
	ImportedMaterialMap.Add(FromActor->PresetKey, FromActor->StaticMeshMaterials);

	// Clean any objects that are using this model
	auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller)
	{
		TArray<int32> affectedMoiIDs;
		controller->GetDocument()->GetObjectIdsByAssembly(FromActor->PresetKey, affectedMoiIDs);
		for (auto curMoiID : affectedMoiIDs)
		{
			auto curMoi = controller->GetDocument()->GetObjectById(curMoiID);
			if (curMoi)
			{
				curMoi->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}
		controller->GetDocument()->CleanObjects(nullptr);
	}
}

void AEditModelDatasmithImporter::OnAssetImportCheckTimer()
{
	// If a DatasmithActor is loading but not building, try import it again
	for (const auto kvp : PresetLoadStatusMap)
	{
		if (kvp.Value == EAssetImportLoadStatus::Loading)
		{
			auto curDatasmithActor = PresetToDatasmithRuntimeActorMap.FindRef(kvp.Key);
			if (curDatasmithActor && !curDatasmithActor->bBuilding)
			{
				UE_LOG(LogTemp, Log, TEXT("Reattempt to Import: %s"), *curDatasmithActor->ImportFilePath);
				curDatasmithActor->MakeFromImportFilePath();
				GetWorldTimerManager().SetTimer(AssetImportCheckTimer, this, &AEditModelDatasmithImporter::OnAssetImportCheckTimer, CheckAfterImportTime, false);
			}
		}
	}
}

bool AEditModelDatasmithImporter::AddDatasmithRuntimeActor(const FGuid& InGUID, const FString& DatasmithImportPath)
{
	PresetLoadStatusMap.Add(InGUID, EAssetImportLoadStatus::Loading);

	FActorSpawnParameters datasmithRuntimeActorSpawnParams;
	datasmithRuntimeActorSpawnParams.Owner = this;
	AEditModelDatasmithRuntimeActor* newDatasmithRuntimeActor = GetWorld()->SpawnActor<AEditModelDatasmithRuntimeActor>(DatasmithRuntimeActorClass, datasmithRuntimeActorSpawnParams);
	newDatasmithRuntimeActor->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
	newDatasmithRuntimeActor->DatasmithImporter = this;
	newDatasmithRuntimeActor->PresetKey = InGUID;
	newDatasmithRuntimeActor->ImportFilePath = DatasmithImportPath;
	PresetToDatasmithRuntimeActorMap.Add(InGUID, newDatasmithRuntimeActor);

	// Timer to keep track of import progress
	// In rare cases, import thread doesn't start at 1st, but will start at 2nd attempt
	GetWorldTimerManager().SetTimer(AssetImportCheckTimer, this, &AEditModelDatasmithImporter::OnAssetImportCheckTimer, CheckAfterImportTime, false);

	UE_LOG(LogTemp, Log, TEXT("Begin Import: %s"), *DatasmithImportPath);
	return newDatasmithRuntimeActor->MakeFromImportFilePath();
}

bool AEditModelDatasmithImporter::GetZipFilePathFromPresetGUID(const FGuid& InGUID, FString& OutFilePath)
{
	const static FString downloadPathName = TEXT("DownloadedAssets");
	const static FString cacheFileName = TEXT("cacheImport.zip");
	OutFilePath = FPaths::Combine(FModumateUserSettings::GetLocalTempDir(), downloadPathName, InGUID.ToString(), cacheFileName);
	return FPaths::FileExists(OutFilePath);
}

bool AEditModelDatasmithImporter::GetFolderPathFromPresetGUID(const FGuid& InGUID, FString& OutFolderPath)
{
	const static FString downloadPathName = TEXT("DownloadedAssets");
	OutFolderPath = FPaths::Combine(FModumateUserSettings::GetLocalTempDir(), downloadPathName, InGUID.ToString());
	return FPaths::DirectoryExists(OutFolderPath);
}

bool AEditModelDatasmithImporter::ImportDatasmithFilesFromPresetFolder(const FGuid& InGUID)
{
	bool result = false;
	FString extractedFolder;
	GetFolderPathFromPresetGUID(InGUID, extractedFolder);
	TArray<FString> dataSmithFiles;
	FFileManagerGeneric::Get().FindFiles(dataSmithFiles, *extractedFolder, TEXT(".udatasmith"));
	for (auto curDatasmithFile : dataSmithFiles)
	{
		FString fullDatasmithPathName = FPaths::Combine(extractedFolder, curDatasmithFile);
		if (ensure(AddDatasmithRuntimeActor(InGUID, fullDatasmithPathName)))
		{
			result = true;
		}
	}
	return result;
}
