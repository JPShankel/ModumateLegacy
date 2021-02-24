// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EdgeDetailData.generated.h"

struct FMiterParticipantData;
class IMiterNode;

USTRUCT()
struct FEdgeDetailCondition
{
	GENERATED_BODY()

	FEdgeDetailCondition();
	FEdgeDetailCondition(const FMiterParticipantData* MiterParticipantData);
	FEdgeDetailCondition(float InAngle, float InOffset, const TArray<float, TInlineAllocator<8>>& InLayerThicknesses);
	void SetData(float InAngle, float InOffset, const TArray<float, TInlineAllocator<8>>& InLayerThicknesses);

	void Invert();

	UPROPERTY(meta = (ToolTip = "The angle (rounded to the nearest degree), from the perspective looking along the direction of the edge, of the internal normal of the separator participant's face."))
	float Angle = 0.0f;

	UPROPERTY(meta = (ToolTip = "The offset (rounded to the 64th of an inch) of the separator participant, relative to its face's normal."))
	float Offset = 0.0f;

	UPROPERTY(meta = (ToolTip = "The ordered thicknesses (rounded to the 64th of an inch) of a separator participant, potentially affected by its own flip value."))
	TArray<float> LayerThicknesses;

	bool operator==(const FEdgeDetailCondition& Other) const;
	bool operator!=(const FEdgeDetailCondition& Other) const;
	friend uint32 GetTypeHash(const FEdgeDetailCondition& EdgeDetailContition);
};

template<>
struct TStructOpsTypeTraits<FEdgeDetailCondition> : public TStructOpsTypeTraitsBase2<FEdgeDetailCondition>
{
	enum
	{
		WithIdenticalViaEquality = true
	};
};


USTRUCT()
struct FEdgeDetailOverrides
{
	GENERATED_BODY()

	FEdgeDetailOverrides();
	FEdgeDetailOverrides(const FMiterParticipantData* MiterParticipantData);

	UPROPERTY(meta = (ToolTip = "The extension/retraction of the start and end (in X and Y respectively) of each layer of a separator participant."))
	TArray<FVector2D> LayerExtensions;

	friend uint32 GetTypeHash(const FEdgeDetailOverrides& EdgeDetailOverrides);
};

USTRUCT()
struct FEdgeDetailData
{
	GENERATED_BODY()

	FEdgeDetailData();
	FEdgeDetailData(const IMiterNode* MiterNode);

	// Generate an oriented list of conditions, for both direct comparison and hash calculation.
	void OrientConditions(int32 OrientationIdx, TArray<FEdgeDetailCondition>& OutConditions) const;

	// Return whether conditions are the same hash, and find which orientation(s), if any, allow the target detail conditions to match the provided other detail.
	bool CompareConditions(const FEdgeDetailData& OtherDetail, TArray<int32>& OutOrientationIndices) const;

	// Modify each condition's angle such that they start at 0.
	static void NormalizeConditionAngles(TArray<FEdgeDetailCondition>& OutConditions);

	// Calculate the orientation-independent hash value for the conditions of this detail.
	uint32 CalculateConditionHash() const;

	// Recalculate the hash.
	void UpdateConditionHash();

	// Fill conditions and overrides from the participants and extensions of an already-evaluated miter node.
	void FillFromMiterNode(const IMiterNode* MiterNode);

	UPROPERTY(meta = (ToolTip = "All conditions of participants in an edge detail, that must be match in order for overrides to apply."))
	TArray<FEdgeDetailCondition> Conditions;

	UPROPERTY(meta = (ToolTip = "All per-participant data to override, given that all participants match conditions."))
	TArray<FEdgeDetailOverrides> Overrides;

	uint32 CachedConditionHash = 0;

protected:
	static TArray<FEdgeDetailCondition> TempOrientedConditions;
	static TArray<uint32> TempOrientedConditionHashes;
};
