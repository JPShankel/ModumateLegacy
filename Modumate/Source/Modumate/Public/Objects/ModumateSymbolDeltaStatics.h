// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MOIState.h"
#include "ToolsAndAdjustments/Common/SelectedObjectToolMixin.h"

struct FMOIDeltaState;
enum class EMOIDeltaType: uint8;
struct FBIMSymbolPresetData;
struct FGraph3DDelta;
struct FGraph2DDelta;

class FModumateSymbolDeltaStatics
{
public:
	static void GetDerivedDeltasFromDeltas(UModumateDocument* Doc, EMOIDeltaType DeltaType, const TArray<FDeltaPtr>& InDeltas, TArray<FDeltaPtr>& DerivedDeltas);
	static void CreateSymbolDerivedDeltasForMoi(UModumateDocument* Doc, const FMOIDeltaState& DeltaState, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas);
	static void GetDerivedDeltasForGraph3d(UModumateDocument* Doc, const FGraph3DDelta* GraphDelta, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas);
	static void GetDerivedDeltasForGraph2d(UModumateDocument* Doc, const FGraph2DDelta* GraphDelta, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas);
	static bool GetDerivedDeltasForPresetDelta(UModumateDocument* Doc, const FBIMPresetDelta* PresetDelta, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas);
	static bool CreateDeltasForNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* SymbolGroup, TArray<FDeltaPtr>& OutDeltas);
	static bool CreatePresetDataForSymbol(UModumateDocument* Doc, const AModumateObjectInstance* SymbolGroup, const FTransform& Transform, FBIMSymbolPresetData& OutPreset);
	static bool CreateDeltasForNewSymbolInstance(UModumateDocument* Doc, int32 GroupID, int32& NextID, FBIMSymbolPresetData& Preset, const FTransform& Transform,
		TArray<FDeltaPtr>& OutDeltas);
	static void PropagateChangedSymbolInstances(UModumateDocument* Doc, int32& NextID, const TSet<int32>& GroupIDs, TArray<FDeltaPtr>& OutDeltas);
	static void PropagateChangedSymbolInstance(UModumateDocument* Doc, int32& NextID, int32 GroupID, TArray<FDeltaPtr>& OutDeltas);

	static bool CreateNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* Group);
	static bool DetachSymbol(UModumateDocument* Doc, const AModumateObjectInstance* Group);

	static bool RemoveGroupMembersFromSymbol(const TSet<int32> GroupMembers, FBIMSymbolPresetData& SymbolData);
	static FVector SymbolAnchor(const FBIMSymbolPresetData& PresetData);

private:
	static TSet<int32> GetAllRootGroupsForSymbol(const UModumateDocument* Doc, const FBIMSymbolPresetData& SymbolData);
};
