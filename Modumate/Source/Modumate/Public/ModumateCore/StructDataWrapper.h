// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "JsonObjectWrapper.h"

#include "StructDataWrapper.generated.h"

#define DEFAULT_SAVE_JSON false

/**
 * A wrapper around an arbitrary USTRUCT or UCLASS that stores it by value,
 * allows for easy JSON and CBOR serialization, and fast + easy conversion back to the original data.
 */
USTRUCT()
struct MODUMATE_API FStructDataWrapper
{
	GENERATED_BODY()

	FStructDataWrapper();
	FStructDataWrapper(UScriptStruct* StructDef, const void* SrcStructPtr, bool bSaveJson = DEFAULT_SAVE_JSON);

	bool SaveStructData(UScriptStruct* StructDef, const void* SrcStructPtr, bool bSaveJson = DEFAULT_SAVE_JSON);
	template<typename InStructType>
	bool SaveStructData(const InStructType& InStruct, bool bSaveJson = DEFAULT_SAVE_JSON)
	{
		return SaveStructData(InStructType::StaticStruct(), &InStruct, bSaveJson);
	}

	bool LoadStructData(UScriptStruct* StructDef, void* DestStructPtr) const;
	template<typename OutStructType>
	bool LoadStructData(OutStructType& OutStruct) const
	{
		return LoadStructData(OutStructType::StaticStruct(), &OutStruct);
	}

	// TODO: these three Save* helper functions should be unnecessary if we can more deeply customize JSON and/or CBOR serialization behavior.

	// If JSON wasn't saved during initialization (not by default for performance), it must be saved before serialization.
	bool SaveJsonFromCbor();

	// If CBOR wasn't loaded during deserialization (not currently possible from JSON), it must be generated before copying into structs.
	bool SaveCborFromJson();

	// If only the JSON string was loaded during deserialization (from CBOR), the JSON object and CBOR buffer must be saved.
	bool SaveFromJsonString();

	bool IsValid() const;

	bool operator==(const FStructDataWrapper& Other) const;
	bool operator!=(const FStructDataWrapper& Other) const;

private:
	bool UpdateStructDefFromName();
	bool SaveStructDataJson(const void* StructPtr);
	bool SaveStructDataCbor(const void* StructPtr);
	bool AllocateTempStruct();
	void FreeTempStruct();

	UPROPERTY()
	FName StructName = NAME_None;

	UPROPERTY()
	FJsonObjectWrapper StructJson;

	UScriptStruct* CachedStructDef = nullptr;
	TArray<uint8> StructCborBuffer;
	uint8* TempStructBuffer = nullptr;

	static TMap<FName, UScriptStruct*> FoundStructDefs;
};
