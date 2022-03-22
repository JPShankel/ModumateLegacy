// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaPlaneSpan.h"
#include "DocumentManagement/ModumateDocument.h"

AMOIMetaPlaneSpan::AMOIMetaPlaneSpan()
	: AMOIPlaneBase()
{
}

FVector AMOIMetaPlaneSpan::GetLocation() const
{
	return Super::GetLocation();
}

bool AMOIMetaPlaneSpan::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (DirtyFlag == EObjectDirtyFlags::Structure)
	{
		// When a span loses its all its hosted objects, delete it
		if (GetChildIDs().Num() == 0)
		{
			TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
			delta->AddCreateDestroyState(StateData, EMOIDeltaType::Destroy);
			OutSideEffectDeltas->Add(delta);
		}
	}
	return AMOIPlaneBase::CleanObject(DirtyFlag, OutSideEffectDeltas);
}


void AMOIMetaPlaneSpan::SetupDynamicGeometry()
{
	TryUpdateCachedGraphData();
}

void AMOIMetaPlaneSpan::MakeMetaPlaneSpanDeltaPtr(UModumateDocument* Doc, int32 NewID, const TArray<int32>& InMemberObjects, TSharedPtr<FMOIDelta>& OutMoiDeltaPtr)
{
	FMOIStateData newSpanStateData;
	newSpanStateData.ID = NewID;
	newSpanStateData.ObjectType = EObjectType::OTMetaPlaneSpan;

	FMOIMetaPlaneSpanData newSpanCustomData;
	for (const auto curID : InMemberObjects)
	{
		const auto volumeGraph = Doc->FindVolumeGraph(curID);
		if (volumeGraph)
		{
			const auto face = volumeGraph->FindFace(curID);
			if (face)
			{
				newSpanCustomData.GraphMembers.Add(face->ID);
			}
		}
	}
	newSpanStateData.CustomData.SaveStructData<FMOIMetaPlaneSpanData>(newSpanCustomData);
	OutMoiDeltaPtr->AddCreateDestroyState(newSpanStateData, EMOIDeltaType::Create);
}

void AMOIMetaPlaneSpan::MakeMetaPlaneSpanDeltaFromGraph(FGraph3D* InGraph, int32 NewID, const TArray<int32>& InMemberObjects, TArray<FDeltaPtr>& OutDeltaPtrs)
{
	FMOIStateData newSpanStateData;
	newSpanStateData.ID = NewID;
	newSpanStateData.ObjectType = EObjectType::OTMetaPlaneSpan;

	FMOIMetaPlaneSpanData newSpanCustomData;
	for (const auto curID : InMemberObjects)
	{
		const auto face = InGraph->FindFace(curID);
		if (face)
		{
			newSpanCustomData.GraphMembers.Add(face->ID);
		}
	}
	newSpanStateData.CustomData.SaveStructData<FMOIMetaPlaneSpanData>(newSpanCustomData);

	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(newSpanStateData, EMOIDeltaType::Create);
	OutDeltaPtrs.Add(delta);
}

bool AMOIMetaPlaneSpan::TryUpdateCachedGraphData()
{
	if (InstanceData.GraphMembers.Num() < 2)
	{
		// Single face only
		int32 curFaceID = InstanceData.GraphMembers[0];
		auto* graph = GetDocument()->FindVolumeGraph(curFaceID);
		CachedGraphFace = graph ? graph->FindFace(curFaceID) : nullptr;
	}
	else
	{
		// After two faces are join, the new face will be used to join the following faces
		int32 newFaceID = InstanceData.GraphMembers[0];
		int32 docNextID = Document->GetNextAvailableID();
		for (int32 i = 0; i < InstanceData.GraphMembers.Num() - 1; ++i)
		{
			TArray<int32> objPair = { newFaceID, InstanceData.GraphMembers[i + 1] };
			TArray<FGraph3DDelta> graphDeltas;
			if (!Document->GetTempVolumeGraph().GetDeltasForSpanJoin(graphDeltas, objPair, docNextID, EGraph3DObjectType::Face))
			{
				FGraph3D::CloneFromGraph(Document->GetTempVolumeGraph(), *Document->GetVolumeGraph());
				return false;
			}

			// Find the new face to be used for next joining
			for (auto curDelta : graphDeltas)
			{
				for (auto kvp : curDelta.FaceAdditions)
				{
					newFaceID = kvp.Key;
				}
			}
		}

		FGraph3DFace* newFace = Document->GetTempVolumeGraph().FindFace(newFaceID);
		if (newFace && newFace->bValid)
		{
			CachedGraphFace = newFace;
		}
	}

	if (ensure(CachedGraphFace && (CachedGraphFace->CachedPositions.Num() > 0)))
	{
		CachedPoints = CachedGraphFace->CachedPositions;
		CachedPlane = CachedGraphFace->CachedPlane;
		CachedAxisX = CachedGraphFace->Cached2DX;
		CachedAxisY = CachedGraphFace->Cached2DY;
		CachedOrigin = CachedGraphFace->CachedPositions[0];
		CachedCenter = CachedGraphFace->CachedCenter;
		CachedHoles = CachedGraphFace->CachedHoles;
		return true;
	}
	return false;
}
