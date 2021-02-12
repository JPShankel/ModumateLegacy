// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateSerialization.h"

FGraph3DVertexRecordV1::FGraph3DVertexRecordV1()
	: ID(MOD_ID_NONE)
	, Position(ForceInitToZero)
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
	if (!FileJson->TryGetObjectField(Modumate::DocHeaderField, headerOb))
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

	if (!FileJson->TryGetObjectField(Modumate::DocObjectInstanceField, docOb))
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
		kvp.Value.Edited = true;
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
