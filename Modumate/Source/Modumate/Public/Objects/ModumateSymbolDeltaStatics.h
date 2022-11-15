// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MOIState.h"
#include "ToolsAndAdjustments/Common/SelectedObjectToolMixin.h"
#include "BIMKernel/Presets/BIMSymbolPresetData.h"

struct FMOIDeltaState;
enum class EMOIDeltaType: uint8;
struct FGraph3DDelta;
struct FGraph2DDelta;

class FBIMSymbolCollectionProxy
{
public:
	FBIMSymbolCollectionProxy(const FBIMPresetCollection* Base)
		: BasePresets(Base) { }
	// Return value only valid until next call:
	FBIMSymbolPresetData* PresetDataFromGUID(const FGuid& Guid);
	TArray<FGuid> GetCachedIDs() const;
	void GetPresetDeltas(TArray<FDeltaPtr>& OutDeltas);

private:
	TMap<FGuid, FBIMSymbolPresetData> LocalPresetData;
	const FBIMPresetCollection *const BasePresets;
};

class FModumateSymbolDeltaStatics
{
public:
	static void GetDerivedDeltasFromDeltas(UModumateDocument* Doc, EMOIDeltaType DeltaType, const TArray<FDeltaPtr>& InDeltas, TArray<FDeltaPtr>& DerivedDeltas);
	static void GetDerivedDeltasForMoi(UModumateDocument* Doc, const FMOIDeltaState& DeltaState, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas);
	static void GetDerivedDeltasForGraph3d(UModumateDocument* Doc, const FGraph3DDelta* GraphDelta, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas);
	static void GetDerivedDeltasForGraph2d(UModumateDocument* Doc, const FGraph2DDelta* GraphDelta, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas);
	static bool GetDerivedDeltasForPresetDelta(UModumateDocument* Doc, const FBIMPresetDelta* PresetDelta,
		EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas);

	static bool CreateDeltasForNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* SymbolGroup, TArray<FDeltaPtr>& OutDeltas);
	static bool CreatePresetDataForSymbol(UModumateDocument* Doc, const AModumateObjectInstance* SymbolGroup, const FTransform& Transform, FBIMSymbolPresetData& OutPreset);
	static bool CreateDeltasForNewSymbolInstance(UModumateDocument* Doc, int32 GroupID, int32& NextID, const FGuid& SymbolID, FBIMSymbolCollectionProxy& SymbolCollection, const FTransform& Transform,
		TArray<FDeltaPtr>& OutDeltas, const TSet<FGuid>& SymbolsList);
	static void PropagateChangedSymbolInstance(UModumateDocument* Doc, int32& NextID, int32 GroupID, TArray<FDeltaPtr>& OutDeltas,
		FBIMSymbolCollectionProxy& SymbolsCollection);
	static void PropagateAllDirtySymbols(UModumateDocument* Doc, int32& NextID, TArray<FDeltaPtr>& OutDeltas);

	static bool CreateNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* Group);
	static bool DetachSymbol(UModumateDocument* Doc, const AModumateObjectInstance* Group);

	static bool RemoveGroupMembersFromSymbol(const TSet<int32> GroupMembers, FBIMSymbolPresetData& SymbolData);
	static FVector SymbolAnchor(const FBIMSymbolPresetData& PresetData);

private:
	static TSet<int32> GetAllRootGroupsForSymbol(const UModumateDocument* Doc, const FBIMSymbolPresetData& SymbolData);
};
