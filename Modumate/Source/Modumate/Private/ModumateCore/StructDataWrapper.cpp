// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/StructDataWrapper.h"

#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "JsonObjectConverter.h"
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

bool FStructDataWrapper::LoadStructData(UScriptStruct* StructDef, void* DestStructPtr) const
{
	if ((StructDef == nullptr) || (DestStructPtr == nullptr) || (StructDef != CachedStructDef))
	{
		return false;
	}

	FMemoryReader reader(StructCborBuffer);
	FCborStructDeserializerBackend deserializerBackend(reader);

	return FStructDeserializer::Deserialize(DestStructPtr, *StructDef, deserializerBackend);
}

bool FStructDataWrapper::SaveJsonFromCbor()
{
	if ((StructCborBuffer.Num() == 0) || !UpdateStructDefFromName() || !AllocateTempStruct())
	{
		return false;
	}

	bool bSuccess = LoadStructData(CachedStructDef, TempStructBuffer) &&
		SaveStructDataJson(TempStructBuffer);

	FreeTempStruct();
	return bSuccess;
}

bool FStructDataWrapper::SaveCborFromJson()
{
	// Require that the Json object is valid, can be turned into a string, and we've parsed the StructName for reflection
	if (!StructJson || !StructJson.JsonObjectToString(StructJson.JsonString) || !UpdateStructDefFromName() || !AllocateTempStruct())
	{
		return false;
	}

	bool bSuccess = FJsonObjectConverter::JsonObjectToUStruct(StructJson.JsonObject.ToSharedRef(), CachedStructDef, TempStructBuffer) &&
		SaveStructDataCbor(TempStructBuffer);

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

bool FStructDataWrapper::AllocateTempStruct()
{
	if (CachedStructDef == nullptr)
	{
		return false;
	}

	const int32 structAllocSize = CachedStructDef->GetStructureSize();
	if (structAllocSize == 0)
	{
		return false;
	}

	TempStructBuffer = (uint8*)FMemory::Malloc(structAllocSize, CachedStructDef->GetMinAlignment());
	if (TempStructBuffer == nullptr)
	{
		return false;
	}

	FMemory::Memzero(TempStructBuffer, structAllocSize);
	return true;
}

void FStructDataWrapper::FreeTempStruct()
{
	FMemory::Free(TempStructBuffer);
	TempStructBuffer = nullptr;
}
