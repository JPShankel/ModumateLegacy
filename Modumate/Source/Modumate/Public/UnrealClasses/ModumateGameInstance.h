// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "Runtime/Engine/Public/TimerManager.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateUserSettings.h"

#include "ModumateGameInstance.generated.h"

static const TCHAR *kModumateRecoveryFile = TEXT("_tempBackupNew.mdmt");
static const TCHAR *kModumateRecoveryFileBackup = TEXT("_tempBackupOld.mdmt");
static const TCHAR *kModumateCleanShutdownFile = TEXT("_modumateDidNotShutDownCleanly.txt");

class MODUMATE_API UModumateDocument;
class MODUMATE_API FModumateAccountManager;
class MODUMATE_API FModumateCloudConnection;

class IAnalyticsProvider;

UENUM(BlueprintType)
enum class ELoginStatus : uint8
{
	None = 0,
	Disconnected,
	HaveValidRefreshToken,
	Connected,
	WaitingForRefreshToken,
	WaitingForVerify,
	WaitingForReverify,
	InvalidEmail,
	InvalidPassword,
	UserDisabled,
	ConnectionError,
	UnknownError
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
	TSharedPtr<FModumateAccountManager> AccountManager;
	TSharedPtr<FModumateCloudConnection> CloudConnection;

	UModumateDocument *GetDocument();

	void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void ProcessLogin(const FHttpResponsePtr Response);
	void CheckCrashRecovery();

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	void RegisterAllCommands();

	void RegisterCommand(const TCHAR *command, const std::function<bool(const Modumate::FModumateFunctionParameterSet &, Modumate::FModumateFunctionParameterSet &)> &fn);

	Modumate::FModumateFunctionParameterSet DoModumateCommand(const Modumate::FModumateCommand &command);

	void GetRegisteredCommands(TMap<FString, FString> &OutCommands);

	IAnalyticsProvider *GetAnalytics() const { return AnalyticsInstance.Get(); }

	TSharedPtr<FModumateAccountManager> GetAccountManager() const;
	TSharedPtr<FModumateCloudConnection> GetCloudConnection() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tooltips)
	TSubclassOf<class UTooltipManager> TooltipManagerClass;

	UFUNCTION(BlueprintCallable)
	ELoginStatus LoginStatus() const;

	UFUNCTION(BlueprintCallable)
	bool IsloggedIn() const;

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

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	class UDimensionManager* DimensionManager;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	class UTooltipManager* TooltipManager;

	static const FString TestScriptRelativePath;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool RecoveringFromCrash = false;
};