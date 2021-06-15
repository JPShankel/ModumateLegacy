// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/StructDataWrapper.h"

#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "JsonObjectConverter.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryReader.h"
#include "StructDeserializer.h"
#include "StructSerializer.h"


TMap<FName, UScriptStruct*> FStructDataWrapper::FoundStructDefs;

FStructDataWrapper::FStructDataWrapper()
{
}

FStructDataWrapper::FStructDataWrapper(UScriptStruct* StructDef, const void* SrcStructPtr, bool bSaveJson)
{
	SaveStructData(StructDef, SrcStructPtr, bSaveJson);
}

bool FStructDataWrapper::SaveStructData(UScriptStruct* StructDef, const void* SrcStructPtr, bool bSaveJson)
{
	if ((StructDef == nullptr) || (SrcStructPtr == nullptr))
	{
		return false;
	}

	StructName = StructDef->GetFName();
	CachedStructDef = StructDef;

	if (bSaveJson && !SaveStructDataJson(SrcStructPtr))
	{
		return false;
	}

	if (!SaveStructDataCbor(SrcStructPtr))
	{
		return false;
	}

	return true;
}

bool FStructDataWrapper::LoadStructData(UScriptStruct* StructDef, void* OutStructPtr, bool bResetStruct) const
{
	if ((StructDef == nullptr) || (OutStructPtr == nullptr) || (StructDef != CachedStructDef))
	{
		return false;
	}

	if (bResetStruct)
	{
		CachedStructDef->DestroyStruct(OutStructPtr);
		InitializeStruct(OutStructPtr);
	}

	FMemoryReader reader(StructCborBuffer);
	FCborStructDeserializerBackend deserializerBackend(reader);

	return FStructDeserializer::Deserialize(OutStructPtr, *CachedStructDef, deserializerBackend) &&
		PostDeserializeStruct(OutStructPtr);
}

bool FStructDataWrapper::SaveJsonFromCbor()
{
	if ((StructCborBuffer.Num() == 0) || !UpdateStructDefFromName() || !CreateInternalStruct())
	{
		return false;
	}

	bool bSuccess = LoadStructData(CachedStructDef, TempStructBuffer) &&
		SaveStructDataJson(TempStructBuffer);

	FreeTempStruct();
	return bSuccess;
}

bool FStructDataWrapper::LoadFromJson()
{
	return StructJson && StructJson.JsonObjectToString(StructJson.JsonString) && UpdateStructDefFromName();
}

bool FStructDataWrapper::SaveCborFromJson()
{
	// Require that the Json object is valid, can be turned into a string, and we've parsed the StructName for reflection
	if (!LoadFromJson() || !CreateInternalStruct())
	{
		return false;
	}

	bool bSuccess = CreateStructFromJSONRaw(TempStructBuffer) && SaveStructDataCbor(TempStructBuffer);

	FreeTempStruct();
	return bSuccess;
}

bool FStructDataWrapper::SaveFromJsonString()
{
	return !StructJson.JsonString.IsEmpty() && StructJson.JsonObjectFromString(StructJson.JsonString) && SaveCborFromJson();
}

bool FStructDataWrapper::IsValid() const
{
	return !StructName.IsNone() && (StructCborBuffer.Num() > 0);
}

bool FStructDataWrapper::operator==(const FStructDataWrapper& Other) const
{
	return (StructName == Other.StructName) &&
		(CachedStructDef == Other.CachedStructDef) &&
		(StructCborBuffer.Num() == Other.StructCborBuffer.Num()) &&
		(FMemory::Memcmp(StructCborBuffer.GetData(), Other.StructCborBuffer.GetData(), StructCborBuffer.Num()) == 0);
}

bool FStructDataWrapper::operator!=(const FStructDataWrapper& Other) const
{
	return !(*this == Other);
}

uint32 GetTypeHash(const FStructDataWrapper& StructDataWrapper)
{
	if (!StructDataWrapper.IsValid())
	{
		return 0;
	}

	auto& structBuffer = StructDataWrapper.StructCborBuffer;
	return FCrc::MemCrc32(structBuffer.GetData(), structBuffer.Num(), GetTypeHash(StructDataWrapper.StructName.ToString()));
}

void FStructDataWrapper::PostSerialize(const FArchive& Ar)
{
	// If we're loading from JSON, then the JSON object should be valid, but we shouldn't have CBOR data yet, so finish doing that now.
	if (!Ar.IsSaving() && !StructName.IsNone() && StructJson && (StructCborBuffer.Num() == 0))
	{
		SaveCborFromJson();
	}
}

bool FStructDataWrapper::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	if (Ar.IsSaving())
	{
		bOutSuccess = (CachedStructDef && (StructCborBuffer.Num() > 0)) || SaveCborFromJson();
		if (bOutSuccess)
		{
			Ar << StructName;

			uint32 uncompressedSize = StructCborBuffer.Num();
			Ar.SerializeInt(uncompressedSize, MAX_uint32);
			Ar.SerializeBits(StructCborBuffer.GetData(), 8 * uncompressedSize);
		}
	}
	else
	{
		Ar << StructName;

		uint32 uncompressedSize = 0;
		Ar.SerializeInt(uncompressedSize, MAX_uint32);
		StructCborBuffer.AddZeroed(uncompressedSize);
		Ar.SerializeBits(StructCborBuffer.GetData(), 8 * uncompressedSize);

		bOutSuccess = SaveJsonFromCbor();
	}

	return true;
}

bool FStructDataWrapper::UpdateStructDefFromName()
{
	if (CachedStructDef && (CachedStructDef->GetFName() == StructName))
	{
		return true;
	}

	if (StructName.IsNone())
	{
		return false;
	}

	if (FoundStructDefs.Contains(StructName))
	{
		CachedStructDef = FoundStructDefs[StructName];
	}
	else
	{
		CachedStructDef = FindObject<UScriptStruct>((UObject*)ANY_PACKAGE, *StructName.ToString());
		if (ensure(CachedStructDef))
		{
			FoundStructDefs.Add(StructName, CachedStructDef);
		}
	}

	return (CachedStructDef != nullptr);
}

bool FStructDataWrapper::SaveStructDataJson(const void* SrcStructPtr)
{
	if ((CachedStructDef == nullptr) || (SrcStructPtr == nullptr))
	{
		return false;
	}

	StructJson.JsonObject = MakeShared<FJsonObject>();
	return FJsonObjectConverter::UStructToJsonObject(CachedStructDef, SrcStructPtr, StructJson.JsonObject.ToSharedRef(), 0, 0) &&
		StructJson.JsonObjectToString(StructJson.JsonString);
}

bool FStructDataWrapper::SaveStructDataCbor(const void* SrcStructPtr)
{
	if (CachedStructDef == nullptr)
	{
		return false;
	}

	StructCborBuffer.Reset();
	FMemoryWriter writer(StructCborBuffer);
	FCborStructSerializerBackend serializerBackend(writer, EStructSerializerBackendFlags::Default);

	FStructSerializerPolicies policies;
	policies.NullValues = EStructSerializerNullValuePolicies::Ignore;

	FStructSerializer::Serialize(SrcStructPtr, *CachedStructDef, serializerBackend, policies);
	return (StructCborBuffer.Num() > 0);
}

void* FStructDataWrapper::CreateInitStructRaw() const
{
	if ((CachedStructDef == nullptr) || !ensure(CachedStructDef->IsNative()))
	{
		return nullptr;
	}

	// Allocate space for the struct
	const int32 RequiredAllocSize = CachedStructDef->GetStructureSize();
	void* structPtr = FMemory::Malloc(RequiredAllocSize, CachedStructDef->GetMinAlignment());
	InitializeStruct(structPtr);

	return structPtr;
}

bool FStructDataWrapper::CreateStructFromJSONRaw(void* OutStructPtr) const
{
	return StructJson.JsonObject.IsValid() && CachedStructDef &&
		FJsonObjectConverter::JsonObjectToUStruct(StructJson.JsonObject.ToSharedRef(), CachedStructDef, OutStructPtr) &&
		PostDeserializeStruct(OutStructPtr);
}

bool FStructDataWrapper::CreateInternalStruct()
{
	TempStructBuffer = (uint8*)CreateInitStructRaw();
	return (TempStructBuffer != nullptr);
}

bool FStructDataWrapper::InitializeStruct(void* OutStructPtr) const
{
	if (CachedStructDef == nullptr)
	{
		return false;
	}

	CachedStructDef->InitializeStruct(OutStructPtr);

	// Perform script-based construction if necessary
	if (CachedStructDef->StructFlags & STRUCT_PostScriptConstruct)
	{
		UScriptStruct::ICppStructOps* structOps = CachedStructDef->GetCppStructOps();
		if (ensure(structOps))
		{
			structOps->PostScriptConstruct(OutStructPtr);
		}
	}

	return true;
}

bool FStructDataWrapper::PostDeserializeStruct(void* OutStructPtr) const
{
	// Execute PostSerialize if necessary
	if ((CachedStructDef == nullptr) || (OutStructPtr == nullptr))
	{
		return false;
	}

	if (CachedStructDef->StructFlags & STRUCT_PostSerializeNative)
	{
		UScriptStruct::ICppStructOps* structOps = CachedStructDef->GetCppStructOps();
		if (ensure(structOps))
		{
			FArchive dummyArchive;
			structOps->PostSerialize(dummyArchive, OutStructPtr);
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}

void FStructDataWrapper::FreeTempStruct()
{
	CachedStructDef->DestroyStruct(TempStructBuffer);
	FMemory::Free(TempStructBuffer);
	TempStructBuffer = nullptr;
}
