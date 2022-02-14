// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateSerialization.h"

#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "StructDeserializer.h"
#include "StructSerializer.h"

const FString FModumateSerializationStatics::DocHeaderField(TEXT("ModumateHeader"));
const FString FModumateSerializationStatics::DocObjectInstanceField(TEXT("ModumateObjects"));


FGraph3DVertexRecordV1::FGraph3DVertexRecordV1()
	: ID(MOD_ID_NONE)
	, Position()
{ }

FGraph3DVertexRecordV1::FGraph3DVertexRecordV1(int32 InID, const FVector& InPosition)
	: ID(InID)
	, Position(InPosition)
{ }

FGraph3DEdgeRecordV1::FGraph3DEdgeRecordV1()
	: ID(MOD_ID_NONE)
	, StartVertexID(MOD_ID_NONE)
	, EndVertexID(MOD_ID_NONE)
{ }

FGraph3DEdgeRecordV1::FGraph3DEdgeRecordV1(int32 InID, int32 InStartVertexID, int32 InEndVertexID, const TSet<int32>& InGroupIDs)
	: ID(InID)
	, StartVertexID(InStartVertexID)
	, EndVertexID(InEndVertexID)
	, GroupIDs(InGroupIDs)
{ }

FGraph3DFaceRecordV1::FGraph3DFaceRecordV1()
	: ID(MOD_ID_NONE)
	, ContainingFaceID(MOD_ID_NONE)
{ }

FGraph3DFaceRecordV1::FGraph3DFaceRecordV1(int32 InID, const TArray<int32>& InVertexIDs, const TSet<int32>& InGroupIDs, int32 InContainingFaceID, const TSet<int32>& InContainedFaceIDs)
	: ID(InID)
	, VertexIDs(InVertexIDs)
	, GroupIDs(InGroupIDs)
	, ContainingFaceID(InContainingFaceID)
	, ContainedFaceIDs(InContainedFaceIDs)
{ }


void FMOIDocumentRecordV4::FromVersion3(const FMOIDocumentRecordV3 &v3)
{
}

FString FCustomAssemblyProperty::ToCompactString() const
{
	FString result;
	const UStruct* traceStructDefinition = FCustomAssemblyProperty::StaticStruct();
	static const FString propSeparator(TEXT(";"));

	for (TFieldIterator<FProperty> propIter(traceStructDefinition); propIter; ++propIter)
	{
		FProperty* structPropDef = *propIter;
		const void* structPropValue = structPropDef->ContainerPtrToValuePtr<uint8>(this);
		FString structPropString;
		structPropDef->ExportTextItem(structPropString, structPropValue, NULL, NULL, PPF_None);
		result += structPropString + propSeparator;
	}

	return result;
}

/*
Used by Document::Load and the object database which uses a saved game as a proxy for a future cache of downloaded shopping assemblies
*/
bool FModumateSerializationStatics::TryReadModumateDocumentRecord(const FString &filePath, FModumateDocumentHeader &OutHeader, FMOIDocumentRecord &OutRecord)
{
#if WITH_EDITOR
	// Developer builds - allow loading of binary (multiplayer) files:
	if (filePath.EndsWith(TEXT(".mdmb")))
	{
		TArray<uint8> fileArray;
		if (FFileHelper::LoadFileToArray(fileArray, *filePath))
		{
			if (LoadDocumentFromBuffer(fileArray, OutHeader, OutRecord))
			{
				return true;
			}
		}
		return false;
	}
#endif

	FString FileJsonString;
	bool bLoadFileSuccess = FFileHelper::LoadFileToString(FileJsonString, *filePath);
	if (!bLoadFileSuccess)
	{
		return false;
	}

	auto JsonReader = TJsonReaderFactory<>::Create(FileJsonString);

	TSharedPtr<FJsonObject> FileJson;
	bool bLoadJsonSuccess = FJsonSerializer::Deserialize(JsonReader, FileJson) && FileJson.IsValid();
	if (!bLoadJsonSuccess)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> *headerOb;
	if (!FileJson->TryGetObjectField(FModumateSerializationStatics::DocHeaderField, headerOb))
	{
		return false;
	}

	// TODO: remove when header V1 is no longer in the field
	bool gotHeader = FJsonObjectConverter::JsonObjectToUStruct<FModumateDocumentHeader>(headerOb->ToSharedRef(), &OutHeader);
	if (!gotHeader || OutHeader.Version == 0)
	{
		FModumateDocumentHeaderV1 headerV1;
		bool gotHeaderV1 = FJsonObjectConverter::JsonObjectToUStruct<FModumateDocumentHeaderV1>(headerOb->ToSharedRef(), &headerV1);
		if (!gotHeaderV1 || headerV1.Version.Len()==0)
		{
			return false;
		}
		OutHeader.Thumbnail = headerV1.Thumbnail;
		int32 vindex=0;
		if (headerV1.Version.FindLastChar(TCHAR('.'), vindex) && vindex < headerV1.Version.Len()-1)
		{
			OutHeader.Version = FCString::Atoi(*headerV1.Version.Mid(vindex + 1, 1));
		}
		else
		{
			OutHeader.Version = 4;
		}
	}

	// TODO: check version for backwards compatibility with old files
	// Old versions will serialize the version of FMOIDocumentRecord that's in the file and then convert to the latest version
	// If old files ever become completely incompatible we can false out
	if (OutHeader.Version < 4)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> *docOb;

	if (!FileJson->TryGetObjectField(FModumateSerializationStatics::DocObjectInstanceField, docOb))
	{
		return false;
	}

	if (!FJsonObjectConverter::JsonObjectToUStruct<FMOIDocumentRecord>(docOb->ToSharedRef(), &OutRecord))
	{
		return false;
	}

	// All presets saved in a document have been edited at some point
	for (auto& kvp : OutRecord.PresetCollection.PresetsByGUID)
	{
		kvp.Value.bEdited = true;
	}

	// Version 4 -> 5: portal locations and rotations are now relative
	// They were saved in the now-deleted `ObjectInstances`, so there's nothing we can do to restore them now.

	// Version 5 -> 6: Roof was split into RoofFace and RoofPerimeter (EToolMode VE_ROOF -> VE_ROOF_FACE and EObjectType OTRoof -> OTRoofFace),
	// but the serialization change is handled by EnumRedirects in DefaultEngine.ini.
	// The version change is here so we know when to stop supporting the enum redirection,
	// or in case we need to root out nested serialized values (i.e. commands) with text replacement.
	if (OutHeader.Version == 5)
	{
		OutHeader.Version = 6;
	}

	return true;
}

bool FModumateSerializationStatics::SaveDocumentToBuffer(const FModumateDocumentHeader& Header, const FMOIDocumentRecord& Record, TArray<uint8>& OutBuffer)
{
	OutBuffer.Reset();

	FStructSerializerPolicies policies;
	policies.NullValues = EStructSerializerNullValuePolicies::Ignore;

	// Before saving anything to the OutBuffer, first make an uncompressed CBOR buffer of the document record, the large part that can be compressed.
	TArray<uint8> recordUncompressedBuffer;
	FMemoryWriter recordBufferWriter(recordUncompressedBuffer);
	FCborStructSerializerBackend recordSerializerBackend(recordBufferWriter, EStructSerializerBackendFlags::Default);
	FStructSerializer::Serialize(Record, recordSerializerBackend, policies);
	uint32 recordUncompressedSize = recordUncompressedBuffer.Num();
	if (recordUncompressedSize == 0)
	{
		return false;
	}

	// Now, save the actual buffer writing in order
	FMemoryWriter totalBufferWriter(OutBuffer);

	// 1) The version of our binary format (distinct from document version; it only dicates the binary components of the file)
	uint32 binaryDocVersion = CurBinaryDocVersion;
	totalBufferWriter.SerializeIntPacked(binaryDocVersion);

	// 2) The (uncompressed) CBOR representation of the document header
	FCborStructSerializerBackend headerSerializerBackend(totalBufferWriter, EStructSerializerBackendFlags::Default);
	FStructSerializer::Serialize(Header, headerSerializerBackend, policies);

	// 3) The size of our uncompressed CBOR buffer of the document record
	totalBufferWriter.SerializeIntPacked(recordUncompressedSize);

	// 4) The compressed CBOR buffer of the document record
	totalBufferWriter.SerializeCompressed(recordUncompressedBuffer.GetData(), recordUncompressedSize, NAME_Zlib, COMPRESS_BiasSpeed);

	if (!totalBufferWriter.Close())
	{
		return false;
	}

	return (OutBuffer.Num() > 0);
}

bool FModumateSerializationStatics::LoadDocumentFromBuffer(const TArray<uint8>& Buffer, FModumateDocumentHeader& OutHeader, FMOIDocumentRecord& OutRecord, bool bLoadOnlyHeader)
{
	FStructDeserializerPolicies policies;
	policies.MissingFields = EStructDeserializerErrorPolicies::Ignore;

	OutHeader = FModumateDocumentHeader();
	OutRecord = FMOIDocumentRecord();

	FMemoryReader totalBufferReader(Buffer);

	// 1) Verify the version of our binary format
	uint32 savedBinaryDocVersion = 0;
	totalBufferReader.SerializeIntPacked(savedBinaryDocVersion);

	if (savedBinaryDocVersion != CurBinaryDocVersion)
	{
		UE_LOG(LogTemp, Error, TEXT("Binary document was saved with an out of date version: %d"), savedBinaryDocVersion);
		return false;
	}

	// 2) Read the document header
	FCborStructDeserializerBackend headerDeserializerBackend(totalBufferReader);
	if (!FStructDeserializer::Deserialize(OutHeader, headerDeserializerBackend, policies))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to deserialize the document header!"));
		return false;
	}

	if (bLoadOnlyHeader)
	{
		return true;
	}

	// 3) Get the expected size of the uncompressed CBOR buffer of the document record
	uint32 uncompressedRecordSize = 0;
	totalBufferReader.SerializeIntPacked(uncompressedRecordSize);

	if (uncompressedRecordSize == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("The document record size is empty!"));
		return false;
	}

	// 4a) Decompress the CBOR buffer of the document record
	TArray<uint8> recordBuffer;
	recordBuffer.AddZeroed(uncompressedRecordSize);
	totalBufferReader.SerializeCompressed(recordBuffer.GetData(), uncompressedRecordSize, NAME_Zlib, COMPRESS_BiasSpeed);

	// 4b) Deserialize the uncompressed CBOR buffer
	FMemoryReader recordBufferReader(recordBuffer);
	FCborStructDeserializerBackend recordDeserializerBackend(recordBufferReader);

	if (!FStructDeserializer::Deserialize(OutRecord, recordDeserializerBackend, policies))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to deserialize the document record!"));
		return false;
	}

	return true;
}
