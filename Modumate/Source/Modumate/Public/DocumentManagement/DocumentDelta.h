// Copyright 2020 Modumate, Inc,  All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/StructDataWrapper.h"

#include "DocumentDelta.generated.h"

class UWorld;
class UModumateDocument;
enum class EMOIDeltaType : uint8;


USTRUCT()
struct MODUMATE_API FDocumentDelta
{
	GENERATED_BODY()

public:
	virtual ~FDocumentDelta() {}

	virtual bool ApplyTo(UModumateDocument* doc, UWorld* world) const { return false; };
	virtual TSharedPtr<FDocumentDelta> MakeInverse() const { return nullptr; };
	virtual FStructDataWrapper SerializeStruct();
	virtual void GetAffectedObjects(TArray<TPair<int32, EMOIDeltaType>>& OutAffectedObjects) const { }

protected:
	// Generic helper for populated the results of GetAffectedObjects
	template<typename ValueType>
	FORCEINLINE static void AddAffectedIDs(
		const TMap<int32, ValueType>& DeltaMap, EMOIDeltaType DeltaType, TArray<TPair<int32, EMOIDeltaType>>& OutAffectedObjects)
	{
		for (auto& kvp : DeltaMap)
		{
			OutAffectedObjects.Add(TPair<int32, EMOIDeltaType>(kvp.Key, DeltaType));
		}
	}

	// TODO: potentially, int32 ID if it is useful here
};

using FDeltaPtr = TSharedPtr<FDocumentDelta>;
using FAffectedObjMap = TMap<EMOIDeltaType, TSet<int32>>;

USTRUCT()
struct MODUMATE_API FDeltasRecord
{
	GENERATED_BODY()

	FDeltasRecord();
	FDeltasRecord(const TArray<FDeltaPtr>& InDeltas, const FString& InOriginUserID = FString(), uint32 InPrevDocHash = 0);

	bool IsEmpty() const;
	void ComputeHash();
	void SetResults(const FAffectedObjMap& InAffectedObjects, const TSet<int32>& InDirtiedObjects, const TArray<FGuid>& InAffectedPresets, const FBox& InAffectedObjBounds);
	void GatherResults(TSet<int32>& OutAffectedObjects, TSet<FGuid>& OutAffectedPresets, FBox* OutAffectedObjBounds = nullptr) const;
	bool ConflictsWithResults(const TSet<int32>& OtherAffectedObjects, const TSet<FGuid>& OtherAffectedPresets, const FBox* OtherAffectedObjBounds = nullptr) const;

	void PostSerialize(const FArchive& Ar);
	bool operator==(const FDeltasRecord& Other) const;
	bool operator!=(const FDeltasRecord& Other) const;
	friend uint32 GetTypeHash(const FDeltasRecord& DeltasRecord);

	// Definitional data, GetTypeHash depends on these

	UPROPERTY()
	TArray<FStructDataWrapper> DeltaStructWrappers;

	UPROPERTY()
	FString OriginUserID;

	UPROPERTY()
	uint32 SelfHash = 0;

	UPROPERTY()
	uint32 PrevDocHash = 0;

	UPROPERTY()
	uint32 TotalHash = 0;

	// Diagnostic, but still useful enough for replication

	UPROPERTY()
	FDateTime TimeStamp = FDateTime(0);

	UPROPERTY()
	TArray<int32> AddedObjects;

	UPROPERTY()
	TArray<int32> ModifiedObjects;

	UPROPERTY()
	TArray<int32> DeletedObjects;

	UPROPERTY()
	TArray<int32> DirtiedObjects;

	UPROPERTY()
	TArray<FGuid> AffectedPresets;

	UPROPERTY()
	FBox AffectedObjBounds = FBox(EForceInit::ForceInitToZero);

	// Non-definitional / derived from application; not necessarily worth the space for replication

	TArray<FDeltaPtr> RawDeltaPtrs;
	FAffectedObjMap AffectedObjectsMap;
	TSet<int32> DirtiedObjectsSet;
};

template<>
struct TStructOpsTypeTraits<FDeltasRecord> : public TStructOpsTypeTraitsBase2<FDeltasRecord>
{
	enum
	{
		WithPostSerialize = true,
		WithIdenticalViaEquality = true,
	};
};
