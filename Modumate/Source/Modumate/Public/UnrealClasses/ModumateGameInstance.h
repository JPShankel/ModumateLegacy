// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "Runtime/Engine/Public/TimerManager.h"
#include "ModumateConsoleCommand.h"
#include "ModumateUserSettings.h"

#include "ModumateGameInstance.generated.h"

static const TCHAR *kModumateRecoveryFile = TEXT("_tempBackupNew.mdmt");
static const TCHAR *kModumateRecoveryFileBackup = TEXT("_tempBackupOld.mdmt");
static const TCHAR *kModumateCleanShutdownFile = TEXT("_modumateDidNotShutDownCleanly.txt");

namespace Modumate
{
	class FModumateDocument;
}

class IAnalyticsProvider;

UENUM(BlueprintType)
enum class ELoginStatus : uint8
{
	None = 0,
	Disconnected,
	Connected,
	WaitingForServer,
	InvalidEmail,
	InvalidPassword,
	UserDisabled,
	ConnectionError,
	UnknownError
};


USTRUCT()
struct MODUMATE_API FGoogleEmailPasswordLoginParams
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString email;

	UPROPERTY()
	FString password;

	UPROPERTY()
	bool returnSecureToken = true;
};

USTRUCT()
struct MODUMATE_API FGoogleErrorType
{
	GENERATED_USTRUCT_BODY();
	
	UPROPERTY()
	FString message;

	UPROPERTY()
	FString domain;

	UPROPERTY()
	FString reason;
};

USTRUCT()
struct MODUMATE_API FGoogleErrorMessage
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	int32 code;

	UPROPERTY()
	FString message;

	UPROPERTY()
	TArray<FGoogleErrorType> errors;
};

USTRUCT()
struct MODUMATE_API FGoogleErrorResponse
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FGoogleErrorMessage error;
};

USTRUCT()
struct MODUMATE_API FGoogleEmailPasswordLoginResponse
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString kind;

	UPROPERTY()
	FString localId;

	UPROPERTY()
	FString email;

	UPROPERTY()
	FString displayName;

	UPROPERTY()
	FString idToken;

	UPROPERTY()
	bool registered;

	UPROPERTY()
	FString refreshToken;

	UPROPERTY()
	FString expiresIn;
};


/**
 *
 */
UCLASS()
class MODUMATE_API UModumateGameInstance : public UGameInstance
{
	GENERATED_UCLASS_BODY()

private:
	FTimerHandle ConsoleWaitTimer;
	TArray<FString> CommandQueue;
	void ProcessCommandQueue();
	TMap<FString, Modumate::FModumateFunction*> CommandMap;
	TSharedPtr<IAnalyticsProvider> AnalyticsInstance;

	Modumate::FModumateDocument *GetDocument();

	void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void CheckCrashRecovery();

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	void RegisterAllCommands();

	void RegisterCommand(const TCHAR *command, const std::function<bool(const Modumate::FModumateFunctionParameterSet &, Modumate::FModumateFunctionParameterSet &)> &fn);

	Modumate::FModumateFunctionParameterSet DoModumateCommand(const Modumate::FModumateCommand &command);

	void GetRegisteredCommands(TMap<FString, FString> &OutCommands);

	IAnalyticsProvider *GetAnalytics() const { return AnalyticsInstance.Get(); }

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	ELoginStatus LoginStatus = ELoginStatus::Disconnected;

	FString GoogleLocalId;
	FString GoogleRefreshToken;

	UFUNCTION(BlueprintCallable, Category = ModumateLogin)
	void Login(const FString &userName, const FString &password);

	UFUNCTION(Exec, Category = ExecFunctions)
	void Modumate(const FString &params);

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FModumateUserSettings UserSettings;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	class UThumbnailCacheManager* ThumbnailCacheManager;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	class UDraftingManager* DraftingManager;

	UPROPERTY()
	FString TestScriptRelativePath;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool RecoveringFromCrash = false;
};
