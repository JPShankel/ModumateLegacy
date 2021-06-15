// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/ModumateSerialization.h"
#include "Online/ModumateAccountManager.h"
#include "UnrealClasses/ModumateGameModeBase.h"

#include "EditModelGameMode.generated.h"

class MODUMATE_API FModumateDatabase;
class UDataTable;

UCLASS()
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

	static const FString ProjectIDArg;
	static const FString APIKeyArg;
	static const FString EncryptionTokenKey;

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
	UStaticMesh* GrassMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface* GrassMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *BillboardTextureMaterial;

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

protected:
	FString CurProjectID;
	FString CurAPIKey;
	TMap<FString, int32> PlayersByUserID;
	TMap<int32, FString> UsersByPlayerID;

	void ResetSession();
};
