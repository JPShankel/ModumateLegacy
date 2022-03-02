// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaPlaneSpan.h"
#include "DocumentManagement/ModumateDocument.h"

AMOIMetaPlaneSpan::AMOIMetaPlaneSpan()
	: AMOIPlaneBase()
{
}

FVector AMOIMetaPlaneSpan::GetLocation() const
{
	if (InstanceData.GraphMembers.Num() > 0)
	{
		int32 graphMemberID = InstanceData.GraphMembers[0]; // TODO: Calculate average location
		const auto volumeGraph = Document->FindVolumeGraph(InstanceData.GraphMembers[0]);
		if (volumeGraph)
		{
			const auto face = volumeGraph->FindFace(graphMemberID);
			if (face)
			{
				return face->CachedCenter;
			}
		}
	}
	return FVector::ZeroVector;
}

void AMOIMetaPlaneSpan::MakeMetaPlaneSpanDeltaPtr(UModumateDocument* Doc, int32 NewID, const TArray<int32>& InMemberObjects, TSharedPtr<FMOIDelta>& OutMoiDeltaPtr)
{
	FMOIStateData newSpanStateData;
	newSpanStateData.ID = NewID;
	newSpanStateData.ObjectType = EObjectType::OTMetaPlaneSpan;

	FMOIMetaSpanData newSpanCustomData;
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
	newSpanStateData.CustomData.SaveStructData<FMOIMetaSpanData>(newSpanCustomData);
	OutMoiDeltaPtr->AddCreateDestroyState(newSpanStateData, EMOIDeltaType::Create);
}

void AMOIMetaPlaneSpan::MakeMetaPlaneSpanDeltaFromGraph(FGraph3D* InGraph, int32 NewID, const TArray<int32>& InMemberObjects, TArray<FDeltaPtr>& OutDeltaPtrs)
{
	FMOIStateData newSpanStateData;
	newSpanStateData.ID = NewID;
	newSpanStateData.ObjectType = EObjectType::OTMetaPlaneSpan;

	FMOIMetaSpanData newSpanCustomData;
	for (const auto curID : InMemberObjects)
	{
		const auto face = InGraph->FindFace(curID);
		if (face)
		{
			newSpanCustomData.GraphMembers.Add(face->ID);
		}
	}
	newSpanStateData.CustomData.SaveStructData<FMOIMetaSpanData>(newSpanCustomData);

	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(newSpanStateData, EMOIDeltaType::Create);
	OutDeltaPtrs.Add(delta);
}
