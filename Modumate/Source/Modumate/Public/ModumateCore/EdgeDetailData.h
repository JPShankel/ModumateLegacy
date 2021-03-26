// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EdgeDetailData.generated.h"

enum class EObjectType : uint8;
struct FMiterParticipantData;
class IMiterNode;

UENUM()
enum class EDetailParticipantType : uint8
{
	None,
	Layered,
	Rigged,
	Extruded
};

USTRUCT()
struct FEdgeDetailCondition
{
	GENERATED_BODY()

	FEdgeDetailCondition();
	FEdgeDetailCondition(const FMiterParticipantData* MiterParticipantData);
	FEdgeDetailCondition(float InAngle, float InOffset, const TArray<float, TInlineAllocator<8>>& InLayerThicknesses, EDetailParticipantType InType);
	void SetData(float InAngle, float InOffset, const TArray<float, TInlineAllocator<8>>& InLayerThicknesses, EDetailParticipantType InType);

	void Invert();

	UPROPERTY(meta = (ToolTip = "The angle (rounded to the nearest degree), from the perspective looking along the direction of the edge, of the internal normal of the separator participant's face."))
	float Angle = 0.0f;

	UPROPERTY(meta = (ToolTip = "The offset (rounded to the 64th of an inch) of the separator participant, relative to its face's normal."))
	float Offset = 0.0f;

	UPROPERTY(meta = (ToolTip = "The ordered thicknesses (rounded to the 64th of an inch) of a separator participant, potentially affected by its own flip value."))
	TArray<float> LayerThicknesses;

	UPROPERTY(meta = (ToolTip = "The type of participant that provides the condition, i.e. to distinguish Walls from Windows from Mullions"))
	EDetailParticipantType Type = EDetailParticipantType::None;

	bool operator==(const FEdgeDetailCondition& Other) const;
	bool operator!=(const FEdgeDetailCondition& Other) const;
	friend uint32 GetTypeHash(const FEdgeDetailCondition& EdgeDetailContition);

	static EDetailParticipantType ObjectTypeToParticipantType(EObjectType ObjectType);
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

	UPROPERTY(meta = (ToolTip = "The extension/retraction of the start and end Surface-hosting sides (in X and Y respectively) of a separator."))
	FVector2D SurfaceExtensions = FVector2D::ZeroVector;

	void Invert();

	friend uint32 GetTypeHash(const FEdgeDetailOverrides& EdgeDetailOverrides);
};

USTRUCT()
struct FEdgeDetailData
{
	GENERATED_BODY()

	FEdgeDetailData();
	FEdgeDetailData(int32 InVersion);
	FEdgeDetailData(const IMiterNode* MiterNode);

	void Reset();

	// Generate an oriented detail data, for both direct comparison/application and hash calculation.
	void OrientData(int32 OrientationIdx);

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

	// Function that's supposed to be automatically called after struct [de]serialization, to make sure the cached hash is up-to-date.
	void PostSerialize(const FArchive& Ar);

	FText MakeShortDisplayText(int32 Index = INDEX_NONE) const;

	UPROPERTY()
	int32 Version = 0;

	UPROPERTY(meta = (ToolTip = "All conditions of participants in an edge detail, that must be match in order for overrides to apply."))
	TArray<FEdgeDetailCondition> Conditions;

	UPROPERTY(meta = (ToolTip = "All per-participant data to override, given that all participants match conditions."))
	TArray<FEdgeDetailOverrides> Overrides;

	// Version 0: initial version, implicitly loaded from when Version was missing
	// Version 1: FEdgeDetailOverrides now has SurfaceExtensions
	// Version 2: FEdgeDetailCondition now has Type
	static constexpr int32 CurrentVersion = 2;

	uint32 CachedConditionHash = 0;

protected:
	static FEdgeDetailData TempOrientedDetail;
	static TArray<uint32> TempOrientedConditionHashes;
};

template<>
struct TStructOpsTypeTraits<FEdgeDetailData> : public TStructOpsTypeTraitsBase2<FEdgeDetailData>
{
	enum
	{
		WithPostSerialize = true
	};
};

