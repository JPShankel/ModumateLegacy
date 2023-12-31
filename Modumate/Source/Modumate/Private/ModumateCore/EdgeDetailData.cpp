// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/EdgeDetailData.h"

#include "Misc/Crc.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Objects/MiterNode.h"
#include "Objects/ModumateObjectInstance.h"

#define LOCTEXT_NAMESPACE "ModumateEdgeDetail"

FEdgeDetailCondition::FEdgeDetailCondition()
{
}

FEdgeDetailCondition::FEdgeDetailCondition(const FMiterParticipantData* MiterParticipantData)
{
	if (ensure(MiterParticipantData))
	{
		EObjectType participantObjectType = MiterParticipantData->MOI ? MiterParticipantData->MOI->GetObjectType() : EObjectType::OTNone;
		EDetailParticipantType participantType = ObjectTypeToParticipantType(participantObjectType);

		SetData(
			MiterParticipantData->MiterAngle,
			MiterParticipantData->LayerStartOffset + 0.5f * MiterParticipantData->LayerDims.TotalUnfinishedWidth,
			MiterParticipantData->LayerDims.LayerThicknesses,
			participantType);

		// Normalize all participants as if they're coming in clockwise, from the perspective of the miter edge coordinate system.
		if (!MiterParticipantData->bPlaneNormalCW)
		{
			Offset = (Offset == 0.0f) ? Offset : -Offset;
			Algo::Reverse(LayerThicknesses);
		}
	}
}

FEdgeDetailCondition::FEdgeDetailCondition(float InAngle, float InOffset, const TArray<float, TInlineAllocator<8>>& InLayerThicknesses, EDetailParticipantType InType)
{
	SetData(InAngle, InOffset, InLayerThicknesses, InType);
}

void FEdgeDetailCondition::SetData(float InAngle, float InOffset, const TArray<float, TInlineAllocator<8>>& InLayerThicknesses, EDetailParticipantType InType)
{
	// For readability and consistency, save condition angles to the nearest degree,
	// and serialize input dimensions that are in centimeters as inches to the nearest 64th.
	Angle = FMath::RoundHalfFromZero(InAngle);
	Offset = UModumateDimensionStatics::CentimetersToInches64(InOffset);

	LayerThicknesses.Reset();
	Algo::Transform(InLayerThicknesses, LayerThicknesses, UModumateDimensionStatics::CentimetersToInches64);

	Type = InType;
}

void FEdgeDetailCondition::Invert()
{
	Angle = (Angle == 0.0f) ? Angle : (360.0f - Angle);
	Offset = (Offset == 0.0f) ? Offset : -Offset;
	Algo::Reverse(LayerThicknesses);
}

bool FEdgeDetailCondition::operator==(const FEdgeDetailCondition& Other) const
{
	return (Angle == Other.Angle) && (Offset == Other.Offset) && (LayerThicknesses == Other.LayerThicknesses) && (Type == Other.Type);
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

	currentHash = HashCombine(currentHash, GetTypeHash(EdgeDetailContition.Type));

	return currentHash;
}

EDetailParticipantType FEdgeDetailCondition::ObjectTypeToParticipantType(EObjectType ObjectType)
{
	switch (ObjectType)
	{
	case EObjectType::OTWallSegment:
	case EObjectType::OTRailSegment:
	case EObjectType::OTFloorSegment:
	case EObjectType::OTCeiling:
	case EObjectType::OTRoofFace:
	case EObjectType::OTStaircase:
	case EObjectType::OTFinish:
	case EObjectType::OTCountertop:
	case EObjectType::OTSystemPanel:
		return EDetailParticipantType::Layered;
	case EObjectType::OTDoor:
	case EObjectType::OTWindow:
		return EDetailParticipantType::Rigged;
	case EObjectType::OTTrim:
	case EObjectType::OTStructureLine:
	case EObjectType::OTMullion:
		return EDetailParticipantType::Extruded;
	default:
		return EDetailParticipantType::None;
	}
}


FEdgeDetailOverrides::FEdgeDetailOverrides()
{
}

FEdgeDetailOverrides::FEdgeDetailOverrides(const FMiterParticipantData* MiterParticipantData)
{
	if (ensure(MiterParticipantData))
	{
		LayerExtensions = MiterParticipantData->LayerExtensions;
		SurfaceExtensions = MiterParticipantData->SurfaceExtensions;

		// Normalize all participants as if they're coming in clockwise, from the perspective of the miter edge coordinate system.
		if (!MiterParticipantData->bPlaneNormalCW)
		{
			Invert();
		}
	}
}

void FEdgeDetailOverrides::Invert()
{
	Algo::Reverse(LayerExtensions);
	for (FVector2D& layerExtension : LayerExtensions)
	{
		Swap(layerExtension.X, layerExtension.Y);
	}
	Swap(SurfaceExtensions.X, SurfaceExtensions.Y);
}

uint32 GetTypeHash(const FEdgeDetailOverrides& EdgeDetailOverrides)
{
	int32 numLayers = EdgeDetailOverrides.LayerExtensions.Num();
	uint32 currentHash = GetTypeHash(numLayers);
	currentHash = FCrc::MemCrc32(EdgeDetailOverrides.LayerExtensions.GetData(), numLayers * EdgeDetailOverrides.LayerExtensions.GetTypeSize(), currentHash);
	currentHash = HashCombine(currentHash, GetTypeHash(EdgeDetailOverrides.SurfaceExtensions));
	return currentHash;
}


FEdgeDetailData FEdgeDetailData::TempOrientedDetail(FEdgeDetailData::CurrentVersion);
TArray<uint32> FEdgeDetailData::TempOrientedConditionHashes;

FEdgeDetailData::FEdgeDetailData()
{
}

FEdgeDetailData::FEdgeDetailData(int32 InVersion)
	: Version(InVersion)
{
}

FEdgeDetailData::FEdgeDetailData(const IMiterNode* MiterNode)
{
	FillFromMiterNode(MiterNode);
}

void FEdgeDetailData::Reset()
{
	Conditions.Reset();
	Overrides.Reset();
	CachedConditionHash = 0;
}

void FEdgeDetailData::OrientData(int32 OrientationIdx)
{
	int32 numParticipants = Conditions.Num();

	if ((OrientationIdx == 0) ||
		!ensure((numParticipants > 0) && (numParticipants == Overrides.Num()) &&
			(OrientationIdx != INDEX_NONE) && (OrientationIdx < 2 * numParticipants)))
	{
		return;
	}

	int32 rotationIdx = OrientationIdx % numParticipants;
	bool bFlipped = OrientationIdx >= numParticipants;

	if (bFlipped)
	{
		Algo::Reverse(Conditions);
		Algo::Reverse(Overrides);
		NormalizeConditionAngles(Conditions);
		for (int32 participantIdx = 0; participantIdx < numParticipants; ++participantIdx)
		{
			Conditions[participantIdx].Invert();
			Overrides[participantIdx].Invert();
		}
	}

	// Rotate the conditions and overrides a number of times based on which rotation index we're given
	for (int32 i = 0; i < rotationIdx; ++i)
	{
		FEdgeDetailCondition firstCondition = Conditions[0];
		Conditions.Add(firstCondition);
		Conditions.RemoveAt(0, 1, false);

		FEdgeDetailOverrides firstOverrides = Overrides[0];
		Overrides.Add(firstOverrides);
		Overrides.RemoveAt(0, 1, false);
	}

	NormalizeConditionAngles(Conditions);
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
		TempOrientedDetail = *this;
		TempOrientedDetail.OrientData(orientationIdx);

		if (TempOrientedDetail.Conditions == OtherDetail.Conditions)
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
		TempOrientedDetail = *this;
		TempOrientedDetail.OrientData(orientationIdx);
		for (const FEdgeDetailCondition& condition : TempOrientedDetail.Conditions)
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
	Reset();

	if (!ensure(MiterNode))
	{
		return;
	}

	Version = FEdgeDetailData::CurrentVersion;

	const FMiterData& miterData = MiterNode->GetMiterData();
	if (miterData.SortedParticipantIDs.Num() == 0)
	{
		return;
	}

	for (int32 participantID : miterData.SortedParticipantIDs)
	{
		// TODO: participants with negative IDs is a hack used to allow top-priority layers to extend across the miter area
		// They will inherit the detail information from their positive twin
		// To be refactored out with mitering 2.0
		if (participantID < 0)
		{
			continue;
		}
		const FMiterParticipantData* participantData = miterData.ParticipantsByID.Find(participantID);
		if (ensure(participantData))
		{
			FEdgeDetailCondition participantCondition(participantData);
			FEdgeDetailOverrides participantOverride(participantData);
			if (ensure(participantCondition.LayerThicknesses.Num() == participantOverride.LayerExtensions.Num()))
			{
				Conditions.Add(MoveTemp(participantCondition));
				Overrides.Add(MoveTemp(participantOverride));
			}
		}
	}

	NormalizeConditionAngles(Conditions);
	UpdateConditionHash();
}

void FEdgeDetailData::PostSerialize(const FArchive& Ar)
{
	if (Version < CurrentVersion)
	{
		// For detail overrides missing SurfaceExtensions, default them to the outer values of LayerExtensions
		if (Version < 1)
		{
			for (auto& detailOverrides : Overrides)
			{
				int32 numLayers = detailOverrides.LayerExtensions.Num();
				if (numLayers > 0)
				{
					detailOverrides.SurfaceExtensions.X = detailOverrides.LayerExtensions[0].X;
					detailOverrides.SurfaceExtensions.Y = detailOverrides.LayerExtensions.Last().Y;
				}
			}
		}

		// For detail conditions missing Type, default them to layered assemblies, since that was the only type previously supported.
		if (Version < 2)
		{
			for (auto& detailCondition : Conditions)
			{
				if (detailCondition.Type == EDetailParticipantType::None)
				{
					detailCondition.Type = EDetailParticipantType::Layered;
				}
			}
		}

		Version = CurrentVersion;
	}

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
			FText anglePartText = FText::Format(LOCTEXT("ShortDisplayAnglePartFormat", "{0}\u00B0"),
				FText::AsNumber(Conditions[conditionIdx].Angle, &angleNumberFormatOptions));
			tempAnglePartTexts.Add(anglePartText);
		}

		angleText = FText::Format(angleFormat, FText::Join(LOCTEXT("ShortDisplayAngleDelim", "-"), tempAnglePartTexts));
	}

	FText indexText = (Index == INDEX_NONE) ? FText::GetEmpty() : FText::Format(indexFormat, FText::AsNumber(Index));
	
	return FText::Format(entireFormat, numParticipantsText, angleText, indexText);
}

void FEdgeDetailData::ConvertToWebPreset(FBIMWebPreset& OutPreset)
{
	FString conditionsKey = TEXT("EdgeDetail.Conditions");
	FString overridesKey = TEXT("EdgeDetail.Overrides");
	
	FCustomDataWebConvertable::ConvertToWebPreset(OutPreset);
	
	FString conditionsJson, overridesJson;
	WriteJsonUstructArray(conditionsJson, &Conditions, TEXT("Conditions"));
	WriteJsonUstructArray(overridesJson, &Overrides, TEXT("Overrides"));

	OutPreset.properties.Add(conditionsKey, {conditionsKey, { conditionsJson}});
	OutPreset.properties.Add(overridesKey, {overridesKey, { overridesJson}});
}

void FEdgeDetailData::ConvertFromWebPreset(const FBIMWebPreset& InPreset)
{
	FString conditionsKey = TEXT("EdgeDetail.Conditions");
	FString overridesKey = TEXT("EdgeDetail.Overrides");
	
	FCustomDataWebConvertable::ConvertFromWebPreset(InPreset);
	
	if(InPreset.properties.Contains(conditionsKey))
	{
		FString conditionsJson = InPreset.properties[conditionsKey].value[0];
		ReadJsonUstructArray(conditionsJson, &Conditions, TEXT("Conditions"));
	}

	if(InPreset.properties.Contains(overridesKey))
	{
		FString overridesJson = InPreset.properties[overridesKey].value[0];
		ReadJsonUstructArray(overridesJson, &Overrides, TEXT("Overrides"));
	}
}

#undef LOCTEXT_NAMESPACE
