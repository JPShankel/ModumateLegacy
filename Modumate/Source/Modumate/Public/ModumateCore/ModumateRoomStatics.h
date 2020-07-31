// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataTables.h"
#include "Graph/Graph3DTypes.h"
#include "ModumateCore/ModumateTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Object.h"

#include "ModumateRoomStatics.generated.h"

USTRUCT(BlueprintType)
struct FRoomConfigurationBlueprint
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	int32 ObjectID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FString RoomNumber;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FColor RoomColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	float Area;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FName UseGroupCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText UseGroupType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	int32 OccupantsNumber;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	float OccupantLoadFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	EAreaType AreaType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	FText LoadFactorSpecialCalc;
};

// TODO: this should be defined by BIM, rather than a USTRUCT table row and a Blueprintable subclass
class MODUMATE_API FModumateDocument;
namespace Modumate
{
	struct MODUMATE_API FRoomConfiguration : public FRoomConfigurationTableRow
	{
		FName DatabaseKey;
		FName UniqueKey() const { return DatabaseKey; }
		FRoomConfigurationBlueprint AsBlueprintObject() const;
		FRoomConfigurationBlueprint AsBlueprintObject(int32 InObjectID, const FString &InRoomNumber, float InArea, int32 InOccupantsNumber) const;
	};

	class MODUMATE_API FModumateObjectInstance;
}

// Helper functions for accessing / editing room data and interpreting room geometry.
UCLASS(BlueprintType)
class MODUMATE_API UModumateRoomStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Rooms")
	static bool GetRoomConfigurationsFromTable(UObject* WorldContextObject, TArray<FRoomConfigurationBlueprint> &OutRoomConfigs);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Rooms")
	static bool GetRoomConfigurationsFromProject(UObject* WorldContextObject, TArray<FRoomConfigurationBlueprint> &OutRoomConfigs);

	static bool GetRoomConfig(const Modumate::FModumateObjectInstance *RoomObj, FRoomConfigurationBlueprint &OutRoomConfig);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Rooms")
	static bool GetRoomConfig(UObject* WorldContextObject, int32 RoomID, FRoomConfigurationBlueprint &OutRoomConfig);

	static bool SetRoomConfigFromKey(Modumate::FModumateObjectInstance *RoomObj, FName ConfigKey);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate | Rooms")
	static bool SetRoomConfigFromKey(UObject* WorldContextObject, int32 RoomID, FName ConfigKey);

	static void UpdateDerivedRoomProperties(Modumate::FModumateObjectInstance *RoomObj);

	static bool CanRoomContainFace(const FModumateDocument *Document, FGraphSignedID FaceID);

	static void CalculateRoomChanges(const FModumateDocument *Document, bool &bOutAnyChange,
		TMap<int32, int32> &OutOldRoomIDsToNewRoomIndices, TMap<int32, TArray<int32>> &OutNewRoomsFaceIDs,
		TSet<int32> &OutOldRoomsToDeleteIDs, TSet<int32> &OutNewRoomsToCreateIndices);

	static void CalculateRoomNumbers(const FModumateDocument *Document,
		TMap<int32, FString> &OutOldRoomNumbers, TMap<int32, FString> &OutNewRoomNumbers);

	static const FName DefaultRoomConfigKey;
};
