// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/PlaneBase.h"

#include "MetaPlaneSpan.generated.h"

USTRUCT()
struct MODUMATE_API FMOIMetaSpanData
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
	FMOIMetaSpanData InstanceData;

	static void MakeMetaPlaneSpanDeltaPtr(UModumateDocument* Doc, int32 NewID, const TArray<int32>& InMemberObjects, TSharedPtr<FMOIDelta>& OutMoiDeltaPtr);
	static void MakeMetaPlaneSpanDeltaFromGraph(FGraph3D* InGraph, int32 NewID, const TArray<int32>& InMemberObjects, TArray<FDeltaPtr>& OutDeltaPtrs);
protected:

};