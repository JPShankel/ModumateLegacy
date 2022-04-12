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

	UPROPERTY()
	FMOIMetaPlaneSpanData InstanceData;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	virtual void SetupDynamicGeometry() override;

	const FGraph3DFace* GetPerimeterFace() const;

protected:

	FGraph3DFace CachedPerimeterFace;

	void UpdateCachedPerimeterFace();
};