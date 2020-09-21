// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MiterNode.h"

#include "Graph/Graph3D.h"
#include "Graph/Graph3DEdge.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Graph/Graph3DTypes.h"
#include "Objects/LayeredObjectInterface.h"
#include "ModumateCore/ModumateMitering.h"
#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateObjectStatics.h"


FMiterParticipantData::FMiterParticipantData()
{
	Reset();
}

FMiterParticipantData::FMiterParticipantData(const struct FMiterData *InMiterData, int32 EdgeFaceIndex)
{
	Reset();

	if (!ensure(InMiterData && InMiterData->MOI && InMiterData->GraphEdge &&
		(EdgeFaceIndex >= 0) && (EdgeFaceIndex < InMiterData->GraphEdge->ConnectedFaces.Num())))
	{
		return;
	}

	const FModumateDocument *doc = InMiterData->MOI->GetDocument();
	if (!ensure(doc))
	{
		return;
	}

	const Modumate::FEdgeFaceConnection &edgeFaceConnection = InMiterData->GraphEdge->ConnectedFaces[EdgeFaceIndex];
	GraphFace = doc->GetVolumeGraph().FindFace(edgeFaceConnection.FaceID);
	if (!ensure(GraphFace))
	{
		return;
	}

	int32 planeObjID = FMath::Abs(edgeFaceConnection.FaceID);
	const FModumateObjectInstance *planeObj = doc->GetObjectById(planeObjID);
	MOI = Modumate::Mitering::FMiterHelpers::GetChildLayeredObj(planeObj);

	// This step is allowed to fail, since we could try to construct a miter participant from a plane without a child layered object.
	if (MOI == nullptr)
	{
		return;
	}

	// Cache all of the data we'll need to quickly evaluate miter collisions and distances.
	MiterAngle = edgeFaceConnection.FaceAngle;
	WorldMiterDir = -edgeFaceConnection.EdgeFaceDir;

	float totalThickness;
	UModumateObjectStatics::GetPlaneHostedValues(MOI, totalThickness, LayerStartOffset, WorldNormal);

	MiterDir2D = UModumateGeometryStatics::ProjectVector2D(WorldMiterDir, InMiterData->AxisX, InMiterData->AxisY);
	Normal2D = UModumateGeometryStatics::ProjectVector2D(WorldNormal, InMiterData->AxisX, InMiterData->AxisY);

	bPlaneNormalCW = ((WorldNormal ^ WorldMiterDir) | InMiterData->EdgeDir) > 0.0f;

	LayerDims = MOI->GetLayeredInterface()->GetCachedLayerDims();
	if (!ensure(LayerDims.NumLayers > 0))
	{
		return;
	}

	for (int32 layerGroupIdx = 0; layerGroupIdx < (int32)EMiterLayerGroup::Num; ++layerGroupIdx)
	{
		EMiterLayerGroup layerGroup = (EMiterLayerGroup)layerGroupIdx;
		int32 layerGroupStartIdx, layerGroupEndIdx;
		GetLayerOffsetIndices(layerGroup, layerGroupStartIdx, layerGroupEndIdx);

		HaveLayerGroup[layerGroupIdx] = (layerGroupStartIdx != layerGroupEndIdx);

		float layerGroupStartOffset = LayerStartOffset + LayerDims.LayerOffsets[layerGroupStartIdx];
		float layerGroupEndOffset = LayerStartOffset + LayerDims.LayerOffsets[layerGroupEndIdx];

		FVector2D origin2D = FVector2D::ZeroVector;
		LayerGroupOrigins2D[layerGroupIdx].Key = origin2D + (layerGroupStartOffset * Normal2D);
		LayerGroupOrigins2D[layerGroupIdx].Value = origin2D + (layerGroupEndOffset * Normal2D);
	}

	LayerExtensions.AddZeroed(LayerDims.NumLayers);

	bValid = true;
}

void FMiterParticipantData::Reset()
{
	bValid = false;
	MOI = nullptr;
	GraphFace = nullptr;
	MiterAngle = 0.0f;
	WorldMiterDir = FVector::ZeroVector;
	WorldNormal = FVector::ZeroVector;
	MiterDir2D = FVector2D::ZeroVector;
	Normal2D = FVector2D::ZeroVector;
	LayerDims = FCachedLayerDimsByType();

	for (int32 layerGroupIdx = 0; layerGroupIdx < (int32)EMiterLayerGroup::Num; ++layerGroupIdx)
	{
		HaveLayerGroup[layerGroupIdx] = false;
		LayerGroupOrigins2D[layerGroupIdx].Key = FVector2D::ZeroVector;
		LayerGroupOrigins2D[layerGroupIdx].Value = FVector2D::ZeroVector;
		HaveExtendedLayerGroup[layerGroupIdx] = false;
		LayerGroupExtensions[layerGroupIdx] = FVector2D::ZeroVector;
	}

	LayerExtensions.Reset();
}

void FMiterParticipantData::GetLayerOffsetIndices(EMiterLayerGroup MiterLayerGroup, int32 &OutLayerStartIdx, int32 &OutLayerEndIdx)
{
	switch (MiterLayerGroup)
	{
	case EMiterLayerGroup::PreStructure:
		OutLayerStartIdx = 0;
		OutLayerEndIdx = LayerDims.StructuralLayerStartIdx;
		break;
	case EMiterLayerGroup::Structure:
		OutLayerStartIdx = LayerDims.StructuralLayerStartIdx;
		OutLayerEndIdx = LayerDims.StructuralLayerEndIdx + 1;
		break;
	case EMiterLayerGroup::PostStructure:
		OutLayerStartIdx = LayerDims.StructuralLayerEndIdx + 1;
		OutLayerEndIdx = LayerDims.LayerOffsets.Num() - 1;
		break;
	}
}

void FMiterParticipantData::FinishLayerGroupExtension(EMiterLayerGroup MiterLayerGroup)
{
	int32 layerGroupIdx = (int32)MiterLayerGroup;
	HaveExtendedLayerGroup[layerGroupIdx] = true;
	const FVector2D &groupExtensions = LayerGroupExtensions[layerGroupIdx];

	int32 layerGroupStartIdx, layerGroupEndIdx;
	GetLayerOffsetIndices(MiterLayerGroup, layerGroupStartIdx, layerGroupEndIdx);

	float groupThickness = (LayerDims.LayerOffsets[layerGroupEndIdx] - LayerDims.LayerOffsets[layerGroupStartIdx]);
	float layerGroupProgress = 0.0f;
	for (int32 layerIdx = layerGroupStartIdx; layerIdx < layerGroupEndIdx; ++layerIdx)
	{
		float layerThickness = LayerDims.LayerOffsets[layerIdx + 1] - LayerDims.LayerOffsets[layerIdx];
		float layerStartAlpha = layerGroupProgress / groupThickness;
		layerGroupProgress += layerThickness;
		float layerEndAlpha = layerGroupProgress / groupThickness;
		LayerExtensions[layerIdx].X = FMath::Lerp(groupExtensions.X, groupExtensions.Y, layerStartAlpha);
		LayerExtensions[layerIdx].Y = FMath::Lerp(groupExtensions.X, groupExtensions.Y, layerEndAlpha);
	}
}

void FMiterParticipantData::IntersectSingle(const FMiterParticipantData &OtherParticipant, bool bUseThisStart, bool bUseOtherStart,
	EMiterLayerGroup ThisLayerGroup, EMiterLayerGroup OtherLayerGroup, FMiterHitResult &OutHitResult) const
{
	int32 thisLayerGroupIdx = (int32)ThisLayerGroup;
	int32 otherLayerGroupIdx = (int32)OtherLayerGroup;

	const auto &thisOriginPair = LayerGroupOrigins2D[thisLayerGroupIdx];
	const FVector2D &thisOrigin = bUseThisStart ? thisOriginPair.Key : thisOriginPair.Value;

	const auto &otherOriginPair = OtherParticipant.LayerGroupOrigins2D[otherLayerGroupIdx];
	const FVector2D &otherOrigin = bUseOtherStart ? otherOriginPair.Key : otherOriginPair.Value;

	const FVector2D &otherExtensions = OtherParticipant.LayerGroupExtensions[otherLayerGroupIdx];
	float otherExtension = bUseOtherStart ? otherExtensions.X : otherExtensions.Y;

	float otherRayDist = 0.0f;
	FVector2D intersection;

	OutHitResult.bRayHit = UModumateGeometryStatics::RayIntersection2D(
		thisOrigin, MiterDir2D, otherOrigin, OtherParticipant.MiterDir2D,
		intersection, OutHitResult.Dist, otherRayDist, false);

	OutHitResult.bPhysicalHit = OutHitResult.bRayHit &&
		OtherParticipant.HaveExtendedLayerGroup[otherLayerGroupIdx] &&
		(otherRayDist <= (otherExtension + RAY_INTERSECT_TOLERANCE));
}


FMiterData::FMiterData()
{
	Reset();
}

void FMiterData::Reset()
{
	// Reset all variables that are cached for each miter geometry update
	// (as opposed to the saved variables that persist between miters, and may need to be overridden and serialized)
	EdgeCenter = FVector::ZeroVector;
	EdgeDir = FVector::ZeroVector;
	AxisX = FVector::ZeroVector;
	AxisY = FVector::ZeroVector;
	SortedMiterIDs.Reset();
	ParticipantsByID.Reset();
}

bool FMiterData::GatherDetails(const FModumateObjectInstance *InMiterObject)
{
	Reset();

	if ((InMiterObject == nullptr) || (InMiterObject->GetObjectType() != EObjectType::OTMetaEdge))
	{
		return false;
	}

	MOI = InMiterObject;
	const FModumateDocument *doc = MOI->GetDocument();
	if (doc == nullptr)
	{
		return false;
	}

	const Modumate::FGraph3D &volumeGraph = doc->GetVolumeGraph();
	GraphEdge = volumeGraph.FindEdge(MOI->ID);
	if (GraphEdge == nullptr)
	{
		return false;
	}

	EdgeCenter = MOI->GetObjectLocation();
	EdgeDir = GraphEdge->CachedDir;
	UModumateGeometryStatics::FindBasisVectors(AxisX, AxisY, EdgeDir);

	// Get all of the plane-hosted objects that are connected to this miter object,
	// and construct helper structs to calculate/cache miter-relevant details
	int32 numConnectedFaces = GraphEdge->ConnectedFaces.Num();

	for (int32 edgeFaceIdx = 0; edgeFaceIdx < numConnectedFaces; ++edgeFaceIdx)
	{
		FMiterParticipantData participant(this, edgeFaceIdx);
		if (!participant.bValid)
		{
			continue;
		}

		int32 participantID = participant.MOI->ID;
		SortedMiterIDs.Add(participantID);
		ParticipantsByID.Add(participantID, MoveTemp(participant));
	}

	int32 numParticipants = ParticipantsByID.Num();

	// Sort the participants by angle
	SortedMiterIDs.Sort([this](const int32 &IDA, const int32 &IDB) {
		return ParticipantsByID[IDA].MiterAngle > ParticipantsByID[IDB].MiterAngle;
	});

	// Return whether we can miter, given the participants for this node
	return (numParticipants >= 2);
}

bool FMiterData::CalculateMitering()
{
	bool bTotalMiterSuccess = false;
	int32 numParticipants = ParticipantsByID.Num();

	// Now, for each object, extend its structural layer group
	for (int32 i = 0; i < numParticipants; ++i)
	{
		bool bMiterSuccess = ExtendLayerGroup(i, EMiterLayerGroup::Structure);
		bTotalMiterSuccess = bTotalMiterSuccess && bMiterSuccess;
	}

	// Now, for each object, extend its non-structural layer groups
	for (int32 i = 0; i < numParticipants; ++i)
	{
		bool bPreMiterSuccess = ExtendLayerGroup(i, EMiterLayerGroup::PreStructure);
		bool bPostMiterSuccess = ExtendLayerGroup(i, EMiterLayerGroup::PostStructure);
		bTotalMiterSuccess = bTotalMiterSuccess && bPreMiterSuccess && bPostMiterSuccess;
	}

	return bTotalMiterSuccess;
}

bool FMiterData::ExtendLayerGroup(int32 ParticipantIndex, EMiterLayerGroup MiterLayerGroup)
{
	int32 layerGroupIdx = (int32)MiterLayerGroup;
	int32 curID = SortedMiterIDs[ParticipantIndex];
	FMiterParticipantData &participant = ParticipantsByID[curID];
	int32 numParticipants = SortedMiterIDs.Num();

	if (!participant.HaveLayerGroup[layerGroupIdx] || (numParticipants < 2))
	{
		return false;
	}

	int32 participantID = participant.MOI->ID;
	const FVector2D &fromDir = participant.MiterDir2D;
	const FVector2D &fromStartOrigin = participant.LayerGroupOrigins2D[layerGroupIdx].Key;
	const FVector2D &fromEndOrigin = participant.LayerGroupOrigins2D[layerGroupIdx].Value;
	bool bExtendingStructure = (MiterLayerGroup == EMiterLayerGroup::Structure);

	bool bFoundLayerGroupExtensions = false;
	FVector2D layerGroupExtensions(ForceInitToZero);

	// Extend the start and end side only as far as they would need to miter with adjacent participants.
	// Determine which direction to iterate to the next and previous miter participants,
	// based on the way the source participant is oriented around the miter edge.
	int32 nextDelta = participant.bPlaneNormalCW ? 1 : -1;
	int32 prevDelta = -nextDelta;

	// Determine which direction to traverse the angularly-sorted list of miter participants,
	// based on which group of layers we're current extending and which side of the group to extend.
	int32 startDelta = 0, endDelta = 0;
	switch (MiterLayerGroup)
	{
	case EMiterLayerGroup::PreStructure:
		startDelta = endDelta = prevDelta;
		break;
	case EMiterLayerGroup::Structure:
		startDelta = prevDelta;
		endDelta = nextDelta;
		break;
	case EMiterLayerGroup::PostStructure:
		startDelta = endDelta = nextDelta;
		break;
	}

	int32 startID = SortedMiterIDs[(ParticipantIndex + numParticipants + startDelta) % numParticipants];
	int32 endID = SortedMiterIDs[(ParticipantIndex + numParticipants + endDelta) % numParticipants];
	FMiterParticipantData &startParticipant = ParticipantsByID[startID];
	FMiterParticipantData &endParticipant = ParticipantsByID[endID];

	// Determine which sides of the "to" layer groups to extend against,
	// based on which way they are oriented around the miter edge.
	bool bSameWindingAsStart = (participant.bPlaneNormalCW == startParticipant.bPlaneNormalCW);
	bool bSameWindingAsEnd = (participant.bPlaneNormalCW == endParticipant.bPlaneNormalCW);
	bool bTargetStartOfStartObj = !bSameWindingAsStart;
	bool bTargetStartOfEndObj = bSameWindingAsEnd;

	EMiterLayerGroup startLayerGroup = EMiterLayerGroup::Structure;
	EMiterLayerGroup endLayerGroup = EMiterLayerGroup::Structure;
	switch (MiterLayerGroup)
	{
	case EMiterLayerGroup::PreStructure:
		startLayerGroup = (bSameWindingAsStart ? EMiterLayerGroup::PostStructure : EMiterLayerGroup::PreStructure);
		endLayerGroup = (bSameWindingAsEnd ? EMiterLayerGroup::PostStructure : EMiterLayerGroup::PreStructure);
		break;
	case EMiterLayerGroup::Structure:
		break;
	case EMiterLayerGroup::PostStructure:
		startLayerGroup = (bSameWindingAsStart ? EMiterLayerGroup::PreStructure : EMiterLayerGroup::PostStructure);
		endLayerGroup = (bSameWindingAsEnd ? EMiterLayerGroup::PreStructure : EMiterLayerGroup::PostStructure);
		break;
	}

	FMiterHitResult startHitResult, endHitResult;
	participant.IntersectSingle(startParticipant, true, bTargetStartOfStartObj,
		MiterLayerGroup, startLayerGroup, startHitResult);
	participant.IntersectSingle(endParticipant, false, bTargetStartOfEndObj,
		MiterLayerGroup, endLayerGroup, endHitResult);

	// Using the collision results for extended sides,
	// try to get actual extension values against the neighboring layer groups
	if (startHitResult.bRayHit && endHitResult.bRayHit)
		{
			layerGroupExtensions.X = startHitResult.Dist;
			layerGroupExtensions.Y = endHitResult.Dist;
			participant.LayerGroupExtensions[layerGroupIdx] = layerGroupExtensions;
			participant.FinishLayerGroupExtension(MiterLayerGroup);
			return true;
		}

		return false;
	}
