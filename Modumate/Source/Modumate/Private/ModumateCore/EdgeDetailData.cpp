// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/EdgeDetailData.h"

#include "Misc/Crc.h"
#include "Objects/MiterNode.h"


FEdgeDetailCondition::FEdgeDetailCondition()
{
}

FEdgeDetailCondition::FEdgeDetailCondition(const FMiterParticipantData* MiterParticipantData)
{
	if (!ensure(MiterParticipantData))
	{
		return;
	}

	Angle = MiterParticipantData->MiterAngle;
	Offset = MiterParticipantData->LayerStartOffset - 0.5f * MiterParticipantData->LayerDims.TotalUnfinishedWidth;
	LayerThicknesses = MiterParticipantData->LayerDims.LayerThicknesses;
}

uint32 GetTypeHash(const FEdgeDetailCondition& EdgeDetailContition)
{
	uint32 currentHash = GetTypeHash(EdgeDetailContition.Angle);
	currentHash = HashCombine(currentHash, GetTypeHash(EdgeDetailContition.Offset));

	int32 numLayers = EdgeDetailContition.LayerThicknesses.Num();
	currentHash = HashCombine(currentHash, GetTypeHash(numLayers));
	currentHash = FCrc::MemCrc32(EdgeDetailContition.LayerThicknesses.GetData(), numLayers * EdgeDetailContition.LayerThicknesses.GetTypeSize(), currentHash);
	return currentHash;
}


FEdgeDetailOverrides::FEdgeDetailOverrides()
{
}

FEdgeDetailOverrides::FEdgeDetailOverrides(const FMiterParticipantData* MiterParticipantData)
{
	if (!ensure(MiterParticipantData))
	{
		return;
	}

	LayerExtensions = MiterParticipantData->LayerExtensions;
}

uint32 GetTypeHash(const FEdgeDetailOverrides& EdgeDetailOverrides)
{
	int32 numLayers = EdgeDetailOverrides.LayerExtensions.Num();
	uint32 currentHash = GetTypeHash(numLayers);
	return FCrc::MemCrc32(EdgeDetailOverrides.LayerExtensions.GetData(), numLayers * EdgeDetailOverrides.LayerExtensions.GetTypeSize(), currentHash);
}


FEdgeDetailData::FEdgeDetailData()
{
}

FEdgeDetailData::FEdgeDetailData(const IMiterNode* MiterNode)
{
	FillFromMiterNode(MiterNode);
}

FEdgeDetailData::~FEdgeDetailData()
{
}

void FEdgeDetailData::UpdateConditionHash()
{
	CachedConditionHash = 0;
	for (const FEdgeDetailCondition& condition : Conditions)
	{
		CachedConditionHash = HashCombine(CachedConditionHash, GetTypeHash(condition));
	}
}

void FEdgeDetailData::FillFromMiterNode(const IMiterNode* MiterNode)
{
	Conditions.Reset();
	Overrides.Reset();
	CachedConditionHash = 0;

	if (!ensure(MiterNode))
	{
		return;
	}

	const FMiterData& miterData = MiterNode->GetMiterData();
	for (int32 participantID : miterData.SortedMiterIDs)
	{
		const FMiterParticipantData* participantData = miterData.ParticipantsByID.Find(participantID);
		FEdgeDetailCondition participantCondition(participantData);
		FEdgeDetailOverrides participantOverride(participantData);
		if (ensure(participantCondition.LayerThicknesses.Num() == participantOverride.LayerExtensions.Num()))
		{
			Conditions.Add(MoveTemp(participantCondition));
			Overrides.Add(MoveTemp(participantOverride));
		}
	}

	UpdateConditionHash();
}
