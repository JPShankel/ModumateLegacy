// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EdgeDetailData.generated.h"

USTRUCT()
struct FEdgeDetailCondition
{
	GENERATED_BODY()

	FEdgeDetailCondition();
	FEdgeDetailCondition(const struct FMiterParticipantData* MiterParticipantData);

	UPROPERTY(meta = (ToolTip = "The angle, from the perspective looking along the direction of the edge, of the internal normal of the separator participant's face."))
	float Angle = 0.0f;

	UPROPERTY(meta = (ToolTip = "The offset of the separator participant, relative to its face's normal."))
	float Offset = 0.0f;

	UPROPERTY(meta = (ToolTip = "The ordered thicknesses of a separator participant, potentially affected by its own flip value."))
	TArray<float> LayerThicknesses;

	friend uint32 GetTypeHash(const FEdgeDetailCondition& EdgeDetailContition);
};

USTRUCT()
struct FEdgeDetailOverrides
{
	GENERATED_BODY()

	FEdgeDetailOverrides();
	FEdgeDetailOverrides(const struct FMiterParticipantData* MiterParticipantData);

	UPROPERTY(meta = (ToolTip = "The extension/retraction of the start and end (in X and Y respectively) of each layer of a separator participant."))
	TArray<FVector2D> LayerExtensions;

	friend uint32 GetTypeHash(const FEdgeDetailOverrides& EdgeDetailOverrides);
};

USTRUCT()
struct FEdgeDetailData
{
	GENERATED_BODY()

	FEdgeDetailData();
	FEdgeDetailData(const class IMiterNode* MiterNode);
	~FEdgeDetailData();

	void UpdateConditionHash();
	void FillFromMiterNode(const class IMiterNode* MiterNode);

	UPROPERTY(meta = (ToolTip = "All conditions of participants in an edge detail, that must be match in order for overrides to apply."))
	TArray<FEdgeDetailCondition> Conditions;

	UPROPERTY(meta = (ToolTip = "All per-participant data to override, given that all participants match conditions."))
	TArray<FEdgeDetailOverrides> Overrides;

	uint32 CachedConditionHash = 0;
};
