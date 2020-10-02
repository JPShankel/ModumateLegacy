// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateSerialization.h"
#include "DocumentManagement/ModumatePresetManager.h"

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
	ObjectInstances = v3.ObjectInstances;
	CommandHistory = v3.CommandHistory;

	for (auto &ca3 : v3.CustomAssemblies)
	{
		FCustomAssemblyRecordV4 ca4;
		ca4.DatabaseKey = ca3.DatabaseKey;
		ca4.DisplayName = ca3.DisplayName;
		ca4.ObjectType = ca3.ObjectType;
		ca4.ToolMode = ca3.ToolMode;
		CustomAssemblies.Add(ca4);
	}
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

	// Version 4 -> 5: portal locations and rotations are now relative, so just delete them for now.
	if (OutHeader.Version == 4)
	{
		bool bRemovedAnyObjs = false;
		TSet<int32> removedObjIDs;

		do
		{
			bRemovedAnyObjs = false;
			int32 numObjs = OutRecord.ObjectInstances.Num();
			for (int32 i = numObjs - 1; i >= 0; --i)
			{
				const FMOIDataRecord_DEPRECATED& objRecord = OutRecord.ObjectInstances[i];

				if (removedObjIDs.Contains(objRecord.ParentID) ||
					(objRecord.ObjectType == EObjectType::OTDoor) ||
					(objRecord.ObjectType == EObjectType::OTWindow))
				{
					bRemovedAnyObjs = true;
					removedObjIDs.Add(objRecord.ID);
					OutRecord.ObjectInstances.RemoveAt(i, 1, false);
				}
			}
		} while (bRemovedAnyObjs);

		OutHeader.Version = 5;
	}

	// Version 5 -> 6: Roof was split into RoofFace and RoofPerimeter (EToolMode VE_ROOF -> VE_ROOF_FACE and EObjectType OTRoof -> OTRoofFace),
	// but the serialization change is handled by EnumRedirects in DefaultEngine.ini.
	// The version change is here so we know when to stop supporting the enum redirection,
	// or in case we need to root out nested serialized values (i.e. commands) with text replacement.
	if (OutHeader.Version == 5)
	{
		OutHeader.Version = 6;
	}

	//If the preset manager is out of date, load a new one and throw out MOI types that depend on presets (leave graph)
	if (OutHeader.Version < FPresetManager::MinimumReadableVersion)
	{
		TArray<EObjectType> whiteList = {
			EObjectType::OTMetaVertex,
			EObjectType::OTMetaEdge,
			EObjectType::OTMetaPlane,
			EObjectType::OTCutPlane,
			EObjectType::OTScopeBox,
			EObjectType::OTDrawing,
			EObjectType::OTRoom };

		OutRecord.ObjectInstances = OutRecord.ObjectInstances.FilterByPredicate(
			[&whiteList](const FMOIDataRecord_DEPRECATED &Record)
		{
			return whiteList.Contains(Record.ObjectType);
		});
	}

	if (OutHeader.Version < 8) // Empty FName keys serialized as "None", BIMKeys as empty string
	{
		const FBIMKey badNone = FBIMKey(TEXT("None"));
		for (auto& objInst : OutRecord.ObjectInstances)
		{
			if (objInst.AssemblyKey == badNone)
			{
				objInst.AssemblyKey = FBIMKey();
			}
		}
		for (auto& kvp : OutRecord.CurrentToolAssemblyMap)
		{
			if (kvp.Value == badNone)
			{
				kvp.Value = FBIMKey();
			}
		}
	}

	return true;
}
