// Copyright 2020 Modumate, Inc,  All Rights Reserved.

#include "DocumentManagement/DocumentDelta.h"

#include "Objects/ModumateObjectEnums.h"
#include "Objects/MOIDelta.h"

FStructDataWrapper FDocumentDelta::SerializeStruct()
{
	// The base delta class should never be serialized!
	check(false);
	return FStructDataWrapper();
}

FDeltasRecord::FDeltasRecord()
{
}

FDeltasRecord::FDeltasRecord(const TArray<FDeltaPtr>& InDeltas, const FString& InOriginUserID, uint32 InPrevDocHash)
	: OriginUserID(InOriginUserID)
	, PrevDocHash(InPrevDocHash)
	, TimeStamp(FDateTime::UtcNow())
	, RawDeltaPtrs(InDeltas)
{
	for (auto& deltaPtr : RawDeltaPtrs)
	{
		DeltaStructWrappers.Add(deltaPtr->SerializeStruct());
	}

	ComputeHash();
}

bool FDeltasRecord::IsEmpty() const
{
	return (DeltaStructWrappers.Num() == 0);
}

void FDeltasRecord::PostSerialize(const FArchive& Ar)
{
	if (!Ar.IsSaving())
	{
		if (RawDeltaPtrs.Num() == 0)
		{
			for (auto& structWrapper : DeltaStructWrappers)
			{
				RawDeltaPtrs.Add(MakeShareable(structWrapper.CreateStructFromJSON<FDocumentDelta>()));
			}
		}

		AffectedObjectsMap.FindOrAdd(EMOIDeltaType::Create).Append(AddedObjects);
		AffectedObjectsMap.FindOrAdd(EMOIDeltaType::Mutate).Append(ModifiedObjects);
		AffectedObjectsMap.FindOrAdd(EMOIDeltaType::Destroy).Append(DeletedObjects);
		DirtiedObjectsSet.Append(DirtiedObjects);
	}
}

void FDeltasRecord::ComputeHash()
{
	SelfHash = 0;
	for (auto& deltaWrapper : DeltaStructWrappers)
	{
		if (!deltaWrapper.IsValid())
		{
			deltaWrapper.SaveCborFromJson();
		}

		SelfHash = HashCombine(SelfHash, GetTypeHash(deltaWrapper));
	}

	SelfHash = HashCombine(SelfHash, GetTypeHash(OriginUserID));
	TotalHash = HashCombine(PrevDocHash, SelfHash);
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

void FDeltasRecord::SetResults(const FAffectedObjMap& InAffectedObjects, const TSet<int32>& InDirtiedObjects, const TArray<FGuid>& InAffectedPresets, const FBox& InAffectedObjBounds)
{
	AffectedObjectsMap = InAffectedObjects;
	DirtiedObjectsSet = InDirtiedObjects;

	FillObjIDArray(InAffectedObjects.Find(EMOIDeltaType::Create), AddedObjects);
	FillObjIDArray(InAffectedObjects.Find(EMOIDeltaType::Mutate), ModifiedObjects);
	FillObjIDArray(InAffectedObjects.Find(EMOIDeltaType::Destroy), DeletedObjects);
	FillObjIDArray(&DirtiedObjectsSet, DirtiedObjects);

	AffectedPresets = InAffectedPresets;
	AffectedObjBounds = InAffectedObjBounds;
}

void FDeltasRecord::GatherResults(TSet<int32>& OutAffectedObjects, TSet<FGuid>& OutAffectedPresets, FBox* OutAffectedObjBounds) const
{
	OutAffectedObjects.Append(AddedObjects);
	OutAffectedObjects.Append(ModifiedObjects);
	OutAffectedObjects.Append(DeletedObjects);
	OutAffectedObjects.Append(DirtiedObjects);
	OutAffectedPresets.Append(AffectedPresets);

	if (OutAffectedObjBounds)
	{
		*OutAffectedObjBounds += AffectedObjBounds;
	}
}

bool FDeltasRecord::ConflictsWithResults(const TSet<int32>& OtherAffectedObjects, const TSet<FGuid>& OtherAffectedPresets, const FBox* OtherAffectedObjBounds) const
{
	for (auto& kvp : AffectedObjectsMap)
	{
		for (int32 affectedObjID : kvp.Value)
		{
			if (OtherAffectedObjects.Contains(affectedObjID))
			{
				return true;
			}
		}
	}

	for (int32 dirtiedObjID : DirtiedObjects)
	{
		if (OtherAffectedObjects.Contains(dirtiedObjID))
		{
			return true;
		}
	}

	for (const FGuid& affectedPreset : AffectedPresets)
	{
		if (OtherAffectedPresets.Contains(affectedPreset))
		{
			return true;
		}
	}

	if (AffectedObjBounds.IsValid && OtherAffectedObjBounds && OtherAffectedObjBounds->IsValid &&
		AffectedObjBounds.Intersect(OtherAffectedObjBounds->ExpandBy(KINDA_SMALL_NUMBER)))
	{
		return true;
	}

	return false;
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
	return DeltasRecord.TotalHash;
}
