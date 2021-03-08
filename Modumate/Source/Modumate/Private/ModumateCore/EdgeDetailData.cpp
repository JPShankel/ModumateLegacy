// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/EdgeDetailData.h"

#include "Misc/Crc.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Objects/MiterNode.h"

#define LOCTEXT_NAMESPACE "ModumateEdgeDetail"

FEdgeDetailCondition::FEdgeDetailCondition()
{
}

FEdgeDetailCondition::FEdgeDetailCondition(const FMiterParticipantData* MiterParticipantData)
{
	if (ensure(MiterParticipantData))
	{
		SetData(
			MiterParticipantData->MiterAngle,
			MiterParticipantData->LayerStartOffset + 0.5f * MiterParticipantData->LayerDims.TotalUnfinishedWidth,
			MiterParticipantData->LayerDims.LayerThicknesses);

		// Normalize all participants as if they're coming in clockwise, from the perspective of the miter edge coordinate system.
		if (!MiterParticipantData->bPlaneNormalCW)
		{
			Offset = (Offset == 0.0f) ? Offset : -Offset;
			Algo::Reverse(LayerThicknesses);
		}
	}
}

FEdgeDetailCondition::FEdgeDetailCondition(float InAngle, float InOffset, const TArray<float, TInlineAllocator<8>>& InLayerThicknesses)
{
	SetData(InAngle, InOffset, InLayerThicknesses);
}

void FEdgeDetailCondition::SetData(float InAngle, float InOffset, const TArray<float, TInlineAllocator<8>>& InLayerThicknesses)
{
	// For readability and consistency, save condition angles to the nearest degree,
	// and serialize input dimensions that are in centimeters as inches to the nearest 64th.
	Angle = FMath::RoundHalfFromZero(InAngle);
	Offset = UModumateDimensionStatics::CentimetersToInches64(InOffset);

	LayerThicknesses.Reset();
	Algo::Transform(InLayerThicknesses, LayerThicknesses, UModumateDimensionStatics::CentimetersToInches64);
}

void FEdgeDetailCondition::Invert()
{
	Angle = (Angle == 0.0f) ? Angle : (360.0f - Angle);
	Offset = (Offset == 0.0f) ? Offset : -Offset;
	Algo::Reverse(LayerThicknesses);
}

bool FEdgeDetailCondition::operator==(const FEdgeDetailCondition& Other) const
{
	return (Angle == Other.Angle) && (Offset == Other.Offset) && (LayerThicknesses == Other.LayerThicknesses);
}

bool FEdgeDetailCondition::operator!=(const FEdgeDetailCondition& Other) const
{
	return !(*this == Other);
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
	if (ensure(MiterParticipantData))
	{
		LayerExtensions = MiterParticipantData->LayerExtensions;

		// Normalize all participants as if they're coming in clockwise, from the perspective of the miter edge coordinate system.
		if (!MiterParticipantData->bPlaneNormalCW)
		{
			Algo::Reverse(LayerExtensions);
			for (FVector2D& layerExtension : LayerExtensions)
			{
				Swap(layerExtension.X, layerExtension.Y);
			}
		}
	}
}

uint32 GetTypeHash(const FEdgeDetailOverrides& EdgeDetailOverrides)
{
	int32 numLayers = EdgeDetailOverrides.LayerExtensions.Num();
	uint32 currentHash = GetTypeHash(numLayers);
	return FCrc::MemCrc32(EdgeDetailOverrides.LayerExtensions.GetData(), numLayers * EdgeDetailOverrides.LayerExtensions.GetTypeSize(), currentHash);
}


TArray<FEdgeDetailCondition> FEdgeDetailData::TempOrientedConditions;
TArray<uint32> FEdgeDetailData::TempOrientedConditionHashes;

FEdgeDetailData::FEdgeDetailData()
{
}

FEdgeDetailData::FEdgeDetailData(const IMiterNode* MiterNode)
{
	FillFromMiterNode(MiterNode);
}

void FEdgeDetailData::OrientConditions(int32 OrientationIdx, TArray<FEdgeDetailCondition>& OutConditions) const
{
	OutConditions = Conditions;
	int32 numConditions = Conditions.Num();

	if ((numConditions == 0) || (OrientationIdx == 0) || !Conditions.IsValidIndex(OrientationIdx / 2))
	{
		return;
	}

	int32 rotationIdx = OrientationIdx % numConditions;
	bool bFlipped = OrientationIdx >= numConditions;

	if (bFlipped)
	{
		Algo::Reverse(OutConditions);
		NormalizeConditionAngles(OutConditions);
		for (auto& condition : OutConditions)
		{
			condition.Invert();
		}
	}

	// Rotate the conditions a number of times based on which rotation index we're given
	for (int32 i = 0; i < rotationIdx; ++i)
	{
		FEdgeDetailCondition firstCondition = OutConditions[0];
		OutConditions.Add(firstCondition);
		OutConditions.RemoveAt(0, 1, false);
	}

	NormalizeConditionAngles(OutConditions);
}

bool FEdgeDetailData::CompareConditions(const FEdgeDetailData& OtherDetail, TArray<int32>& OutOrientationIndices) const
{
	OutOrientationIndices.Reset();

	int32 numConditions = Conditions.Num();
	if (numConditions != OtherDetail.Conditions.Num())
	{
		return false;
	}

	// Empty conditions are vacuously equal.
	if (numConditions == 0)
	{
		OutOrientationIndices.Add(0);
		return true;
	}

	// Otherwise, figure out which combination of flipping and/or rotation makes this set of conditions equal to the other set of conditions.
	for (int32 orientationIdx = 0; orientationIdx < 2 * numConditions; ++orientationIdx)
	{
		OrientConditions(orientationIdx, TempOrientedConditions);

		if (TempOrientedConditions == OtherDetail.Conditions)
		{
			OutOrientationIndices.Add(orientationIdx);
		}
	}

	// TODO: determine if some of the orientations would result in identical mappings of overrides.
	// For example, symmetrically mitered centered symmetric participants could have equal conditions at several orientations,
	// but some of them could result in the same override application.

	bool bHasEqualOrientation = (OutOrientationIndices.Num() > 0);
	bool bHashesEqual = (CachedConditionHash == OtherDetail.CachedConditionHash);
	if (!ensure(bHasEqualOrientation == bHashesEqual))
	{
		UE_LOG(LogTemp, Error, TEXT("Detail hash 0x%08X should%smatch other detail hash 0x%08X!"),
			CachedConditionHash, bHasEqualOrientation ? TEXT(" ") : TEXT(" not "), OtherDetail.CachedConditionHash);
	}

	return bHasEqualOrientation;
}

void FEdgeDetailData::NormalizeConditionAngles(TArray<FEdgeDetailCondition>& OutConditions)
{
	if (OutConditions.Num() == 0)
	{
		return;
	}

	float startingAngle = OutConditions[0].Angle;
	for (auto& condition : OutConditions)
	{
		condition.Angle = FRotator::ClampAxis(condition.Angle - startingAngle);
	}
}

uint32 FEdgeDetailData::CalculateConditionHash() const
{
	int32 numConditions = Conditions.Num();
	if (numConditions == 0)
	{
		return 0;
	}

	TempOrientedConditionHashes.Reset();

	// Calculate a hash for each orientation (flip + rotation) of the conditions, so we can combine them.
	for (int32 orientationIdx = 0; orientationIdx < 2 * numConditions; ++orientationIdx)
	{
		uint32 orientationHash = 0;
		OrientConditions(orientationIdx, TempOrientedConditions);
		for (const FEdgeDetailCondition& condition : TempOrientedConditions)
		{
			orientationHash = HashCombine(orientationHash, GetTypeHash(condition));
		}
		TempOrientedConditionHashes.Add(orientationHash);
	}

	// Combine the sorted hashes for each orientation, so they're consistently combined for every possible original orientation.
	uint32 combinedHash = 0;
	TempOrientedConditionHashes.Sort();
	for (uint32 orientationHash : TempOrientedConditionHashes)
	{
		combinedHash = HashCombine(combinedHash, orientationHash);
	}

	return combinedHash;
}

void FEdgeDetailData::UpdateConditionHash()
{
	CachedConditionHash = CalculateConditionHash();
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
	if (miterData.SortedMiterIDs.Num() == 0)
	{
		return;
	}

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

	NormalizeConditionAngles(Conditions);
	UpdateConditionHash();
}

void FEdgeDetailData::PostSerialize(const FArchive& Ar)
{
	UpdateConditionHash();
}

FText FEdgeDetailData::MakeShortDisplayText(int32 Index) const
{
	FText entireFormat = LOCTEXT("ShortDisplayEntireFormat", "{0}-Participant{1}{2}");
	FText angleFormat = LOCTEXT("ShortDisplayWholeAngleFormat", ", {0}");
	FText indexFormat = LOCTEXT("ShortDisplayIndexFormat", ", #{0}");

	int32 numParticipants = Conditions.Num();
	FText numParticipantsText = FText::AsNumber(numParticipants);

	FText angleText = FText::GetEmpty();
	if (numParticipants > 1)
	{
		static const FNumberFormattingOptions angleNumberFormatOptions = FNumberFormattingOptions().SetMaximumFractionalDigits(0);
		static TArray<FText> tempAnglePartTexts;
		tempAnglePartTexts.Reset();

		for (int32 conditionIdx = 1; conditionIdx < numParticipants; ++conditionIdx)
		{
			FText anglePartText = FText::Format(LOCTEXT("ShortDisplayAnglePartFormat", "{0}°"),
				FText::AsNumber(Conditions[conditionIdx].Angle, &angleNumberFormatOptions));
			tempAnglePartTexts.Add(anglePartText);
		}

		angleText = FText::Format(angleFormat, FText::Join(LOCTEXT("ShortDisplayAngleDelim", "-"), tempAnglePartTexts));
	}

	FText indexText = (Index == INDEX_NONE) ? FText::GetEmpty() : FText::Format(indexFormat, FText::AsNumber(Index));
	
	return FText::Format(entireFormat, numParticipantsText, angleText, indexText);
}

#undef LOCTEXT_NAMESPACE
