// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/PlaneBase.h"

#include "MetaPlaneSpan.generated.h"

USTRUCT()
struct MODUMATE_API FMOIMetaPlaneSpanData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> GraphMembers;
};

UCLASS()
class MODUMATE_API AMOIMetaPlaneSpan : public AMOIPlaneBase
{
	GENERATED_BODY()
public:
	AMOIMetaPlaneSpan();

	virtual FVector GetLocation() const override;

	UPROPERTY()
	FMOIMetaPlaneSpanData InstanceData;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	const FGraph3DFace* GetCachedGraphFace() const { return CachedGraphFace; }

protected:

	bool TryUpdateCachedGraphData();

	FGraph3DFace* CachedGraphFace = nullptr;
};