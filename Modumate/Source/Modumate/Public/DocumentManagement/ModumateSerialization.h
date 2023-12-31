// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/PrettyJSONWriter.h"

#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "Objects/ModumateObjectEnums.h"
#include "DocumentManagement/DocumentDelta.h"
#include "DocumentManagement/DocumentSettings.h"
#include "DrawingDesigner/DrawingDesignerDocument.h"
#include "JsonObjectConverter.h"
#include "Objects/MOIState.h"
#include "VectorTypes.h"

#include "ModumateSerialization.generated.h"

USTRUCT()
struct FCustomAssemblyProperty
{
	GENERATED_BODY()

	UPROPERTY()
	int32 LayerID = 0;

	UPROPERTY()
	FString PropertyName;

	UPROPERTY()
	FString PropertyValueJSON;

	FString ToCompactString() const;
};


// ---------------- Graph2D serialization ----------------

USTRUCT()
struct FGraph2DEdgeRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> VertexIDs;

	friend bool operator==(const FGraph2DEdgeRecord& Lhs, const FGraph2DEdgeRecord& RHS)
	{
		return Lhs.VertexIDs == RHS.VertexIDs;
	}

	friend bool operator!=(const FGraph2DEdgeRecord& Lhs, const FGraph2DEdgeRecord& RHS)
	{
		return !(Lhs == RHS);
	}
};

USTRUCT()
struct FGraph2DPolygonRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> VertexIDs;

	friend bool operator==(const FGraph2DPolygonRecord& Lhs, const FGraph2DPolygonRecord& RHS)
	{
		return Lhs.VertexIDs == RHS.VertexIDs;
	}

	friend bool operator!=(const FGraph2DPolygonRecord& Lhs, const FGraph2DPolygonRecord& RHS)
	{
		return !(Lhs == RHS);
	}
};

USTRUCT()
struct FGraph2DRecord
{
	GENERATED_BODY()

	UPROPERTY()
	float Epsilon = 0.0f;

	UPROPERTY()
	TMap<int32, FVector2D> Vertices;

	UPROPERTY()
	TMap<int32, FGraph2DEdgeRecord> Edges;

	UPROPERTY()
	TMap<int32, FGraph2DPolygonRecord> Polygons;
	
	friend bool operator==(const FGraph2DRecord& Lhs, const FGraph2DRecord& RHS)
	{
		return Lhs.Epsilon == RHS.Epsilon
			&& Lhs.Vertices.OrderIndependentCompareEqual(RHS.Vertices)
			&& Lhs.Edges.OrderIndependentCompareEqual(RHS.Edges)
			&& Lhs.Polygons.OrderIndependentCompareEqual(RHS.Polygons);
	}

	friend bool operator!=(const FGraph2DRecord& Lhs, const FGraph2DRecord& RHS)
	{
		return !(Lhs == RHS);
	}
};


// ---------------- Graph3D serialization ----------------

USTRUCT()
struct FGraph3DVertexRecordV1
{
	GENERATED_BODY()

	FGraph3DVertexRecordV1();
	FGraph3DVertexRecordV1(int32 InID, const FVector& InPosition);

	UPROPERTY()
	int32 ID;

	UPROPERTY()
	FVec3d Position;

	// Currently, vertices to not take advantage of group ids
	// if that changes, the data record will need a property for them here
	friend bool operator==(const FGraph3DVertexRecordV1& Lhs, const FGraph3DVertexRecordV1& RHS)
	{
		return Lhs.ID == RHS.ID
			&& Lhs.Position == RHS.Position;
	}

	friend bool operator!=(const FGraph3DVertexRecordV1& Lhs, const FGraph3DVertexRecordV1& RHS)
	{
		return !(Lhs == RHS);
	}
};
typedef FGraph3DVertexRecordV1 FGraph3DVertexRecord;

USTRUCT()
struct FGraph3DEdgeRecordV1
{
	GENERATED_BODY()

	FGraph3DEdgeRecordV1();
	FGraph3DEdgeRecordV1(int32 InID, int32 InStartVertexID, int32 InEndVertexID, const TSet<int32>& InGroupIDs);

	UPROPERTY()
	int32 ID;

	UPROPERTY()
	int32 StartVertexID;

	UPROPERTY()
	int32 EndVertexID;

	UPROPERTY()
	TSet<int32> GroupIDs;

	friend bool operator==(const FGraph3DEdgeRecordV1& Lhs, const FGraph3DEdgeRecordV1& RHS)
	{
		return Lhs.ID == RHS.ID
			&& Lhs.StartVertexID == RHS.StartVertexID
			&& Lhs.EndVertexID == RHS.EndVertexID
			&& Lhs.GroupIDs.Difference(RHS.GroupIDs).Num() == 0;
	}

	friend bool operator!=(const FGraph3DEdgeRecordV1& Lhs, const FGraph3DEdgeRecordV1& RHS)
	{
		return !(Lhs == RHS);
	}
};
typedef FGraph3DEdgeRecordV1 FGraph3DEdgeRecord;

USTRUCT()
struct FGraph3DFaceRecordV1
{
	GENERATED_BODY()

	FGraph3DFaceRecordV1();
	FGraph3DFaceRecordV1(int32 InID, const TArray<int32>& InVertexIDs, const TSet<int32>& InGroupIDs, int32 InContainingFaceID, const TSet<int32>& InContainedFaceIDs);

	UPROPERTY()
	int32 ID;

	UPROPERTY()
	TArray<int32> VertexIDs;

	UPROPERTY()
	TSet<int32> GroupIDs;

	UPROPERTY()
	int32 ContainingFaceID;

	UPROPERTY()
	TSet<int32> ContainedFaceIDs;
	
	friend bool operator==(const FGraph3DFaceRecordV1& Lhs, const FGraph3DFaceRecordV1& RHS)
	{
		return Lhs.ID == RHS.ID
			&& Lhs.VertexIDs == RHS.VertexIDs
			&& Lhs.GroupIDs.Difference(RHS.GroupIDs).Num() == 0
			&& Lhs.ContainingFaceID == RHS.ContainingFaceID
			&& Lhs.ContainedFaceIDs.Difference(RHS.ContainedFaceIDs).Num() == 0;
	}

	friend bool operator!=(const FGraph3DFaceRecordV1& Lhs, const FGraph3DFaceRecordV1& RHS)
	{
		return !(Lhs == RHS);
	}
};
typedef FGraph3DFaceRecordV1 FGraph3DFaceRecord;

USTRUCT()
struct FGraph3DRecordV1
{
	friend bool operator==(const FGraph3DRecordV1& Lhs, const FGraph3DRecordV1& RHS)
	{
		return Lhs.Vertices.OrderIndependentCompareEqual(RHS.Vertices)
			&& Lhs.Edges.OrderIndependentCompareEqual(RHS.Edges)
			&& Lhs.Faces.OrderIndependentCompareEqual(RHS.Faces);
	}

	friend bool operator!=(const FGraph3DRecordV1& Lhs, const FGraph3DRecordV1& RHS)
	{
		return !(Lhs == RHS);
	}

	GENERATED_BODY()

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
	GENERATED_BODY()

	UPROPERTY()
	FString CodeName;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	EObjectType ObjectType= EObjectType::OTNone;

	UPROPERTY()
	EToolMode ToolMode= EToolMode::VE_NONE;

	UPROPERTY()
	TMap<FString, FString> Genome;
};


USTRUCT()
struct FCustomAssemblyRecordV4
{
	GENERATED_BODY()

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	EObjectType ObjectType= EObjectType::OTNone;

	UPROPERTY()
	EToolMode ToolMode= EToolMode::VE_NONE;

	UPROPERTY()
	TArray<FCustomAssemblyProperty> PropertySheet;
};
typedef FCustomAssemblyRecordV4 FCustomAssemblyRecord;


// ---------------- Document serialization ----------------

USTRUCT()
struct FMOIDocumentRecordBASE
{
	GENERATED_BODY()

	UPROPERTY()
	FGraph3DRecordV1 VolumeGraph;

};

USTRUCT()
struct FMOIDocumentRecordV3 : public FMOIDocumentRecordBASE
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCustomAssemblyRecordV3> CustomAssemblies;
};

USTRUCT()
struct FMOIDocumentRecordV4 : public FMOIDocumentRecordBASE
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOIStateData> ObjectData;

	UPROPERTY()
	TMap<EToolMode, FGuid> CurrentToolAssemblyGUIDMap;

	UPROPERTY()
	FBIMPresetCollection PresetCollection;

	UPROPERTY()
	TMap<FString, FGraph2DRecord> CustomGraph2DRecords;

	UPROPERTY()
	TMap<int32, FGraph2DRecord> SurfaceGraphs;

	UPROPERTY()
	TArray<FDeltasRecord> AppliedDeltas;

	UPROPERTY()
	FDocumentSettings Settings;

	UPROPERTY()
	TMap<uint32, FGuid> TypicalEdgeDetails;

	UPROPERTY()
	FDrawingDesignerDocument DrawingDesignerDocument;

	void FromVersion3(const FMOIDocumentRecordV3 &v3);
};

USTRUCT()
struct FMOIDocumentRecordV5 : public FMOIDocumentRecordBASE
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMOIStateData> ObjectData;

	UPROPERTY()
	TMap<EToolMode, FGuid> CurrentToolAssemblyGUIDMap;

	UPROPERTY()
	FBIMPresetCollection PresetCollection;

	UPROPERTY()
	TMap<FString, FGraph2DRecord> CustomGraph2DRecords;

	UPROPERTY()
	TMap<int32, FGraph2DRecord> SurfaceGraphs;

	UPROPERTY()
	TArray<FDeltasRecord> AppliedDeltas;

	UPROPERTY()
	FDocumentSettings Settings;

	UPROPERTY()
	TMap<uint32, FGuid> TypicalEdgeDetails;

	UPROPERTY()
	FDrawingDesignerDocument DrawingDesignerDocument;

	UPROPERTY()
	int32 RootVolumeGraph = 0;

	UPROPERTY()
	int32 RootDesignOptionID = 0;

	UPROPERTY()
	TMap<int32, FGraph3DRecordV1> VolumeGraphs;
};

using FMOIDocumentRecord = FMOIDocumentRecordV5;

USTRUCT()
struct FModumateDocumentHeaderV1
{
	GENERATED_BODY()

	UPROPERTY()
	FString Version;

	UPROPERTY()
	FString Thumbnail;
};

USTRUCT()
struct FModumateDocumentHeaderV2
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = 0;

	UPROPERTY()
	uint32 DocumentHash = 0;

	UPROPERTY()
	FString Thumbnail;
};

using FModumateDocumentHeader = FModumateDocumentHeaderV2;

// Version 5: portal locations and rotations are now relative to their parent, rather than in world space.
// Version 6: Roof was split into RoofFace and RoofPerimeter (EToolMode VE_ROOF -> VE_ROOF_FACE and EObjectType OTRoof -> OTRoofFace)
// Version 7: Preset structure change, throw out presets and preserve graph for old file load
// Version 8: Empty FName keys serialized as "None", BIMKeys as empty string
// Version 9: Object state data serialized directly, deprecated FMOIDataRecord
// Version 10: BIM Taxonomy present in serialized presets
// Version 11: Fix lost material bindings
// Version 12: Strip custom preset NCPs of spaces
// Version 13: BIM patterns converted from child pins to properties
// Version 14: Linear extrusion width/depth values were swapped, and effectively only canceled out for Trim, so Mullion/Beam/Column dimensions need to be swapped
// Version 15: Support for multiple custom data entries in presets
// Version 16: Miter priority in layers
// Version 17: Typical edge details
// Version 18: Multiple volume graphs
// Version 19: Editable part sizes in BIM
// Version 20: Mark and Comment properties in all presets
// Version 21: Edge and plane hosted objects have spans for parents
// Version 22: Portals use MetaPlaneSpan
// Version 23: Truncated Preset Marketplace
// Version 24: Canonical Presets and their derivations. Cloud based importing.
// Version 25: Fix missing pin sets
// Version 26: Preset Custom Data upgrade, deprecating preset properties
// Version 27: Symbol Presets have multiple 3d graphs
// Version 28: VDPTable is 1:* instead of 1:1
static constexpr int32 DocVersion = 28;

struct MODUMATE_API FModumateSerializationStatics
{
	static const FString DocHeaderField;
	static const FString DocObjectInstanceField;

	// Binary doc version 2: standards-compliant endianness in CBOR
	static constexpr uint32 CurBinaryDocVersion = 2;

	static bool TryReadModumateDocumentRecord(const FString &FilePath, FModumateDocumentHeader &OutHeader, FMOIDocumentRecord &OutRecord);
	static bool SaveDocumentToBuffer(const FModumateDocumentHeader& Header, const FMOIDocumentRecord& Record, TArray<uint8>& OutBuffer);
	static bool LoadDocumentFromBuffer(const TArray<uint8>& Buffer, FModumateDocumentHeader& OutHeader, FMOIDocumentRecord& OutRecord, bool bLoadOnlyHeader = false);
};
