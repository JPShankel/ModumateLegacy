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

	bool LoadStructData(UScriptStruct* StructDef, void* DestStructPtr, bool bResetStruct = false) const;
	template<typename OutStructType>
	bool LoadStructData(OutStructType& OutStruct, bool bResetStruct = false) const
	{
		return LoadStructData(OutStructType::StaticStruct(), &OutStruct, bResetStruct);
	}

	// TODO: these three Save* helper functions should be unnecessary if we can more deeply customize JSON and/or CBOR serialization behavior.

	// If JSON wasn't saved during initialization (not by default for performance), it must be saved before serialization.
	bool SaveJsonFromCbor();

	// If we've only loaded from JSON, we must load internal struct reflection data before we can serialize to CBOR.
	bool LoadFromJson();

	// If CBOR wasn't loaded during deserialization, it must be generated before copying into structs.
	bool SaveCborFromJson();

	// If only the JSON string was loaded during deserialization (from CBOR), the JSON object and CBOR buffer must be saved.
	bool SaveFromJsonString();

	// Allocate an actual struct, created and loaded from JSON (rather than filling out an existing one from CBOR with LoadStructData)
	template<typename OutStructType>
	OutStructType* CreateStructFromJSON()
	{
		auto result = CreateInitStructRaw();
		if (result)
		{
			CreateStructFromJSONRaw(result);
		}
		return (OutStructType*)result;
	}

	bool IsValid() const;

	bool operator==(const FStructDataWrapper& Other) const;
	bool operator!=(const FStructDataWrapper& Other) const;

	// Function that's supposed to be automatically called after struct [de]serialization, for keeping CBOR and JSON data up to date
	void PostSerialize(const FArchive& Ar);

private:
	bool UpdateStructDefFromName();
	bool SaveStructDataJson(const void* StructPtr);
	bool SaveStructDataCbor(const void* StructPtr);
	void* CreateInitStructRaw();
	bool CreateStructFromJSONRaw(void* OutStructPtr);
	bool CreateInternalStruct();
	bool InitializeStruct(void* OutStructPtr) const;
	bool PostDeserializeStruct(void* OutStructPtr) const;
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

template<>
struct TStructOpsTypeTraits<FStructDataWrapper> : public TStructOpsTypeTraitsBase2<FStructDataWrapper>
{
	enum
	{
		WithPostSerialize = true
	};
};
