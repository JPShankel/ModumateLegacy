// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "EdgeDetailObj.generated.h"


USTRUCT()
struct MODUMATE_API FMOIEdgeDetailData
{
	GENERATED_BODY()

	FMOIEdgeDetailData();
	FMOIEdgeDetailData(int32 InOrientationIndex);

	UPROPERTY()
	int32 OrientationIndex = INDEX_NONE;
};

USTRUCT()
struct MODUMATE_API FMOITypicalEdgeDetailDelta : public FDocumentDelta
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid OldTypicalEdge;

	UPROPERTY()
	FGuid NewTypicalEdge;

	UPROPERTY()
	uint32 ConditionValue;

	virtual bool ApplyTo(UModumateDocument* Doc, UWorld* World) const override;
	virtual TSharedPtr<FDocumentDelta> MakeInverse() const override;
	virtual FStructDataWrapper SerializeStruct() override;
};

UCLASS()
class MODUMATE_API AMOIEdgeDetail : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	UPROPERTY()
	FMOIEdgeDetailData InstanceData;
};
