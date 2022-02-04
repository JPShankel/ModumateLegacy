// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelDatasmithImporter.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "DatasmithRuntimeBlueprintLibrary.h"

// Sets default values
AEditModelDatasmithImporter::AEditModelDatasmithImporter()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AEditModelDatasmithImporter::BeginPlay()
{
	Super::BeginPlay();

	FActorSpawnParameters datasmithRuntimeActorSpawnParams;
	datasmithRuntimeActorSpawnParams.Name = FName(*FString::Printf(TEXT("%s_DatasmithRuntimeActor"), *GetName()));
	DatasmithRuntimeActor = GetWorld()->SpawnActor<ADatasmithRuntimeActor>(DatasmithRuntimeActorClass, datasmithRuntimeActorSpawnParams);
	DatasmithRuntimeActor->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
}

bool AEditModelDatasmithImporter::ImportDatasmithFromDialogue()
{
	// For now, spawn a new datasmith actor
	DatasmithRuntimeActor = GetWorld()->SpawnActor<ADatasmithRuntimeActor>(DatasmithRuntimeActorClass, FTransform::Identity);
	if (DatasmithRuntimeActor == nullptr)
	{
		return false;
	}
	const FString defaultPath;
	bool bLoadResult = UDatasmithRuntimeLibrary::LoadFileFromExplorer(DatasmithRuntimeActor.Get(), defaultPath);

	return bLoadResult;

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

		bool bLoadResult = UDatasmithRuntimeLibrary::LoadFile(DatasmithRuntimeActor.Get(), cacheFile);
		UE_LOG(LogTemp, Error, TEXT("bLoadResult is %s"), (bLoadResult ? TEXT("true") : TEXT("false")));
	}
}
