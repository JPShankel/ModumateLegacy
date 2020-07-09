// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateDataTables.h"
#include "Graph/Graph3DTypes.h"
#include "Database/ModumateShoppingItem.h"
#include "ModumateCore/ModumateTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Object.h"

#include "ModumateRoomStatics.generated.h"


// TODO: this should be defined by BIM, rather than a USTRUCT table row and a Blueprintable subclass
namespace Modumate
{
	struct FRoomConfiguration : public FRoomConfigurationTableRow
	{
		FName DatabaseKey;
		FName UniqueKey() const { return DatabaseKey; }
		FRoomConfigurationBlueprint AsBlueprintObject() const;
		FRoomConfigurationBlueprint AsBlueprintObject(int32 InObjectID, const FString &InRoomNumber, float InArea, int32 InOccupantsNumber) const;
	};

	class FModumateDocument;
	class FModumateObjectInstance;
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

	static bool CanRoomContainFace(const Modumate::FModumateDocument *Document, Modumate::FSignedID FaceID);

	static void CalculateRoomChanges(const Modumate::FModumateDocument *Document, bool &bOutAnyChange,
		TMap<int32, int32> &OutOldRoomIDsToNewRoomIndices, TMap<int32, TArray<int32>> &OutNewRoomsFaceIDs,
		TSet<int32> &OutOldRoomsToDeleteIDs, TSet<int32> &OutNewRoomsToCreateIndices);

	static void CalculateRoomNumbers(const Modumate::FModumateDocument *Document,
		TMap<int32, FString> &OutOldRoomNumbers, TMap<int32, FString> &OutNewRoomNumbers);

	static const FName DefaultRoomConfigKey;
};
