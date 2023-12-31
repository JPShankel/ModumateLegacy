// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DrawingDesigner/DrawingDesignerNode.h"
#include "DrawingDesigner/DrawingDesignerView.h"
#include "DrawingDesignerDocument.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ModumateDrawingDesigner, Log, All);

USTRUCT()
struct MODUMATE_API FDrawingDesignerDocument 
{
	GENERATED_BODY()

	FDrawingDesignerDocument();

	UPROPERTY()
	int32 version = INDEX_NONE;
	
	UPROPERTY()
	int32 nextId = 1;

	UPROPERTY()
	TMap<FString, FDrawingDesignerNode> nodes;

	bool bDirty = false;

	int32 GetAndIncrDrawingId() {
		return nextId++;
	};

	bool Add(FDrawingDesignerNode obj);
	bool Remove(const FString& Id);
	bool Modify(FDrawingDesignerNode obj);

	bool Validate();
	
	bool WriteJson(FString& OutJson) const;
	bool ReadJson(const FString& InJson);

	bool operator==(const FDrawingDesignerDocument& RHS) const;
	bool operator!=(const FDrawingDesignerDocument& RHS) const;

protected:
	bool RemoveRecurse(const FString& Id);
	bool RemoveGhosts(const int32 RootId, const int32 Id);
	bool ValidateRecurse(const FString& id, TMap<FString, bool>& seenNodeMap);
};