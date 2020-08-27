// Copyright 2020 Modumate, Inc. All Rights Reserved
#pragma once

#include "DocumentManagement/ModumateLayeredObjectInterface.h"

class FModumateObjectInstance;

namespace Modumate
{
	class FGraph3DFace;
	class FGraph3DEdge;
}

// The individual groups of layers that miter together as a unit,
// interpolating their extents for each of the layers within.
enum class EMiterLayerGroup : uint8
{
	PreStructure,
	Structure,
	PostStructure,
	Num
};

struct MODUMATE_API FMiterHitResult
{
	// The distance that the source ray traveled to intersect with the target ray
	float Dist = -FLT_MAX;

	// Whether the source ray intersects with the target ray  at all
	bool bRayHit = false;

	// Whether the source ray intersects with the target participant at its physically extended length
	bool bPhysicalHit = false;
};

// A collection of data required to miter a plane-hosted object for a given edge
struct MODUMATE_API FMiterParticipantData
{
	// Whether all of the data has been successfully initialized
	bool bValid = false;

	// The pointer to the actual plane-hosted object
	const FModumateObjectInstance* MOI = nullptr;

	// The pointer to the actual graph face for this object
	const Modumate::FGraph3DFace* GraphFace = nullptr;

	// The angle of the miter participant, relative to the edge, for sorting
	float MiterAngle = 0.0f;

	// The cached 3D axes for the miter participant
	FVector WorldMiterDir = FVector::ZeroVector;	// the axis facing inward, to the edge along the plane
	FVector WorldNormal = FVector::ZeroVector;	// the normal of the plane

	// The miter participant 3D axes, projected into the miter's 2D axes
	FVector2D MiterDir2D = FVector2D::ZeroVector;
	FVector2D Normal2D = FVector2D::ZeroVector;

	// Whether the participant's plane's normal points CW around the miter edge,
	// for reconciling start/end layer offsets with each other
	bool bPlaneNormalCW = false;

	// The cached distance along the plane's normal from which the rest of the layers are positioned
	float LayerStartOffset = 0.0f;

	// The cached layer dimensions of this plane-hosted object
	FCachedLayerDimsByType LayerDims;

	// For each layer group, whether the plane-hosted object has any layers in it
	bool HaveLayerGroup[EMiterLayerGroup::Num];

	// For each layer group, the 2D-projected origin points of the start and end sides of the layer group, offset from the miter center.
	TPair<FVector2D, FVector2D> LayerGroupOrigins2D[EMiterLayerGroup::Num];

	// For each layer group, if it has been extended in the miter
	bool HaveExtendedLayerGroup[EMiterLayerGroup::Num];

	// For each layer group, the mitered extension values for the start and end of the group
	FVector2D LayerGroupExtensions[EMiterLayerGroup::Num];

	// For each layer, the mitered extension values for the start and of the layer
	TArray<FVector2D> LayerExtensions;

	FMiterParticipantData();
	FMiterParticipantData(const struct FMiterData *InMiterData, int32 EdgeFaceIndex);
	void Reset();
	void GetLayerOffsetIndices(EMiterLayerGroup MiterLayerGroup, int32 &OutLayerStartIdx, int32 &OutLayerEndIdx);
	void FinishLayerGroupExtension(EMiterLayerGroup MiterlayerGroup);

	void IntersectSingle(const FMiterParticipantData &OtherParticipant, bool bUseThisStart, bool bUseOtherStart,
		EMiterLayerGroup ThisLayerGroup, EMiterLayerGroup OtherLayerGroup, FMiterHitResult &OutHitResult) const;
};


struct MODUMATE_API FMiterData
{
	// The pointer to the actual miter edge object
	const FModumateObjectInstance* MOI = nullptr;

	// The pointer to the actual graph edge for this object
	const Modumate::FGraph3DEdge* GraphEdge = nullptr;

	// The position to use as a reference point for the miter (the midpoint of the edge)
	FVector EdgeCenter = FVector::ZeroVector;

	// The direction of the edge itself
	FVector EdgeDir = FVector::ZeroVector;

	// The axes to use for projecting miter participants into 2D for simpler intersections.
	FVector AxisX = FVector::ZeroVector;
	FVector AxisY = FVector::ZeroVector;

	// The miter participant IDs, sorted by angle
	TArray<int32> SortedMiterIDs;

	// The miter participants, stored by ID
	TMap<int32, FMiterParticipantData> ParticipantsByID;

	FMiterData();
	void Reset();

	// Given a miter object, update all of the miter details
	bool GatherDetails(const FModumateObjectInstance *InMiterObject);

	// Once all of the miter details have been gathered, calculate where all of the layers are extended.
	bool CalculateMitering();

	// Given an index in the sorted list of participants and a layer group to extend, extend it.
	bool ExtendLayerGroup(int32 ParticipantIndex, EMiterLayerGroup MiterLayerGroup);
};

class MODUMATE_API IMiterNode
{
public:
	virtual const FMiterData &GetMiterData() const = 0;
};

