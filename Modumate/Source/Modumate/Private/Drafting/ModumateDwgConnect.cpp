// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateDwgConnect.h"

#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Base64.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ModalDialog/ModalDialogWidget.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ModumateCloudConnection.h"

#include "Drafting/MiniZip.h"

#define LOCTEXT_NAMESPACE "ModumateDwg"

// For local testing:
const FString FModumateDwgConnect::ServerAddress(TEXT("http://localhost:8080"));

FModumateDwgConnect::FModumateDwgConnect(const FModumateDwgDraw& dwgDraw)
	: DwgDraw(dwgDraw)
{ }

FModumateDwgConnect::~FModumateDwgConnect()
{
}

class FModumateDwgConnect::DwgSaver
{
public:
	FString ZipDirectory;
	TAtomic<int> NumPages;
	TWeakObjectPtr<UModumateGameInstance> WeakGameInstance;
	bool Successful { true };
	void OnHttpReply(FHttpRequestPtr Request, FHttpResponsePtr Response,
		bool bWasSuccessful, FString saveFilename);

};

void FModumateDwgConnect::DwgSaver::OnHttpReply(FHttpRequestPtr Request, FHttpResponsePtr Response,
	bool bWasSuccessful, FString saveFilename)
{
	if (bWasSuccessful)
	{
		int32 code = Response->GetResponseCode();
		if (code == EHttpResponseCodes::Ok)
		{
			const TArray<uint8>& dwgContents = Response->GetContent();
			if (!FFileHelper::SaveArrayToFile(dwgContents, *saveFilename))
			{
				UE_LOG(LogAutoDrafting, Error, TEXT("Couldn't save DWG file to %s"), *saveFilename);
				Successful = false;
				return;
			}
		}
		else
		{
			Successful = false;
			UE_LOG(LogAutoDrafting, Error, TEXT("DWG Server returned status %d for file %s"),
				code, *saveFilename);
		}

	}
	else
	{
		Successful = false;
		UE_LOG(LogAutoDrafting, Error, TEXT("DWG Server reply unsuccessful for file %s"),
			*saveFilename);
	}
	if (Successful && --NumPages == 0)
	{
		FMiniZip miniZip;
		struct ListFilesVisitor : IPlatformFile::FDirectoryVisitor
		{
			FMiniZip * zipper;

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if (!bIsDirectory)
				{
					zipper->AddFile(FilenameOrDirectory);
				}
				return true;
			}
		} listFilesVisitor;
		listFilesVisitor.zipper = &miniZip;

		IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
		platformFile.IterateDirectory(*ZipDirectory, listFilesVisitor);

		UModumateGameInstance* gameInstance = nullptr;
		if (WeakGameInstance.IsValid())
		{
			gameInstance = WeakGameInstance.Get();
		}

		if (miniZip.CreateArchive(ZipDirectory + TEXT(".zip")))
		{
			UE_LOG(LogAutoDrafting, Display, TEXT("Saved DWG bundle to %s.zip"), *ZipDirectory);
			if (gameInstance)
			{

				AEditModelPlayerController* controller = CastChecked<AEditModelPlayerController>(gameInstance->GetFirstLocalPlayerController());
				if (controller && controller->EditModelUserWidget && controller->EditModelUserWidget->ModalDialogWidgetBP)
				{
					FModalButtonParam dismissButton(EModalButtonStyle::Default, LOCTEXT("SavedDWGok", "OK"), nullptr);
					controller->EditModelUserWidget->ModalDialogWidgetBP->CreateModalDialog(LOCTEXT("SavedDWGTitle", "Notice"),
						FText::Format(LOCTEXT("SavedDWGMsg", "Saved DWG bundle to {0}.zip"), FText::FromString(ZipDirectory)),
						TArray<FModalButtonParam>({ dismissButton })
						);
				}
			}
		}
		else
		{
			UE_LOG(LogAutoDrafting, Error, TEXT("Failed to create DWG bundle from %s"), *ZipDirectory);
		}

	}

	return;
}

bool FModumateDwgConnect::ConvertJsonToDwg(FString filename)
{
	Filename = filename;

	UWorld* world = DwgDraw.GetWorld();
	GameInstance = world->GetGameInstance<UModumateGameInstance>();
	if (GameInstance == nullptr)
	{
		return false;
	}

	AccountManager = GameInstance->GetAccountManager();
	AEditModelPlayerController* controller =  CastChecked<AEditModelPlayerController>(GameInstance->GetFirstLocalPlayerController(world));
	if (controller && controller->EMPlayerState && controller->EMPlayerState->IsNetMode(NM_Client))
	{
		ProjectID = controller->EMPlayerState->CurProjectID;
	}
	else
	{
		ProjectID.Empty();
	}

	return UploadJsonAndSave();
}

bool FModumateDwgConnect::UploadJsonAndSave()
{
	FString basename = FPaths::GetBaseFilename(Filename, true);
	FString zipDirectory = FPaths::GetBaseFilename(Filename, false);

	IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!platformFile.CreateDirectory(*zipDirectory))
	{
		UE_LOG(LogAutoDrafting, Error, TEXT("Couldn't create archive directory %s"), *zipDirectory);
		return false;
	}

	int numPages = DwgDraw.GetNumPages();
	ResponseSaver = MakeShared<DwgSaver>();
	ResponseSaver->NumPages = numPages;
	ResponseSaver->ZipDirectory = zipDirectory;
	ResponseSaver->WeakGameInstance = GameInstance;
		
	for (int page = 0; page < numPages; ++page)
	{
		FString filenameDwg = DwgDraw.GetPageName(page);
		if (filenameDwg.IsEmpty())
		{
			filenameDwg = FString::Printf(TEXT("%s_%d"), *basename, page + 1);
		}
		filenameDwg = zipDirectory / filenameDwg + TEXT(".dwg");
		FString pageJson = DwgDraw.GetJsonAsString(page);
		CallDwgServer(pageJson, filenameDwg);
	}

	for (auto& image: DwgDraw.GetImages())
	{
		FString imageDestination = zipDirectory / FPaths::GetCleanFilename(image);
		if (!platformFile.CopyFile(*imageDestination, *image))
		{
			UE_LOG(LogAutoDrafting, Error, TEXT("Couldn't copy DWG image file to %s"), *imageDestination);
		}
	}
		
	return true;
}

bool FModumateDwgConnect::CallDwgServer(const FString& jsonString, const FString& saveFilename)
{
	auto request = FHttpModule::Get().CreateRequest();
	TSharedPtr<DwgSaver> responseSaver = ResponseSaver;
	request->OnProcessRequestComplete().BindLambda([responseSaver, saveFilename](FHttpRequestPtr Request,
		FHttpResponsePtr Response, bool bWasSuccessful)
		{
			responseSaver->OnHttpReply(Request, Response, bWasSuccessful, saveFilename);
		});

	request->SetVerb(TEXT("POST"));
	request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

#if 0
	// Send directly to DWG Server.
	request->SetHeader(TEXT("Authorization"), TEXT("Basic YW1zOmtpZ2VibXk2dWtrNzN3"));  // For direct connection to DWG server.
	request->SetContentAsString(jsonString);
	request->SetURL(ServerAddress + TEXT("/jsontodwg"));
#else
	// Send via AMS public web-server.
	request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AccountManager->CloudConnection->GetAuthToken());

	if (!ProjectID.IsEmpty())
	{
		request->SetURL(AccountManager->CloudConnection->GetCloudRootURL() + TEXT("/api/v2/projects/") + ProjectID + TEXT("/dwg/"));
	}
	else
	{
		request->SetURL(AccountManager->CloudConnection->GetCloudRootURL() + TEXT("/api/v2/service/jsontodwg"));
	}
	request->SetContentAsString(jsonString);
#endif
	request->ProcessRequest();

	return true;
}

#undef LOCTEXT_NAMESPACE
