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
		FGuid newArchitecturalMeshKey;
		FModumateDatabase* db = GetWorld()->GetGameInstance<UModumateGameInstance>()->ObjectDatabase;
		db->AddArchitecturalMeshFromDatasmith(filename, newArchitecturalMeshKey);

		// Create new preset
		int32 urlLastSlashIdx;
		filename.FindLastChar(TEXT('\\'), urlLastSlashIdx);
		FString newPresetName = filename.RightChop(urlLastSlashIdx + 1);

		FGuid newPresetID;
		controller->GetDocument()->GetPresetCollection().MakeNewPresetFromDatasmith(*db, newPresetName, newArchitecturalMeshKey, newPresetID);
		controller->GetDocument()->UpdateWebPresets();
	}

	controller->EMPlayerState->ShowingFileDialog = false;
	return bLoadSuccess;
}

bool AEditModelDatasmithImporter::ImportDatasmithFromURL(const FString& URL)
{
	auto Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(URL);
	Request->ProcessRequest();

	TWeakObjectPtr<AEditModelDatasmithImporter> weakThisCaptured(this);
	Request->OnProcessRequestComplete().BindLambda([weakThisCaptured](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{ 
		auto sharedThis = weakThisCaptured.Get();
		if (sharedThis != nullptr)
		{
			sharedThis->RequestCompleteCallback(Request, Response, bWasSuccessful);
		}
 });
	
	return true;
}

void AEditModelDatasmithImporter::RequestCompleteCallback(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Error, TEXT("bWasSuccessful is %s"), (bWasSuccessful ? TEXT("true") : TEXT("false")));
	if (Request->GetStatus() == EHttpRequestStatus::Succeeded
		&& Response->GetContent().Num() != 0)
	{
		const static FString cacheFileName = TEXT("cacheImport.udatasmith");
		FString cacheFile = FPaths::Combine(FModumateUserSettings::GetLocalTempDir(), cacheFileName);
		FFileHelper::SaveStringToFile(Response->GetContentAsString(), *cacheFile);

		bool bLoadResult = UDatasmithRuntimeLibrary::LoadFile(CurrentDatasmithRuntimeActor.Get(), cacheFile);
		UE_LOG(LogTemp, Error, TEXT("bLoadResult is %s"), (bLoadResult ? TEXT("true") : TEXT("false")));
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
		for (const auto curPart : InAssetRequest.Assembly.Parts)
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
				PresetLoadStatusMap.Add(InAssetRequest.Assembly.PresetGUID, EAssetImportLoadStatus::Loading);

				FActorSpawnParameters datasmithRuntimeActorSpawnParams;
				datasmithRuntimeActorSpawnParams.Owner = this;
				CurrentDatasmithRuntimeActor = GetWorld()->SpawnActor<AEditModelDatasmithRuntimeActor>(DatasmithRuntimeActorClass, datasmithRuntimeActorSpawnParams);
				CurrentDatasmithRuntimeActor->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
				CurrentDatasmithRuntimeActor->DatasmithImporter = this;
				CurrentDatasmithRuntimeActor->PresetKey = InAssetRequest.Assembly.PresetGUID;
				CurrentDatasmithRuntimeActor->ImportFilePath = testFile;
				PresetToDatasmithRuntimeActorMap.Add(InAssetRequest.Assembly.PresetGUID, CurrentDatasmithRuntimeActor.Get());

				// Timer to keep track of import progress
				// In rare cases, import thread doesn't start at 1st, but will start at 2nd attempt
				GetWorldTimerManager().SetTimer(AssetImportCheckTimer, this, &AEditModelDatasmithImporter::OnAssetImportCheckTimer, CheckAfterImportTime, false);

				UE_LOG(LogTemp, Log, TEXT("Begin Import: %s"), *testFile);
				bool bLoadResult = CurrentDatasmithRuntimeActor->MakeFromImportFilePath();
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
