// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/MOIState.h"
#include "DocumentManagement//ModumateSerialization.h"
#include "BIMSymbolPresetData.generated.h"

USTRUCT()
struct MODUMATE_API FBIMSymbolPresetIDSet
{
	GENERATED_BODY()

	UPROPERTY()
	TSet<int32> IDSet;
};

USTRUCT()
struct MODUMATE_API FBIMSymbolPresetDataV26
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<int32, FMOIStateData> Members;

	UPROPERTY()
	FGraph3DRecordV1 Graph3d;

	UPROPERTY()
	TMap<int32, FGraph2DRecord> SurfaceGraphs;

	UPROPERTY()
	TMap<int32, FBIMSymbolPresetIDSet> EquivalentIDs;

	UPROPERTY()
	FVector Anchor { ForceInit };
};

USTRUCT()
struct MODUMATE_API FBIMSymbolPresetData : public FCustomDataWebConvertable
{
	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Symbol);
	}
	virtual UStruct* VirtualizedStaticStruct() override
	{
		return FBIMSymbolPresetData::StaticStruct();
	}
	GENERATED_BODY()

	UPROPERTY()
	TMap<int32, FMOIStateData> Members;

	UPROPERTY()
	TMap<int32, FGraph3DRecordV1> Graphs;

	UPROPERTY()
	TMap<int32, FGraph2DRecord> SurfaceGraphs;

	UPROPERTY()
	TMap<int32, FBIMSymbolPresetIDSet> EquivalentIDs;

	UPROPERTY()
	FVector Anchor { ForceInit };

	UPROPERTY()
	int32 RootGraph { MOD_ID_NONE };
};
