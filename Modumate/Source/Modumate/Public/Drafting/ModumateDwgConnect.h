// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Drafting/ModumateDwgDraw.h"
#include "Runtime/Online/HTTP/Public/Http.h"


class FModumateAccountManager;
class UModumateGameInstance;

/**
	* Encapsulate connection to cloud drafting server.
	*/
class FModumateDwgConnect
{
public:
	FModumateDwgConnect(const FModumateDwgDraw& dwgDraw);
	~FModumateDwgConnect();

	bool ConvertJsonToDwg(FString filename);

private:
	const FModumateDwgDraw& DwgDraw;
	FString Filename;
	FString ProjectID;  // For multiplayer.
	TSharedPtr<FModumateAccountManager> AccountManager;
	UModumateGameInstance* GameInstance = nullptr;

	static const FString ServerAddress;

	bool UploadJsonAndSave();
	bool CallDwgServer(const FString& jsonString, const FString& saveFilename);

	// Class for persistent aspects needed for HTTP reply callbacks.
	class DwgSaver;
	TSharedPtr<DwgSaver> ResponseSaver;
};
