// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateAutotraceConnect.h"

#include "Logging/LogMacros.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Online/ModumateAccountManager.h"
#include "Misc/FileHelper.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Objects/CutPlane.h"

namespace Modumate
{
	struct FModumateAutotraceConnect::ResponseHandler
	{
		void operator()(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
		UWorld* World;
		FMOICutPlaneImpl* CutPlane;
		int32 CutPlaneID { 0 };
		int32 RenderID{ 0 };

		enum ResponseCode { kInvalidIdToken = 401, kErrorConverting = 463 };
	};

	void FModumateAutotraceConnect::ResponseHandler::operator()(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		FString jsonResponse;
		int32 code = Response->GetResponseCode();
		if (bWasSuccessful && code == EHttpResponseCodes::Ok)
		{
			const TArray<uint8>& dwgContents = Response->GetContent();
			jsonResponse = FString(dwgContents.Num(), (char*) &dwgContents[0]);
		}
		else
		{
			UE_LOG(LogAutoDrafting, Error, TEXT("AutoTrace server returned fail - code %d"), code);
			if (code == kErrorConverting)
			{
				UE_LOG(LogAutoDrafting, Error, TEXT("Reason: %s"),  *Response->GetContentAsString());
			}
		}

		AEditModelGameState_CPP *gameState = World->GetGameState<AEditModelGameState_CPP>();
		FModumateDocument &document = gameState->Document;

		if (document.GetObjectById(CutPlaneID) != nullptr)
		{	// Cut plane exists.
			CutPlane->TraceRequestComplete(RenderID, MoveTemp(jsonResponse));
		}
	}

	bool FModumateAutotraceConnect::ConvertImageFromFile(const FString& filename, int32 renderID, FMOICutPlaneImpl* cutPlane, int32 cutPlaneID, UWorld* world)
	{
		TArray<uint8> pngFile;
		if (!FFileHelper::LoadFileToArray(pngFile, *filename))
		{
			return false;
		}

		UModumateGameInstance* gameInstance = world->GetGameInstance<UModumateGameInstance>();
		const auto accountManager = gameInstance->GetAccountManager();

		TSharedRef<IHttpRequest> request = FHttpModule::Get().CreateRequest();
		ResponseHandler handler;
		handler.World = world;
		handler.CutPlaneID = cutPlaneID;
		handler.CutPlane = cutPlane;
		handler.RenderID = renderID;
		request->OnProcessRequestComplete().BindLambda(handler);

		request->SetVerb(TEXT("POST"));
		request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
		request->SetHeader(TEXT("Content-Type"), TEXT("image/png"));

#if 0
		// Local server:
		static const FString ServerAddress(TEXT("http://localhost:8081"));
		request->SetURL(ServerAddress + "/autotrace");
		request->SetHeader(TEXT("Authorization"), TEXT("Basic YW1zOmtpZ2VibXk2dWtrNzN3"));
#else
		request->SetURL(accountManager->GetAmsAddress() + TEXT("/api/v2/service/autotrace"));
		request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + accountManager->GetIdToken());
#endif

		request->SetContent(pngFile);

		return request->ProcessRequest();
	}

}
