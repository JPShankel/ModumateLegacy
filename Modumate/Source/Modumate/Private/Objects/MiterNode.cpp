// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MiterNode.h"

#include "Graph/Graph3D.h"
#include "Graph/Graph3DEdge.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/EdgeDetailData.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Graph/Graph3DTypes.h"
#include "Objects/EdgeDetailObj.h"
#include "Objects/LayeredObjectInterface.h"
#include "Objects/MetaEdge.h"
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

	if (!ensure(InMiterData && InMiterData->MOI.IsValid() && InMiterData->GraphEdge &&
		(EdgeFaceIndex >= 0) && (EdgeFaceIndex < InMiterData->GraphEdge->ConnectedFaces.Num())))
	{
		return;
	}

	const UModumateDocument* doc = InMiterData->MOI->GetDocument();
	if (!ensure(doc))
	{
		return;
	}

	const FEdgeFaceConnection &edgeFaceConnection = InMiterData->GraphEdge->ConnectedFaces[EdgeFaceIndex];
	GraphFace = doc->GetVolumeGraph().FindFace(edgeFaceConnection.FaceID);
	if (!ensure(GraphFace))
	{
		return;
	}

	int32 planeObjID = FMath::Abs(edgeFaceConnection.FaceID);
	const AModumateObjectInstance *planeObj = doc->GetObjectById(planeObjID);
	MOI = FMiterHelpers::GetChildLayeredObj(planeObj);

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

	// All structural layers plus pre and post structure
	GroupOrigins2D.SetNum(LayerDims.LayerGroups.Num()+2);
	GroupExtensions.SetNumZeroed(LayerDims.LayerGroups.Num());
	
	LayerExtensions.SetNumZeroed(LayerDims.NumLayers);
	SurfaceExtensions = FVector2D::ZeroVector;

	// Pre-structure origin
	float layerGroupStartOffset = LayerStartOffset + LayerDims.LayerOffsets[0];
	float layerGroupEndOffset = LayerStartOffset + LayerDims.LayerOffsets[0];
	GroupOrigins2D[0].Key = (layerGroupStartOffset * Normal2D);
	GroupOrigins2D[0].Value = (layerGroupEndOffset * Normal2D);

	// Post-structure origin
	layerGroupStartOffset = LayerStartOffset + LayerDims.LayerOffsets[LayerDims.NumLayers];
	layerGroupEndOffset = LayerStartOffset + LayerDims.LayerOffsets[LayerDims.NumLayers];
	GroupOrigins2D.Last().Key = (layerGroupStartOffset * Normal2D);
	GroupOrigins2D.Last().Value = (layerGroupEndOffset * Normal2D);

	// Structural origins
	for (int32 groupIdx = 0; groupIdx < LayerDims.LayerGroups.Num(); ++groupIdx)
	{
		const auto& group = LayerDims.LayerGroups[groupIdx];

		layerGroupStartOffset = LayerStartOffset + LayerDims.LayerOffsets[group.StartIndex];
		layerGroupEndOffset = LayerStartOffset + LayerDims.LayerOffsets[group.EndIndex+1];

		GroupOrigins2D[groupIdx + 1].Key = (layerGroupStartOffset * Normal2D);
		GroupOrigins2D[groupIdx + 1].Value = (layerGroupEndOffset * Normal2D);
	}

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

	LayerExtensions.Reset();
	GroupExtensions.Reset();
	SurfaceExtensions = FVector2D::ZeroVector;
}

void FMiterParticipantData::IntersectStructureGroup(const FMiterLayerGroup& LayerGroup, const FMiterParticipantData& OtherParticipant, bool bUseThisStart, bool bUseOtherStart, FMiterHitResult& OutHitResult) const
{
	int32 numOtherLayers = OtherParticipant.LayerDims.LayerGroups.Num();

	const auto& thisOriginPair = GroupOrigins2D[LayerGroup.GroupIdx + 1];
	FVector2D thisOrigin = bUseThisStart ? thisOriginPair.Key : thisOriginPair.Value;

	EMiterLayerGroupPreferredNeighbor targetNeighborDelta = bUseThisStart == bUseOtherStart ? LayerGroup.PreferredNeighbor : MiterLayerGroupOtherNeighbor(LayerGroup.PreferredNeighbor);

	int32 bestGroup = INDEX_NONE;
	OutHitResult.bPriorityMatchHit = false;

	/*
	* If this is not the lone instance of the highest priority layer, we look for a viable target in the other participant
	*/
	if (!LayerGroup.bIsLoneTopPriorityLayer)
	{
		for (int32 i = 0; i < numOtherLayers; ++i)
		{
			int32 otherGroupIdx = targetNeighborDelta == EMiterLayerGroupPreferredNeighbor::Previous ? i : numOtherLayers - i - 1;

			// If I have no designated target side, or this layer group is on my designated side or this layer group is structual, I can miter
			bool bCanMiterWith = targetNeighborDelta == EMiterLayerGroupPreferredNeighbor::Both ||
				targetNeighborDelta == OtherParticipant.LayerDims.LayerGroups[otherGroupIdx].PreferredNeighbor ||
				OtherParticipant.LayerDims.LayerGroups[otherGroupIdx].PreferredNeighbor == EMiterLayerGroupPreferredNeighbor::Both;

			if (!bCanMiterWith)
			{
				continue;
			}

			// If we find an exact match, prioritize that
			if (OtherParticipant.LayerDims.LayerGroups[otherGroupIdx].Priority == LayerGroup.Priority)
			{
				bestGroup = otherGroupIdx;
				OutHitResult.bPriorityMatchHit = true;
				break;
			}

			// Accept the first higher priority layer, but keep looking for an exact match
			if (bestGroup == INDEX_NONE && OtherParticipant.LayerDims.LayerGroups[otherGroupIdx].Priority < LayerGroup.Priority)
			{
				bestGroup = otherGroupIdx;
			}
		}
	}
	/*
	* If this is the lone instance of the highest priority layer, it won't have any collision targets, so extend this layer to the
	* opposite side of the highest priority layer in the other participant
	*/
	else
	{
		bestGroup = 0;
		auto topPriority = OtherParticipant.LayerDims.LayerGroups[0].Priority;
		for (int32 i = 0; i < numOtherLayers; ++i)
		{
			if (OtherParticipant.LayerDims.LayerGroups[i].Priority < topPriority)
			{
				topPriority = OtherParticipant.LayerDims.LayerGroups[i].Priority;
				bestGroup = i;
			}
		}
		bUseOtherStart = !bUseOtherStart;
	}

	if (bestGroup == INDEX_NONE)
	{
		return;
	}

	int32 otherGroupIdx = bestGroup;

	const auto& otherOriginPair = OtherParticipant.GroupOrigins2D[otherGroupIdx + 1];
	FVector2D otherOrigin;

	// If we hit a matching layer, pick origin to create angled miter
	if (OutHitResult.bPriorityMatchHit)
	{
		otherOrigin = bUseOtherStart ? otherOriginPair.Key : otherOriginPair.Value;
	}
	else
	{
		// Otherwise, if we're targeting a central structural layer, pick the side passed in
		if (targetNeighborDelta == EMiterLayerGroupPreferredNeighbor::Both)
		{
			otherOrigin = bUseOtherStart ? otherOriginPair.Key : otherOriginPair.Value;
		}
		// ..or if it's a low priority layer, pick the side of the structural layer we're targeting
		else
		{
			otherOrigin = targetNeighborDelta == EMiterLayerGroupPreferredNeighbor::Previous ? otherOriginPair.Key : otherOriginPair.Value;
		}
	}

	FVector2D intersection;
	float thisRayDist,otherRayDist;

	if (UModumateGeometryStatics::RayIntersection2D(
		thisOrigin, MiterDir2D, otherOrigin, OtherParticipant.MiterDir2D,
		intersection, thisRayDist, otherRayDist, false))
	{
		OutHitResult.Dist = thisRayDist;
		OutHitResult.bRayHit = true;
	}
}

void FMiterParticipantData::IntersectSurfaceGroup(int32 ThisSurfaceIdx, int32 OtherSurfaceIdx, const FMiterParticipantData& OtherParticipant, bool bUseThisStart, bool bUseOtherStart, FMiterHitResult& OutHitResult) const
{
	TPair<FVector2D, FVector2D> thisOriginPair, otherOriginPair;
	FVector2D thisOrigin, otherOrigin;

	thisOriginPair = GroupOrigins2D[ThisSurfaceIdx];
	thisOrigin = bUseThisStart ? thisOriginPair.Key : thisOriginPair.Value;

	otherOriginPair = OtherParticipant.GroupOrigins2D[OtherSurfaceIdx];
	otherOrigin = bUseOtherStart ? otherOriginPair.Key : otherOriginPair.Value;

	float otherRayDist = 0.0f;
	FVector2D intersection;

	if (UModumateGeometryStatics::RayIntersection2D(
		thisOrigin, MiterDir2D, otherOrigin, OtherParticipant.MiterDir2D,
		intersection, OutHitResult.Dist, otherRayDist, false))
	{
		OutHitResult.bRayHit = true;
	}
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
	SortedParticipantIDs.Reset();
	ParticipantsByID.Reset();
}

bool FMiterData::GatherDetails(const AModumateObjectInstance *InMiterObject)
{
	Reset();

	if ((InMiterObject == nullptr) || (InMiterObject->GetObjectType() != EObjectType::OTMetaEdge))
	{
		return false;
	}

	MOI = InMiterObject;
	const UModumateDocument* doc = MOI->GetDocument();
	if (doc == nullptr)
	{
		return false;
	}

	const FGraph3D &volumeGraph = doc->GetVolumeGraph();
	GraphEdge = volumeGraph.FindEdge(MOI->ID);
	if (GraphEdge == nullptr)
	{
		return false;
	}

	EdgeCenter = MOI->GetLocation();
	EdgeDir = GraphEdge->CachedDir;
	UModumateGeometryStatics::FindBasisVectors(AxisX, AxisY, EdgeDir);

	// Get all of the plane-hosted objects that are connected to this miter object,
	// and construct helper structs to calculate/cache miter-relevant details
	int32 numConnectedFaces = GraphEdge->ConnectedFaces.Num();


	// If we only find one participant having the highest priority layer, we'll need to mark that layer for special extension
	int32 topPriorityCount = 0;
	FBIMPresetLayerPriority topPriority;
	int32 extended180ID = INDEX_NONE;

	for (int32 edgeFaceIdx = 0; edgeFaceIdx < numConnectedFaces; ++edgeFaceIdx)
	{
		FMiterParticipantData participant(this, edgeFaceIdx);
		if (!participant.bValid)
		{
			continue;
		}

		if (topPriorityCount == 0)
		{
			topPriority = participant.LayerDims.LayerGroups[0].Priority;
			topPriorityCount = 1;
			extended180ID = participant.MOI->ID;
		}

		for (int32 i = 0; i < participant.LayerDims.LayerGroups.Num(); ++i)
		{
			if (participant.LayerDims.LayerGroups[i].Priority < topPriority)
			{
				topPriorityCount = 1;
				topPriority = participant.LayerDims.LayerGroups[i].Priority;
				extended180ID = participant.MOI->ID;
			}
			else if (participant.LayerDims.LayerGroups[i].Priority == topPriority && extended180ID != INDEX_NONE && extended180ID != participant.MOI->ID)
			{
				++topPriorityCount;
			}
		}

		int32 participantID = participant.MOI->ID;
		SortedParticipantIDs.Add(participantID);
		ParticipantsByID.Add(participantID, MoveTemp(participant));
	}

	/*
	 * If we found one and only one participant with the highest priority layer,
	 * mark each instance of that layer type as being a "lone top priority layer."
	 * Lone top priority layers have no miter targets and so extend themselves 
	 * to the opposite side of their neighbors
	 * */

	if (topPriorityCount == 1 && numConnectedFaces > 1 && ensure(extended180ID != INDEX_NONE))
	{
		auto& participant = ParticipantsByID[extended180ID];
		for (auto& layer : participant.LayerDims.LayerGroups)
		{
			if (layer.Priority == topPriority)
			{
				layer.bIsLoneTopPriorityLayer = true;
			}
		}

		/*
		* In addition to marking the original participant's layers as top priority,
		* we must also create a twin virtual layer 180 degrees around so other layers
		* will see this participant as a candidate for extending both sides
		* 
		* TODO: this is a hack that requires us to use the negative MOI ID to store the
		* virtual participant. This is to be refactored so each participant can consider
		* every other participant instead of just a "preferred" neighbor
		*/
		auto newParticipant = participant;

		newParticipant.MiterAngle += 180.0;
		if (newParticipant.MiterAngle > 360)
		{
			newParticipant.MiterAngle -= 360;
		}
		newParticipant.bPlaneNormalCW = !newParticipant.bPlaneNormalCW;
		SortedParticipantIDs.Add(-newParticipant.MOI->ID);
		ParticipantsByID.Add(-newParticipant.MOI->ID, newParticipant);
	}

	// Sort the participants by angle
	SortedParticipantIDs.Sort([this](const int32 &IDA, const int32 &IDB) {
		return ParticipantsByID[IDA].MiterAngle < ParticipantsByID[IDB].MiterAngle;
	});

	// Return whether we gathered all of the necessary data for mitering
	return true;
}

bool FMiterData::CalculateMitering()
{
	int32 numParticipants = ParticipantsByID.Num();

	auto metaEdge = Cast<AMOIMetaEdge>(MOI.Get());
	auto edgeDetailMOI = metaEdge ? metaEdge->CachedEdgeDetailMOI : nullptr;
	const FEdgeDetailData* edgeDetailData = edgeDetailMOI ? &edgeDetailMOI->GetAssembly().EdgeDetailData : nullptr;
	int32 numDetailParticipants = edgeDetailData ? edgeDetailData->Overrides.Num() : 0;

	// If we have an edge detail, we don't need the mirrored top-priority layer used by auto mitering
	if (edgeDetailData)
	{
		ParticipantsByID = ParticipantsByID.FilterByPredicate([](const TPair<int32, FMiterParticipantData>& Participant) {return Participant.Key > 0; });
		SortedParticipantIDs = SortedParticipantIDs.FilterByPredicate([](int32 ID) {return ID > 0; });
	}

	// If there is an edge detail that overrides layer extension data for this miter node,
	// then let it populate the data rather than our miter algorithm.
	if (edgeDetailData && ensureMsgf(numParticipants == numDetailParticipants,
		TEXT("Cannot apply a miter detail with %d participants to an edge that has %d participants!"),
		numDetailParticipants, numParticipants))
	{
		FEdgeDetailData orientedDetailData = *edgeDetailData;
		orientedDetailData.OrientData(edgeDetailMOI->InstanceData.OrientationIndex);

		for (int32 participantIdx = 0; participantIdx < numParticipants; ++participantIdx)
		{
			int32 participantID = SortedParticipantIDs[participantIdx];
			if (!ensure(ParticipantsByID.Contains(participantID)))
			{
				return false;
			}

			FMiterParticipantData& miterParticipant = ParticipantsByID[participantID];
			int32 numParticipantLayers = miterParticipant.LayerExtensions.Num();

			const FEdgeDetailOverrides& edgeDetailOverrides = orientedDetailData.Overrides[participantIdx];
			int32 numDetailOverrideLayers = edgeDetailOverrides.LayerExtensions.Num();

			if (!ensureMsgf(numParticipantLayers == numDetailOverrideLayers,
				TEXT("Cannot apply miter detail extensions at index %d with %d layer extensions to participant with %d layers!"),
				participantIdx, numDetailOverrideLayers, numParticipantLayers))
			{
				return false;
			}

			// Edge detail overrides are normalized as if all participants are coming in with CW orientation,
			// so if the detail is applying to a miter participant that's CCW, we need to adjust the extensions.
			miterParticipant.LayerExtensions = edgeDetailOverrides.LayerExtensions;
			miterParticipant.SurfaceExtensions = edgeDetailOverrides.SurfaceExtensions;
			if (!miterParticipant.bPlaneNormalCW)
			{
				Algo::Reverse(miterParticipant.LayerExtensions);
				for (FVector2D& layerExtension : miterParticipant.LayerExtensions)
				{
					Swap(layerExtension.X, layerExtension.Y);
				}

				Swap(miterParticipant.SurfaceExtensions.X, miterParticipant.SurfaceExtensions.Y);
			}
		}

		return true;
	}

	// Gather all layer groups from all participants and sort by priority
	TArray<FMiterLayerGroup> SortedParticipantGroups;
	for (int32 partIdx = 0; partIdx < numParticipants; ++partIdx)
	{
		FMiterParticipantData& participant = ParticipantsByID[SortedParticipantIDs[partIdx]];
		participant.ExtendedGroupIndices.Empty();
		for (int32 groupIdx=0;groupIdx<participant.LayerDims.LayerGroups.Num();++groupIdx)
		{
			auto& layerGroup = participant.LayerDims.LayerGroups[groupIdx];
			layerGroup.ParticipantIdx = partIdx;
			layerGroup.GroupIdx = groupIdx;
			SortedParticipantGroups.Add(layerGroup);
		}
	}

	SortedParticipantGroups.Sort([](const FMiterLayerGroup& LHS, const FMiterLayerGroup& RHS) {
		return LHS.Priority < RHS.Priority;
	});

	// Now, extend each structural layer group from participants
	bool bTotalMiterSuccess = true;
	for (auto& participantGroup : SortedParticipantGroups)
	{
		bTotalMiterSuccess = ExtendLayerGroup(participantGroup) && bTotalMiterSuccess;
	}

	// Now, for each object, extend its surface groups
	FMiterLayerGroup prePostGroup;
	for (int32 i = 0; i < numParticipants; ++i)
	{
		bTotalMiterSuccess = ExtendSurfaceGroups(i) && bTotalMiterSuccess;
	}

	return bTotalMiterSuccess;
}

FMiterParticipantData* FMiterData::GetParticipantBySortedIDIndex(int32 ParticipantIdx)
{
	// If the participant is missing a particular layer group, then we report success without doing any work.
	if (ParticipantIdx >= SortedParticipantIDs.Num() || SortedParticipantIDs.Num() < 2)
	{
		return nullptr;
	}
	return ParticipantsByID.Find(SortedParticipantIDs[ParticipantIdx]);
}

bool FMiterData::ExtendSurfaceGroups(int32 ParticipantIdx)
{
	FMiterParticipantData* participant = GetParticipantBySortedIDIndex(ParticipantIdx);

	if (participant == nullptr)
	{
		return false;
	}

	int32 nextDelta, prevDelta;
	participant->GetNeighboringParticipantDeltas(nextDelta, prevDelta);

	FMiterParticipantData* nextNeighbor, *prevNeighbor;
	if (!ensureAlways(GetNeighboringParticipants(ParticipantIdx, nextDelta, nextNeighbor, prevDelta, prevNeighbor)))
	{
		return false;
	}

	bool bSameWindingAsNext = (nextNeighbor->bPlaneNormalCW == participant->bPlaneNormalCW);
	bool bSameWindingAsPrev = (prevNeighbor->bPlaneNormalCW == participant->bPlaneNormalCW);

	int32 prevNeighborOriginIdx = INDEX_NONE, nextNeighborOriginIdx= INDEX_NONE;
	prevNeighborOriginIdx = bSameWindingAsPrev ? prevNeighbor->GroupOrigins2D.Num() - 1 : 0;
	nextNeighborOriginIdx = bSameWindingAsNext ? 0 : nextNeighbor->GroupOrigins2D.Num() - 1;
	
	FMiterHitResult prevHitResult,nextHitResult;
	participant->IntersectSurfaceGroup(0, prevNeighborOriginIdx, *prevNeighbor, true, !bSameWindingAsPrev, prevHitResult);
	participant->IntersectSurfaceGroup(participant->GroupOrigins2D.Num()-1, nextNeighborOriginIdx, *nextNeighbor, false, bSameWindingAsNext, nextHitResult);

	if (prevHitResult.bRayHit && nextHitResult.bRayHit)
	{
		participant->SurfaceExtensions.X = prevHitResult.Dist;
		participant->SurfaceExtensions.Y = nextHitResult.Dist;
		return true;
	}
	return false;
}

void FMiterParticipantData::GetNeighboringParticipantDeltas(int32& OutNextNeighbor, int32& OutPreviousNeighbor) const
{
	OutNextNeighbor = bPlaneNormalCW ? -1 : 1;
	OutPreviousNeighbor = -OutNextNeighbor;
}

bool FMiterData::GetNeighboringParticipants(int32 ParticipantIdx, int32 NextDelta, FMiterParticipantData*& NextNeighbor, int32 PrevDelta, FMiterParticipantData*& PrevNeighbor)
{
	int32 numParticipants = SortedParticipantIDs.Num();

	int32 nextID = SortedParticipantIDs[(ParticipantIdx + numParticipants + NextDelta) % numParticipants];
	int32 prevID = SortedParticipantIDs[(ParticipantIdx + numParticipants + PrevDelta) % numParticipants];

	NextNeighbor = ParticipantsByID.Find(nextID);
	PrevNeighbor = ParticipantsByID.Find(prevID);

	return true;
}

bool FMiterData::ExtendLayerGroup(const FMiterLayerGroup& LayerGroup)
{
	FMiterParticipantData* participant = GetParticipantBySortedIDIndex(LayerGroup.ParticipantIdx);

	if (participant == nullptr)
	{
		return false;
	}

	int32 nextDelta, prevDelta;
	participant->GetNeighboringParticipantDeltas(nextDelta, prevDelta);

	// Low priority layers favor one or the other neighboring participant, higher priority layers test against both
	if (LayerGroup.PreferredNeighbor == EMiterLayerGroupPreferredNeighbor::Previous)
	{
		nextDelta = prevDelta;
	}
	else if(LayerGroup.PreferredNeighbor == EMiterLayerGroupPreferredNeighbor::Next)
	{
		prevDelta = nextDelta;
	}

	FMiterParticipantData* startParticipant = nullptr;
	FMiterParticipantData* endParticipant = nullptr;

	if (!ensureAlways(GetNeighboringParticipants(LayerGroup.ParticipantIdx, prevDelta, startParticipant, nextDelta, endParticipant)))
	{
		return false;
	}

	// Determine which sides of the "to" layer groups to extend against,
	// based on which way they are oriented around the miter edge.
	bool bSameWindingAsStart = (participant->bPlaneNormalCW == startParticipant->bPlaneNormalCW);
	bool bSameWindingAsEnd = (participant->bPlaneNormalCW == endParticipant->bPlaneNormalCW);
	bool bTargetStartOfStartObj = !bSameWindingAsStart;
	bool bTargetStartOfEndObj = bSameWindingAsEnd;

	FMiterHitResult startHitResult, endHitResult;
	participant->IntersectStructureGroup(LayerGroup, *startParticipant, true, bTargetStartOfStartObj, startHitResult);
	participant->IntersectStructureGroup(LayerGroup, *endParticipant, false, bTargetStartOfEndObj, endHitResult);

	// If we only have one top priority layer, extend it evenly to the furthest intersection
	if (LayerGroup.bIsLoneTopPriorityLayer)
	{
		if (startHitResult.Dist > endHitResult.Dist)
		{
			endHitResult = startHitResult;
		}
		else
		{
			startHitResult = endHitResult;
		}
	}

	// Using the collision results for extended sides,
	// try to get actual extension values against the neighboring layer groups
	auto& groupExtensions = participant->GroupExtensions[LayerGroup.GroupIdx];
	if (startHitResult.bRayHit && endHitResult.bRayHit)
	{
		participant->ExtendedGroupIndices.Add(LayerGroup.GroupIdx);

		groupExtensions.X = startHitResult.Dist;
		groupExtensions.Y = endHitResult.Dist;

		int32 layerGroupStartIdx, layerGroupEndIdx;
		layerGroupStartIdx = LayerGroup.StartIndex;
		layerGroupEndIdx = LayerGroup.EndIndex + 1;
		float groupThickness = (participant->LayerDims.LayerOffsets[layerGroupEndIdx] - participant->LayerDims.LayerOffsets[layerGroupStartIdx]);

		float layerGroupProgress = 0.0f;
		for (int32 layerIdx = layerGroupStartIdx; layerIdx < layerGroupEndIdx; ++layerIdx)
		{
			float layerThickness = participant->LayerDims.LayerOffsets[layerIdx + 1] - participant->LayerDims.LayerOffsets[layerIdx];
			// Accumulating layer thicknesses introduces more than the default SMALL_NUMBER (1e-8) of error,
			// but shouldn't accumulate more than a micrometer of error. Ensure here only for sanity checking.
			ensure(FMath::IsNearlyEqual(layerThickness, participant->LayerDims.LayerThicknesses[layerIdx], KINDA_SMALL_NUMBER));
			float layerStartAlpha = layerGroupProgress / groupThickness;
			layerGroupProgress += layerThickness;
			float layerEndAlpha = layerGroupProgress / groupThickness;
			participant->LayerExtensions[layerIdx].X = FMath::Lerp(groupExtensions.X, groupExtensions.Y, layerStartAlpha);
			participant->LayerExtensions[layerIdx].Y = FMath::Lerp(groupExtensions.X, groupExtensions.Y, layerEndAlpha);
		}

		return true;
	}

	// TODO: when we fail to find a suitable target in our favored participant, move on to the next one in winding order
	groupExtensions.X = groupExtensions.Y = 0;

	return false;
}
