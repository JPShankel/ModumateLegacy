// Copyright 2020 Modumate, Inc,  All Rights Reserved.

#include "DocumentManagement/DocumentDelta.h"

#include "Database/ModumateObjectEnums.h"

FStructDataWrapper FDocumentDelta::SerializeStruct()
{
	// The base delta class should never be serialized!
	check(false);
	return FStructDataWrapper();
}

FDeltasRecord::FDeltasRecord()
{
}

FDeltasRecord::FDeltasRecord(const TArray<FDeltaPtr>& InDeltas, int32 InID, const FString& InOriginUserID, uint32 InPrevDocHash)
	: ID(InID)
	, OriginUserID(InOriginUserID)
	, PrevDocHash(InPrevDocHash)
	, TimeStamp(FDateTime::Now())
	, RawDeltaPtrs(InDeltas)
{
	for (auto& deltaPtr : RawDeltaPtrs)
	{
		DeltaStructWrappers.Add(deltaPtr->SerializeStruct());
	}

	CacheSelfHash();
}

bool FDeltasRecord::IsEmpty() const
{
	return (DeltaStructWrappers.Num() == 0);
}

void FDeltasRecord::PostSerialize(const FArchive& Ar)
{
	CacheSelfHash();

	AffectedObjectsMap.FindOrAdd(EMOIDeltaType::Create).Append(AddedObjects);
	AffectedObjectsMap.FindOrAdd(EMOIDeltaType::Mutate).Append(ModifiedObjects);
	AffectedObjectsMap.FindOrAdd(EMOIDeltaType::Destroy).Append(DeletedObjects);
	DirtiedObjectsSet.Append(DirtiedObjects);
}

void FDeltasRecord::CacheSelfHash()
{
	CachedSelfHash = 0;
	for (auto& deltaWrapper : DeltaStructWrappers)
	{
		if (!deltaWrapper.IsValid())
		{
			deltaWrapper.SaveCborFromJson();
		}

		CachedSelfHash = HashCombine(CachedSelfHash, GetTypeHash(deltaWrapper));
	}

	CachedSelfHash = HashCombine(CachedSelfHash, GetTypeHash(ID));
	CachedSelfHash = HashCombine(CachedSelfHash, GetTypeHash(OriginUserID));
}

void FillObjIDArray(const TSet<int32>* AffectedObjectSetPtr, TArray<int32>& OutAffectedObjectArray)
{
	OutAffectedObjectArray.Reset();

	if (AffectedObjectSetPtr)
	{
		for (auto it = AffectedObjectSetPtr->CreateConstIterator(); it; ++it)
		{
			OutAffectedObjectArray.Add(*it);
		}
	}
}

void FDeltasRecord::SetResults(const FAffectedObjMap& InAffectedObjects, const TSet<int32>& InDirtiedObjects, const TArray<FGuid>& InAffectedPresets)
{
	AffectedObjectsMap = InAffectedObjects;
	DirtiedObjectsSet = InDirtiedObjects;

	FillObjIDArray(InAffectedObjects.Find(EMOIDeltaType::Create), AddedObjects);
	FillObjIDArray(InAffectedObjects.Find(EMOIDeltaType::Mutate), ModifiedObjects);
	FillObjIDArray(InAffectedObjects.Find(EMOIDeltaType::Destroy), DeletedObjects);
	FillObjIDArray(&DirtiedObjectsSet, DirtiedObjects);

	AffectedPresets = InAffectedPresets;
}

bool FDeltasRecord::operator==(const FDeltasRecord& Other) const
{
	return DeltaStructWrappers == Other.DeltaStructWrappers;
}

bool FDeltasRecord::operator!=(const FDeltasRecord& Other) const
{
	return !(*this == Other);
}

uint32 GetTypeHash(const FDeltasRecord& DeltasRecord)
{
	return HashCombine(DeltasRecord.PrevDocHash, DeltasRecord.CachedSelfHash);
}
