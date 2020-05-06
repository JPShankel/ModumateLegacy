// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ModumateObjectEnums.h"
#include "ModumateDecisionTree.h"
#include "ModumateCrafting.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "ModumateCameraView.h"

#include "ModumateSerialization.generated.h"

// Future versions of serialization types should be constructible from previous versions

typedef TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > FPrettyJsonStringWriterFactory;
typedef TJsonWriter< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > FPrettyJsonStringWriter;

// ---------------- Object serialization ----------------


/*
DDL 1.0 preset record, to be deprecated
Future note: keep runtime objects like FCraftingPreset out of serialization because it creates cyclical include dependencies
*/
struct FCraftingPreset;
USTRUCT()
struct MODUMATE_API FModumatePresetRecord_DEPRECATED
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<FCraftingPreset> CraftingPresetArray;
};

/*
DDL 2.0
*/


USTRUCT()
struct FCustomAssemblyProperty
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	int32 LayerID;

	UPROPERTY()
	FString PropertyName;

	UPROPERTY()
	FString PropertyValueJSON;

	FString ToCompactString() const;
};

USTRUCT()
struct MODUMATE_API FCraftingPresetRecord
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FName NodeType;

	UPROPERTY()
	FName PresetID;

	UPROPERTY()
	bool IsReadOnly;

	UPROPERTY()
	TMap<FString,FString> PropertyMap;

	UPROPERTY()
	TArray<int32> ChildNodePinSetIndices;

	UPROPERTY()
	TArray<int32> ChildNodePinSetPositions;

	UPROPERTY()
	TArray<FName> ChildNodePinSetListIDs;

	UPROPERTY()
	TArray<FName> ChildNodePinSetPresetIDs;
};

USTRUCT()
struct FMOIDataRecordV1
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	int32 ID;

	UPROPERTY()
	EObjectType ObjectType;

	UPROPERTY()
	FString AssemblyKey;

	UPROPERTY()
	int32 ParentID;

	UPROPERTY()
	TArray<int32> ChildIDs;

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	FVector UVAnchor;

	UPROPERTY()
	TArray<FVector> ControlPoints;

	UPROPERTY()
	TArray<int32> ControlIndices;

	UPROPERTY()
	FVector Extents;

	UPROPERTY()
	bool ObjectInverted;

	UPROPERTY()
	TMap<FString, FString> ObjectProperties;
};
typedef FMOIDataRecordV1 FMOIDataRecord;


// ---------------- Graph2D serialization ----------------

USTRUCT()
struct FGraph2DEdgeRecord
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<int32> VertexIDs;
};

USTRUCT()
struct FGraph2DPolygonRecord
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<int32> EdgeIDs;
};

USTRUCT()
struct FGraph2DRecord
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TMap<int32, FVector2D> Vertices;

	UPROPERTY()
	TMap<int32, FGraph2DEdgeRecord> Edges;

	UPROPERTY()
	TMap<int32, FGraph2DPolygonRecord> Polygons;
};


// ---------------- Graph3D serialization ----------------

USTRUCT()
struct FGraph3DVertexRecordV1
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	int32 ID;

	UPROPERTY()
	FVector Position;

	// Currently, vertices to not take advantage of group ids
	// if that changes, the data record will need a property for them here
};
typedef FGraph3DVertexRecordV1 FGraph3DVertexRecord;

USTRUCT()
struct FGraph3DEdgeRecordV1
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	int32 ID;

	UPROPERTY()
	int32 StartVertexID;

	UPROPERTY()
	int32 EndVertexID;

	UPROPERTY()
	TSet<int32> GroupIDs;
};
typedef FGraph3DEdgeRecordV1 FGraph3DEdgeRecord;

USTRUCT()
struct FGraph3DFaceRecordV1
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	int32 ID;

	UPROPERTY()
	TArray<int32> VertexIDs;

	UPROPERTY()
	TSet<int32> GroupIDs;
};
typedef FGraph3DFaceRecordV1 FGraph3DFaceRecord;

USTRUCT()
struct FGraph3DRecordV1
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TMap<int32, FGraph3DVertexRecordV1> Vertices;

	UPROPERTY()
	TMap<int32, FGraph3DEdgeRecordV1> Edges;

	UPROPERTY()
	TMap<int32, FGraph3DFaceRecordV1> Faces;
};
typedef FGraph3DRecordV1 FGraph3DRecord;


// ---------------- Assembly serialization ----------------

USTRUCT()
struct FCustomAssemblyRecordV3
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString DatabaseKey;

	UPROPERTY()
	FString CodeName;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	EObjectType ObjectType;

	UPROPERTY()
	EToolMode ToolMode;

	UPROPERTY()
	TMap<FString, FString> Genome;
};

USTRUCT()
struct FCustomAssemblyCraftingNodeRecord
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FName TypeName;

	UPROPERTY()
	FName PresetID;

	UPROPERTY()
	int32 InstanceID;

	UPROPERTY()
	int32 ParentID;

	UPROPERTY()
	TMap<FString, FString> Properties;

	UPROPERTY()
	int32 PinSetIndex;

	UPROPERTY()
	int32 PinSetPosition;
};


USTRUCT()
struct FCustomAssemblyRecordV4
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString DatabaseKey;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	EObjectType ObjectType;

	UPROPERTY()
	EToolMode ToolMode;

	UPROPERTY()
	TArray<FCustomAssemblyProperty> PropertySheet;

	//TODO: CraftingNodes is deprecated in favor of RootPreset
	UPROPERTY()
	TArray<FCustomAssemblyCraftingNodeRecord> CraftingNodes;

	UPROPERTY()
	FName RootPreset;
};
typedef FCustomAssemblyRecordV4 FCustomAssemblyRecord;


// ---------------- Document serialization ----------------

USTRUCT()
struct FMOIDocumentRecordBASE
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<FMOIDataRecordV1> ObjectInstances;

	UPROPERTY()
	FGraph3DRecordV1 VolumeGraph;

	UPROPERTY()
	TArray<FString> CommandHistory;
};

USTRUCT()
struct FMOIDocumentRecordV3 : public FMOIDocumentRecordBASE
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<FCustomAssemblyRecordV3> CustomAssemblies;
};

USTRUCT()
struct FMOIDocumentRecordV4 : public FMOIDocumentRecordBASE
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<FCustomAssemblyRecordV4> CustomAssemblies;

	// DDL 1.0, to be deprecated
	UPROPERTY()
	TArray<FCraftingPreset> CraftingPresetArray;

	UPROPERTY()
	TMap<EToolMode, FName> CurrentToolAssemblyMap;

	// DDL 2.0
	UPROPERTY()
	TArray<FCraftingPresetRecord> CraftingPresetArrayV2;

	UPROPERTY()
	TArray<FName> ProjectAssemblyPresets;

	UPROPERTY()
	TSet<FName> KeyStore;

	UPROPERTY()
	TArray<FModumateCameraView> CameraViews;

	UPROPERTY()
	TMap<FName, FGraph2DRecord> CustomGraph2DRecords;

	void FromVersion3(const FMOIDocumentRecordV3 &v3);
};

typedef FMOIDocumentRecordV4 FMOIDocumentRecord;

USTRUCT()
struct FModumateDocumentHeaderV1
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString Version;

	UPROPERTY()
	FString Thumbnail;
};

USTRUCT()
struct FModumateDocumentHeaderV2
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	int32 Version;

	UPROPERTY()
	FString Thumbnail;
};


typedef FModumateDocumentHeaderV2 FModumateDocumentHeader;

// USTRUCTs above cannot be in a namespace
namespace Modumate
{
	// Version 5: portal locations and rotations are now relative to their parent, rather than in world space.
	// Version 6: VE_ROOF split into VE_ROOF_FACE and VE_ROOF_PERIMETER
	static const int32 DocVersion = 6;

	static const TCHAR * DocObjectInstanceField = TEXT("ModumateObjects");
	static const TCHAR * DocHeaderField = TEXT("ModumateHeader");
	static const TCHAR * CommandHistoryField = TEXT("CommandHistory");
}

struct MODUMATE_API FModumateSerializationStatics
{
	static bool TryReadModumateDocumentRecord(const FString &filePath, FModumateDocumentHeader &OutHeader, FMOIDocumentRecord &OutRecord);
};
