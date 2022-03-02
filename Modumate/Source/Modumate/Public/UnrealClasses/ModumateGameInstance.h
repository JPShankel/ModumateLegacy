// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "Engine/GameInstance.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "Runtime/Engine/Public/TimerManager.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateUserSettings.h"

#include "ModumateGameInstance.generated.h"

static const TCHAR *kModumateRecoveryFile = TEXT("_tempBackupNew.mdmt");
static const TCHAR *kModumateRecoveryFileBackup = TEXT("_tempBackupOld.mdmt");
static const TCHAR *kModumateCleanShutdownFile = TEXT("_modumateDidNotShutDownCleanly.txt");

class AEditModelGameMode;
class UModumateDocument;
class FModumateAccountManager;
class FModumateCloudConnection;
class FQuantitiesManager;
class UDrawingDesignerMeshCache;

class IAnalyticsProvider;

UENUM(BlueprintType)
enum class ELoginStatus : uint8
{
	None = 0,
	Disconnected,
	Connected,
	WaitingForRefreshToken,
	WaitingForReverify,
	InvalidCredentials,
	UserDisabled,
	ConnectionError
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
	TMap<FString, FModumateFunction*> CommandMap;
	TSharedPtr<IAnalyticsProvider> AnalyticsInstance;
	TSharedPtr<FModumateAccountManager> AccountManager;
	TSharedPtr<FModumateCloudConnection> CloudConnection;
	TSharedPtr<FQuantitiesManager> QuantitiesManager;

	UPROPERTY()
	UDrawingDesignerMeshCache* DrawingDesignerMeshCache = nullptr;

	FText PendingMainMenuStatus;
	UModumateDocument* GetDocument();

	void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void ProcessLogin(const FHttpResponsePtr Response);
	void CheckCrashRecovery();

public:
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void StartGameInstance() override;


	virtual void ReceivedNetworkEncryptionToken(const FString& EncryptionToken, const FOnEncryptionKeyResponse& Delegate) override;
	virtual void ReceivedNetworkEncryptionAck(const FOnEncryptionKeyResponse& Delegate) override;

	void RegisterAllCommands();

	void RegisterCommand(const TCHAR *InCommand, const TFunction<bool(const FModumateFunctionParameterSet &, FModumateFunctionParameterSet &)> &InFN);

	FModumateFunctionParameterSet DoModumateCommand(const FModumateCommand &command);

	void GetRegisteredCommands(TMap<FString, FString> &OutCommands);

	IAnalyticsProvider *GetAnalytics() const { return AnalyticsInstance.Get(); }

	TSharedPtr<FModumateAccountManager> GetAccountManager() const;
	TSharedPtr<FModumateCloudConnection> GetCloudConnection() const;
	TSharedPtr<FQuantitiesManager> GetQuantitiesManager() const;
	UDrawingDesignerMeshCache* GetMeshCache() const;

	const AEditModelGameMode* GetEditModelGameMode() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tooltips)
	TSubclassOf<class UTooltipManager> TooltipManagerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UModumateTutorialManager> TutorialManagerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UDrawingDesignerMeshCache> DrawingDesignerMeshCacheClass;

	UFUNCTION(BlueprintCallable)
	ELoginStatus LoginStatus() const;

	UFUNCTION(BlueprintCallable)
	bool IsloggedIn() const;

	UFUNCTION(BlueprintCallable, Category = ModumateLogin)
	void Login(const FString& UserName, const FString& Password, const FString& RefreshToken, bool bSaveUserName = false, bool bSaveRefreshToken = false);

	UFUNCTION(Exec, Category = ExecFunctions)
	void Modumate(const FString &Params);

	UFUNCTION()
	bool ProcessCustomURLArgs(const FString& Args);

	UFUNCTION()
	void SlowTick();

	UFUNCTION()
	void GoToMainMenu(const FText& StatusMessage);

	UFUNCTION()
	void OnKickedFromMPSession(const FText& KickReason);

	UFUNCTION()
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorMessage);

	UFUNCTION()
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDrive, ENetworkFailure::Type FailureType, const FString& ErrorMessage);

	UFUNCTION()
	void OnStartConnectCloudProject(const FString& NewProjectID);

	UFUNCTION()
	void OnEndConnectCloudProject();

	UFUNCTION()
	bool CheckMainMenuStatus(FText& OutStatusMessage);

	UFUNCTION()
	void create_local_project_from_ams();

	UFUNCTION()
	void open_cloud_project_from_ams(int32 ProjectID);

	UFUNCTION()
	void pass_user_package_from_ams(const FString& content);

	UFUNCTION()
	void open_offline_project_from_ams();

	UFUNCTION()
	FString get_refresh_token_for_ams() const;

	UFUNCTION()
	void open_url_in_browser_from_ams(const FString& InURL);

	UFUNCTION()
	void on_logout_from_ams();

	void BringViewportWindowToFront();
	void SetDownloadedDocument(const FModumateDocumentHeader& InDownloadedDocHeader, const FMOIDocumentRecord& InDownloadedDocRecord);
	void ClearDownloadedDocument();

	void ApplyGraphicsFromModumateUserSettings();
	void AutoDetectAndSaveModumateUserSettings();

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

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	class UModumateTutorialManager* TutorialManager;

	class FModumateDatabase* ObjectDatabase;

	static const FString TestScriptRelativePath;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool RecoveringFromCrash = false;

	bool HasAskedForRestrictedFileUpload = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString PendingProjectPath;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString PendingInputLogPath;

	// The ID of a cloud-hosted project for which a multiplayer client should establish a server connection
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString PendingClientConnectProjectID;

	// The ID of a cloud-hosted project to which a multiplayer client is currently connecting,
	// starting from the Main Menu level until the project finishes loading in the Edit Model level.
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString CurClientConnectProjectID;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString DocumentProjectID;

	FTimerHandle SlowTickHandle;

	bool bExitRequested = false;
};