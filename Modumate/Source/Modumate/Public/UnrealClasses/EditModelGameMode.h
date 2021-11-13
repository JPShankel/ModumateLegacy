// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/ModumateSerialization.h"
#include "Online/ModumateAccountManager.h"
#include "UnrealClasses/ModumateGameModeBase.h"

#include "EditModelGameMode.generated.h"

class MODUMATE_API FModumateDatabase;
class UDataTable;

UCLASS(Config=Game)
class MODUMATE_API AEditModelGameMode : public AModumateGameModeBase
{
	GENERATED_BODY()

public:

	AEditModelGameMode();

	//~ Begin AGameModeBase Interface
	virtual void InitGameState() override;
	//~ End AGameModeBase Interface

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	const FString& GetCurProjectID() const;
	bool GetUserByPlayerID(int32 PlayerID, FString& OutUserID) const;
	bool GetPlayerByUserID(const FString& UserID, int32& OutPlayerID) const;
	bool IsAnyUserPendingLogin() const;
	void OnUserDownloadedDocument(const FString& UserID, uint32 DownloadedDocHash);
	void OnServerUploadedDocument(uint32 UploadedDocHash);

	static const FString ProjectIDArg;
	static const FString APIKeyArg;
	static const FString CloudUrl;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UClass *PortalFrameActorClass;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UClass *MOIGroupActorClass;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *GreenMaterial;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *HideMaterial;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *AnchorMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *VolumeHoverMaterial;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *RoomHandleMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *RoomHandleMaterialNoDepth;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *SphereMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *PlaneMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *DefaultObjectMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *MetaPlaneMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *CutPlaneMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *ScopeBoxMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *LineMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *LineMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *VertexMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh* VertexMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *TerrainMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface* TerrainMaterialTranslucent;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh* GrassMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface* GrassMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *BillboardTextureMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *DepthMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *SobelEdgeMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UTexture2D *ButtonEditGreenTexture;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UTexture2D *ButtonEditRedTexture;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* RoomConfigurationTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Editing")
	TSubclassOf<class ADynamicMeshActor> DynamicMeshActorClass;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Editing")
	TSubclassOf<class ACutPlaneCaptureActor> CutPlaneCaptureActorClass;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pawn")
	TSubclassOf<class AEditModelToggleGravityPawn> ToggleGravityPawnClass;

	UPROPERTY(Config, EditAnywhere, Category = "Server")
	float ServerAutoExitTimeout = 120.0f;
	
	static constexpr float ServerStatusTimerRate = 1.0f;

protected:
	// The ID of the project that's being used by this server instance, for initial downloading and subsequent uploading
	FString CurProjectID;

	// The PlayerID (as used by PlayerState) for each connected client, keyed by their AMS UserID
	TMap<FString, int32> PlayersByUserID;

	// The AMS UserID for each connected client, keyed by their PlayerState's PlayerID
	TMap<int32, FString> UsersByPlayerID;

	// The ordered list of AMS User IDs, in which entries are cleared rather than removed in order to calculate unique indices for each active client.
	// This is only necessary because PlayerState's PlayerID is monotonically increasing, so a single user connecting and disconnecting will keep receiving a higher PlayerID.
	TArray<FString> OrderedUserIDs;

	// For users who are currently logging in, the document hash that they've reported as having downloaded.
	TMap<FString, uint32> PendingLoginDocHashByUserID;

	// If a user logged in and we need to upload the document before they could log in, then keep track of whether that upload is finished,
	// so that we can tell the user to start downloading it when it's finished.
	TSet<uint32> PendingLoginUploadDocHashes;

	FTimerHandle ServerStatusTimer;
	float TimeWithoutPlayers = 0.0f;

	UFUNCTION()
	APlayerController* FindControllerByUserID(const FString& UserID);

	UFUNCTION()
	bool TrySendUserInitialState(APlayerController* NewPlayer);

	UFUNCTION()
	void OnServerStatusTimer();
};
