// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/MOIState.h"
#include "DocumentManagement//ModumateSerialization.h"
#include "BIMKernel/Presets/CustomData/CustomDataWebConvertable.h"
#include "BIMSymbolPresetData.generated.h"


USTRUCT()
struct MODUMATE_API FBIMSymbolPresetIDSet
{
	friend bool operator==(const FBIMSymbolPresetIDSet& Lhs, const FBIMSymbolPresetIDSet& RHS)
	{
		return Lhs.IDSet.Difference(RHS.IDSet).Num() == 0;
	}

	friend bool operator!=(const FBIMSymbolPresetIDSet& Lhs, const FBIMSymbolPresetIDSet& RHS)
	{
		return !(Lhs == RHS);
	}

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

	virtual FString GetPropertyPrefix() const override
	{
		return GetEnumValueString(EPresetPropertyMatrixNames::Symbol);
	}
	virtual UStruct* VirtualizedStaticStruct() override
	{
		return FBIMSymbolPresetData::StaticStruct();
	}

	virtual void ConvertToWebPreset(FBIMWebPreset& OutPreset) override;
	virtual void ConvertFromWebPreset(const FBIMWebPreset& InPreset) override;

	friend bool operator==(const FBIMSymbolPresetData& Lhs, const FBIMSymbolPresetData& RHS)
	{
		return Lhs.Members.OrderIndependentCompareEqual(RHS.Members)
			&& Lhs.Graphs.OrderIndependentCompareEqual(RHS.Graphs)
			&& Lhs.SurfaceGraphs.OrderIndependentCompareEqual(RHS.SurfaceGraphs)
			&& Lhs.EquivalentIDs.OrderIndependentCompareEqual(RHS.EquivalentIDs)
			&& Lhs.Anchor == RHS.Anchor
			&& Lhs.RootGraph == RHS.RootGraph;
	}

	friend bool operator!=(const FBIMSymbolPresetData& Lhs, const FBIMSymbolPresetData& RHS)
	{
		return !(Lhs == RHS);
	}
};
