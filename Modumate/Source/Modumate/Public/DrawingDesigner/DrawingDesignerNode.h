// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "JsonObjectConverter.h"
#include "DrawingDesignerNode.generated.h"

UENUM()
enum class ENodeSchemaType
{
	unknown = 0,
	document,
	directory,
	page,
	annotation
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerNode
{
	GENERATED_BODY()
	FDrawingDesignerNode() {};

	UPROPERTY()
	int32 id = INDEX_NONE;

	UPROPERTY()
	int32 parent = INDEX_NONE;

	UPROPERTY()
	ENodeSchemaType nodeType = ENodeSchemaType::unknown;

	UPROPERTY()
	TArray<int32> children;

	TSharedPtr<FJsonObject> SerializedChunk;

	bool WriteJson(FString& OutJson) const;
	bool ReadJson(const FString& InJson);
	bool FromJsonObject(const TSharedPtr<FJsonObject> InValue);
	bool AsJsonObject(TSharedPtr<FJsonObject>& outObject) const;

	bool operator==(const FDrawingDesignerNode& RHS) const;
	bool operator!=(const FDrawingDesignerNode& RHS) const;
};
