// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateRoomStatics.h"

#include "Algo/LevenshteinDistance.h"
#include "DynamicMeshActor.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelGameState_CPP.h"
#include "Graph3D.h"
#include "ModumateObjectDatabase.h"
#include "ModumateObjectInstance.h"

using namespace Modumate;

FRoomConfigurationBlueprint FRoomConfiguration::AsBlueprintObject() const
{
	return AsBlueprintObject(MOD_ID_NONE, FString(), 0.0f, 0);
}

FRoomConfigurationBlueprint FRoomConfiguration::AsBlueprintObject(int32 InObjectID, const FString &InRoomNumber, float InArea, int32 InOccupantsNumber) const
{
	FRoomConfigurationBlueprint ret;

	ret.Key = DatabaseKey;
	ret.ObjectID = InObjectID;
	ret.RoomNumber = InRoomNumber;
	ret.RoomColor = FColor::FromHex(HexValue);
	ret.Area = InArea;
	ret.UseGroupCode = UseGroupCode;
	ret.UseGroupType = UseGroupType;
	ret.DisplayName = DisplayName;
	ret.OccupantsNumber = InOccupantsNumber;
	ret.OccupantLoadFactor = OccupantLoadFactor;
	ret.AreaType = AreaType;
	ret.LoadFactorSpecialCalc = LoadFactorSpecialCalc;

	return ret;
}


const FName UModumateRoomStatics::DefaultRoomConfigKey(TEXT("Unassigned"));

bool UModumateRoomStatics::GetRoomConfigurationsFromTable(UObject* WorldContextObject, TArray<FRoomConfigurationBlueprint> &OutRoomConfigs)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameMode_CPP *gameMode = world ? world->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
	if (!ensure(gameMode && gameMode->ObjectDatabase))
	{
		return false;
	}

	for (auto &kvp : gameMode->ObjectDatabase->RoomConfigurations.DataMap)
	{
		OutRoomConfigs.Add(kvp.Value.AsBlueprintObject());
	}

	return true;
}

bool UModumateRoomStatics::GetRoomConfigurationsFromProject(UObject* WorldContextObject, TArray<FRoomConfigurationBlueprint> &OutRoomConfigs)
{
	OutRoomConfigs.Reset();

	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState_CPP *gameState = world ? Cast<AEditModelGameState_CPP>(world->GetGameState()) : nullptr;
	if (gameState == nullptr)
	{
		return false;
	}

	bool bSuccess = true;
	TArray<FModumateObjectInstance *> roomObjs = gameState->Document.GetObjectsOfType(EObjectType::OTRoom);
	for (auto *roomObj : roomObjs)
	{
		auto &roomConfig = OutRoomConfigs.AddDefaulted_GetRef();
		bSuccess = GetRoomConfig(roomObj, roomConfig) && bSuccess;
	}

	return bSuccess;
}

bool UModumateRoomStatics::GetRoomConfig(const FModumateObjectInstance *RoomObj, FRoomConfigurationBlueprint &OutRoomConfig)
{
	if (!ensure(RoomObj))
	{
		return false;
	}

	// TODO: expose these BIM values directly to Blueprint and delete FRoomConfigurationBlueprint, and/or
	// allow the proper serialization of types like FColor and FText to avoid unnecessary conversion.
	bool bSuccess = true;

	bSuccess = RoomObj->TryGetProperty<FName>(BIM::EScope::Room, BIM::Parameters::Preset, OutRoomConfig.Key) && bSuccess;

	OutRoomConfig.ObjectID = RoomObj->ID;

	bSuccess = RoomObj->TryGetProperty<FString>(BIM::EScope::Room, BIM::Parameters::Number, OutRoomConfig.RoomNumber) && bSuccess;

	FString colorString;
	bSuccess = RoomObj->TryGetProperty<FString>(BIM::EScope::Room, BIM::Parameters::Color, colorString) && bSuccess;
	OutRoomConfig.RoomColor = FColor::FromHex(colorString);

	bSuccess = RoomObj->TryGetProperty<float>(BIM::EScope::Room, BIM::Parameters::Area, OutRoomConfig.Area) && bSuccess;

	bSuccess = RoomObj->TryGetProperty<FName>(BIM::EScope::Room, BIM::Parameters::Code, OutRoomConfig.UseGroupCode) && bSuccess;

	FString groupTypeString;
	bSuccess = RoomObj->TryGetProperty<FString>(BIM::EScope::Room, BIM::Parameters::UseGroupType, groupTypeString) && bSuccess;
	OutRoomConfig.UseGroupType = FText::FromString(groupTypeString);

	FString displayNameString;
	bSuccess = RoomObj->TryGetProperty<FString>(BIM::EScope::Room, BIM::Parameters::Name, displayNameString) && bSuccess;
	OutRoomConfig.DisplayName = FText::FromString(displayNameString);

	bSuccess = RoomObj->TryGetProperty<int32>(BIM::EScope::Room, BIM::Parameters::OccupantsNumber, OutRoomConfig.OccupantsNumber) && bSuccess;

	bSuccess = RoomObj->TryGetProperty<float>(BIM::EScope::Room, BIM::Parameters::OccupantLoadFactor, OutRoomConfig.OccupantLoadFactor) && bSuccess;

	FString areaTypeString;
	bSuccess = RoomObj->TryGetProperty<FString>(BIM::EScope::Room, BIM::Parameters::AreaType, areaTypeString) &&
		TryEnumValueByString(EAreaType, areaTypeString, OutRoomConfig.AreaType) && bSuccess;

	FString loadFactorSpecialCalcString;
	bSuccess = RoomObj->TryGetProperty<FString>(BIM::EScope::Room, BIM::Parameters::LoadFactorSpecialCalc, loadFactorSpecialCalcString) && bSuccess;
	OutRoomConfig.LoadFactorSpecialCalc = FText::FromString(loadFactorSpecialCalcString);

	return bSuccess;
}

bool UModumateRoomStatics::GetRoomConfig(UObject* WorldContextObject, int32 RoomID, FRoomConfigurationBlueprint &OutRoomConfig)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState_CPP *gameState = world ? Cast<AEditModelGameState_CPP>(world->GetGameState()) : nullptr;
	FModumateObjectInstance *roomObj = gameState ? gameState->Document.GetObjectById(RoomID) : nullptr;
	return GetRoomConfig(roomObj, OutRoomConfig);
}

bool UModumateRoomStatics::SetRoomConfigFromKey(Modumate::FModumateObjectInstance *RoomObj, FName ConfigKey)
{
	UWorld *world = RoomObj ? RoomObj->GetWorld() : nullptr;
	AEditModelGameMode_CPP *gameMode = world ? world->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
	const FRoomConfiguration *roomConfig = gameMode ? gameMode->ObjectDatabase->GetRoomConfigByKey(ConfigKey) : nullptr;
	if (roomConfig == nullptr)
	{
		return false;
	}

	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::Preset, ConfigKey);
	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::Color, roomConfig->HexValue);
	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::Area, 0.0f);
	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::Code, roomConfig->UseGroupCode);
	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::UseGroupType, roomConfig->UseGroupType.ToString());
	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::Name, roomConfig->DisplayName.ToString());
	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::OccupantsNumber, 0);
	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::OccupantLoadFactor, roomConfig->OccupantLoadFactor);
	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::AreaType, EnumValueString(EAreaType, roomConfig->AreaType));
	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::LoadFactorSpecialCalc, roomConfig->LoadFactorSpecialCalc.ToString());

	// Mark the room as dirty, so that it will re-calculate area, update derived properties, and update its material.
	// TODO: calculate both gross and net area during structural clean (SetupDynamicGeometry), so that we only need to mark visuals as dirty.
	RoomObj->MarkDirty(EObjectDirtyFlags::Structure);

	return true;
}

bool UModumateRoomStatics::SetRoomConfigFromKey(UObject* WorldContextObject, int32 RoomID, FName ConfigKey)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	AEditModelGameState_CPP *gameState = world ? Cast<AEditModelGameState_CPP>(world->GetGameState()) : nullptr;
	FModumateObjectInstance *roomObj = gameState ? gameState->Document.GetObjectById(RoomID) : nullptr;

	return SetRoomConfigFromKey(roomObj, ConfigKey);
}

void UModumateRoomStatics::UpdateDerivedRoomProperties(Modumate::FModumateObjectInstance *RoomObj)
{
	// Retreive the room area
	ADynamicMeshActor *roomMesh = RoomObj ? Cast<ADynamicMeshActor>(RoomObj->GetActor()) : nullptr;
	if (!ensure(roomMesh))
	{
		return;
	}

	// Store the room area in ft^2, since that's how everyone will display it and use it in calculations
	// TODO: differentiate between gross and net area
	float roomAreaValueCM2 = roomMesh->GetBaseArea();
	float cm2Toft2 = FMath::Pow(InchesPerFoot * InchesToCentimeters, 2);
	float roomAreaValueFT2 = FMath::RoundHalfFromZero(roomAreaValueCM2 / cm2Toft2);
	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::Area, roomAreaValueFT2);

	// Calculate number of occupants
	int32 occupantsNumValue = 0;
	float occupantLoadFactor = 0.0f;
	if (RoomObj->TryGetProperty<float>(BIM::EScope::Room, BIM::Parameters::OccupantLoadFactor, occupantLoadFactor) &&
		(occupantLoadFactor > 0.0f))
	{
		occupantsNumValue = FMath::RoundHalfFromZero(roomAreaValueFT2 / occupantLoadFactor);
	}

	RoomObj->SetProperty(BIM::EScope::Room, BIM::Parameters::OccupantsNumber, occupantsNumValue);
}

bool UModumateRoomStatics::CanRoomContainFace(const Modumate::FModumateDocument *Document, FSignedID FaceID)
{
	const FGraph3DFace *graphFace = Document->GetVolumeGraph().FindFace(FaceID);
	const FModumateObjectInstance *planeObj = Document->GetObjectById(FMath::Abs(FaceID));

	if ((graphFace == nullptr) || (planeObj == nullptr))
	{
		return false;
	}

	// Only allow traversing to planes facing upwards (even slightly)
	FVector signedFaceNormal = FMath::Sign(FaceID) * FVector(graphFace->CachedPlane);
	float normalUpDot = (signedFaceNormal | FVector::UpVector);
	if (FMath::IsNearlyZero(normalUpDot) || (normalUpDot < 0.0f))
	{
		return false;
	}

	// Only allow traversing to planes that have a Floor child
	const TArray<int32> &planeChildIDs = planeObj->GetChildIDs();
	for (int32 planeChildID : planeChildIDs)
	{
		const FModumateObjectInstance *planeChildObj = Document->GetObjectById(planeChildID);
		if (planeChildObj && (planeChildObj->GetObjectType() == EObjectType::OTFloorSegment))
		{
			return true;
		}
	}

	return false;
}

void UModumateRoomStatics::CalculateRoomChanges(const Modumate::FModumateDocument *Document, bool &bOutAnyChange,
	TMap<int32, int32> &OutOldRoomIDsToNewRoomIndices,
	TMap<int32, TArray<int32>> &OutNewRoomsFaceIDs,
	TSet<int32> &OutOldRoomsToDeleteIDs,
	TSet<int32> &OutNewRoomsToCreateIndices)
{
	OutOldRoomIDsToNewRoomIndices.Reset();
	OutNewRoomsFaceIDs.Reset();
	OutOldRoomsToDeleteIDs.Reset();
	OutNewRoomsToCreateIndices.Reset();

	if (!ensure(Document))
	{
		return;
	}

	// Make abstract structures to store sorted FaceIDs that we can use to compare new and old room traversals
	TMap<int32, TArray<int32>> oldRoomsFaceIDs;

	// Gather the current set of room objects and their FaceIDs, to compare against the new room calculation
	TArray<const FModumateObjectInstance *> curRoomObjs = Document->GetObjectsOfType(EObjectType::OTRoom);
	for (const FModumateObjectInstance *curRoomObj : curRoomObjs)
	{
		TArray<FSignedID> roomFaceIDs = curRoomObj->GetControlPointIndices();
		roomFaceIDs.Sort();
		oldRoomsFaceIDs.Add(curRoomObj->ID, roomFaceIDs);
	}

	// Gather the navigable plane IDs that can be eligible for room traversal.
	// TODO: don't naively consider all floors; there may be other navigable plane-hosted objects.
	TSet<int32> floorPlaneIDs;
	TArray<const FModumateObjectInstance *> floorObjs = Document->GetObjectsOfType(EObjectType::OTFloorSegment);
	for (const FModumateObjectInstance *floorObj : floorObjs)
	{
		int32 floorParentID = floorObj->GetParentID();
		floorPlaneIDs.Add(floorParentID);
		floorPlaneIDs.Add(-floorParentID);
	}

	// Traverse the graph to find the separate rooms
	TArray<FGraph3DTraversal> roomTraversals;
	Document->GetVolumeGraph().TraverseFacesGeneric(floorPlaneIDs, roomTraversals,
		FGraph3D::AlwaysPassPredicate,
		[Document](FSignedID FaceID) { return UModumateRoomStatics::CanRoomContainFace(Document, FaceID); }
	);
	int32 totalNumRooms = roomTraversals.Num();

	// Store the new room info in a comparable data structure
	for (int32 roomIdx = 0; roomIdx < totalNumRooms; ++roomIdx)
	{
		TArray<FSignedID> roomFaceIDs = roomTraversals[roomIdx].FaceIDs;
		roomFaceIDs.Sort();
		OutNewRoomsFaceIDs.Add(roomIdx, MoveTemp(roomFaceIDs));
	}

	// Keep track of which old room IDs and new room indices are unmapped,
	// so need to either be deleted or created.
	TArray<int32> remainingNewRoomIndicesArray, remainingOldRoomIDsArray;
	OutNewRoomsFaceIDs.GetKeys(remainingNewRoomIndicesArray);
	oldRoomsFaceIDs.GetKeys(remainingOldRoomIDsArray);
	OutOldRoomsToDeleteIDs.Append(remainingOldRoomIDsArray);
	OutNewRoomsToCreateIndices.Append(remainingNewRoomIndicesArray);

	// TODO: replace with the Hungarian method of finding the optimal mapping
	// between old room face IDs and new room face IDs, but that's much more complicated.
	bOutAnyChange = (oldRoomsFaceIDs.Num() != OutNewRoomsFaceIDs.Num());
	TMap<int32, int32> newRoomIndicesToOldRoomIDs;
	for (auto &oldRoomKVP : oldRoomsFaceIDs)
	{
		int32 closestNewRoomIndex = INDEX_NONE;
		int32 closestRoomDist = 0;

		for (auto &newRoomKVP : OutNewRoomsFaceIDs)
		{
			int32 roomDist = Algo::LevenshteinDistance(oldRoomKVP.Value, newRoomKVP.Value);
			if ((closestNewRoomIndex == INDEX_NONE) || (roomDist < closestRoomDist))
			{
				closestNewRoomIndex = newRoomKVP.Key;
				closestRoomDist = roomDist;
			}
		}

		int32 oldRoomSize = oldRoomKVP.Value.Num();
		if ((closestNewRoomIndex != INDEX_NONE) && (closestRoomDist < oldRoomSize) &&
			!OutOldRoomIDsToNewRoomIndices.Contains(oldRoomKVP.Key) &&
			!newRoomIndicesToOldRoomIDs.Contains(closestNewRoomIndex))
		{
			OutOldRoomIDsToNewRoomIndices.Add(oldRoomKVP.Key, closestNewRoomIndex);
			newRoomIndicesToOldRoomIDs.Add(closestNewRoomIndex, oldRoomKVP.Key);

			OutOldRoomsToDeleteIDs.Remove(oldRoomKVP.Key);
			OutNewRoomsToCreateIndices.Remove(closestNewRoomIndex);

			if (closestRoomDist > 0)
			{
				bOutAnyChange = true;
			}
		}
	}

	if ((OutOldRoomsToDeleteIDs.Num() > 0) || (OutNewRoomsToCreateIndices.Num() > 0))
	{
		bOutAnyChange = true;
	}
}

void UModumateRoomStatics::CalculateRoomNumbers(const Modumate::FModumateDocument *Document,
	TMap<int32, FString> &OutOldRoomNumbers, TMap<int32, FString> &OutNewRoomNumbers)
{
	OutOldRoomNumbers.Reset();
	OutNewRoomNumbers.Reset();

	if (!ensure(Document))
	{
		return;
	}

	TArray<const FModumateObjectInstance *> curRoomObjs = Document->GetObjectsOfType(EObjectType::OTRoom);
	int32 totalNumRooms = curRoomObjs.Num();

	// TODO: handle having multiple floors
	// Calculate the maximum length of a room number, in order to have enough digits to sequence all possible rooms.
	// For example, in a building with 15 floors where floor 4 has the most rooms, 150 of them,
	// the base digits would be 3, so that floor 4 starts with room 4001, and floor 15 starts with 15001.
	// Or, for example, in a building with 1 floor and 4 rooms, base digits would be the minimum value of 2,
	// and rooms start at 101.
	int32 maxNumRoomsPerFloor = totalNumRooms;
	int32 totalRoomBaseDigits = FMath::Max(2, FMath::CeilToInt(FMath::LogX(10, maxNumRoomsPerFloor)));
	int32 totalRoomBase = FMath::Pow(10, totalRoomBaseDigits);

	int32 floorNum = 1;
	int32 roomBaseOnFloor = floorNum * totalRoomBase;

	for (int32 roomIdx = 0; roomIdx < totalNumRooms; ++roomIdx)
	{
		const FModumateObjectInstance *roomObj = curRoomObjs[roomIdx];
		if (!ensure(roomObj))
		{
			return;
		}

		FString oldRoomNumberValue;
		roomObj->TryGetProperty(BIM::EScope::Room, BIM::Parameters::Number, oldRoomNumberValue);

		int32 roomNumberInt = roomBaseOnFloor + roomIdx + 1;
		FString newRoomNumberValue = FText::AsNumber(roomNumberInt, &FNumberFormattingOptions::DefaultNoGrouping()).ToString();

		if (newRoomNumberValue != oldRoomNumberValue)
		{
			OutOldRoomNumbers.Add(roomObj->ID, oldRoomNumberValue);
			OutNewRoomNumbers.Add(roomObj->ID, newRoomNumberValue);
		}
	}
}
