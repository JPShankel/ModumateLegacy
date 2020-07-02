// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateDwgConnect.h"

#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Base64.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Online/ModumateAccountManager.h"

#include "Drafting/MiniZip.h"

namespace Modumate
{
	//const FString FModumateDwgConnect::ServerAddress { TEXT("http://192.168.124.98:8080") };
	const FString FModumateDwgConnect::ServerAddress { TEXT("https://account.modumate.com") };

	FModumateDwgConnect::FModumateDwgConnect(const FModumateDwgDraw& dwgDraw)
		: DwgDraw(dwgDraw)
	{ }

	Modumate::FModumateDwgConnect::~FModumateDwgConnect()
	{
	}

	class FModumateDwgConnect::DwgSaver
	{
	public:
		FString ZipDirectory;
		TAtomic<int> NumPages;
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

			if (miniZip.CreateArchive(ZipDirectory + TEXT(".zip")))
			{
				UE_LOG(LogAutoDrafting, Display, TEXT("Saved DWG bundle to %s.zip"), *ZipDirectory);
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

		UModumateGameInstance* gameInstance = nullptr;
		const auto& worlds = GEngine->GetWorldContexts();
		for (const auto& worldContext : worlds)
		{
			gameInstance = worldContext.World()->GetGameInstance<UModumateGameInstance>();
			if (gameInstance)
			{
				break;
			}
		}
		if (gameInstance == nullptr)
		{
			return false;
		}
		AccountManager = gameInstance->GetAccountManager();

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
		
		for (int page = 0; page < numPages; ++page)
		{
			FString filenameDwg = FString::Printf(TEXT("%s/%s%d.dwg"), *zipDirectory, *basename, page + 1);
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
		TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();
		TSharedPtr<DwgSaver> responseSaver = ResponseSaver;
		request->OnProcessRequestComplete().BindLambda([responseSaver, saveFilename](FHttpRequestPtr Request,
			FHttpResponsePtr Response, bool bWasSuccessful)
			{
				responseSaver->OnHttpReply(Request, Response, bWasSuccessful, saveFilename);
			});

		request->SetVerb(TEXT("POST"));
		request->SetURL(ServerAddress + TEXT("/api/v1/service/jsontodwg"));
		request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
		request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

#if 0
		// Send directly to DWG Server.
		request->SetHeader(TEXT("Authorization"), TEXT("Basic YW1zOmtpZ2VibXk2dWtrNzN3"));  // For direct connection to DWG server.
		request->SetContentAsString(jsonString);
		request->SetURL(ServerAddress + TEXT("/jsontodwg"));
#else
		// Send via AMS public web-server.
		auto contentJsonObject = MakeShared<FJsonObject>();
		contentJsonObject->SetStringField(TEXT("key"), AccountManager->GetApiKey());
		contentJsonObject->SetStringField(TEXT("idToken"), AccountManager->GetIdToken());
		contentJsonObject->SetStringField(TEXT("json"), FBase64::Encode(jsonString));
		FString contentjson;
		auto serializer = TJsonWriterFactory<>::Create(&contentjson);
		FJsonSerializer::Serialize(contentJsonObject, serializer);
		request->SetContentAsString(contentjson);
#endif

		request->ProcessRequest();

		return true;
	}
}
