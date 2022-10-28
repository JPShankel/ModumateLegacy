// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/ModumateObjectInstance.h"

#include "MetaGraph.generated.h"

USTRUCT()
struct MODUMATE_API FMOIMetaGraphData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FQuat Rotation = FQuat::Identity;
};

UCLASS()
class MODUMATE_API AMOIMetaGraph: public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIMetaGraph();

	virtual void PostCreateObject(bool bNewObject) override;

	bool SetupBoundingBox();

	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual bool ShowStructureOnSelection() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual bool GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;
	virtual FTransform GetWorldTransform() const override;
	virtual bool ToWebMOI(FWebMOI& OutMOI) const override;
	virtual bool FromWebMOI(const FString& InJson) override;

	virtual FVector GetCorner(int32 Index) const override;
	virtual int32 GetNumCorners() const override { return CachedCorners.Num(); };
	
	UPROPERTY()
	FMOIMetaGraphData InstanceData;

private:
	FBox CachedBounds { ForceInitToZero };
	TArray<FVector> CachedCorners;
	FGuid CachedSymbolGuid;

	void SwapSymbol(TArray<FDeltaPtr>* OutSideEffectDeltas);
};

