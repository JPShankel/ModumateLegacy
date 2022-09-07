// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MOIState.h"
#include "ToolsAndAdjustments/Common/SelectedObjectToolMixin.h"

struct FMOIDeltaState;
enum class EMOIDeltaType: uint8;
struct FBIMSymbolPresetData;
struct FGraph3DDelta;

class FModumateSymbolDeltaStatics
{
public:
	static void GetDerivedDeltasFromDeltas(UModumateDocument* Doc, EMOIDeltaType DeltaType, const TArray<FDeltaPtr>& InDeltas, TArray<FDeltaPtr>& DerivedDeltas);
	static void CreateSymbolDerivedDeltasForMoi(UModumateDocument* Doc, const FMOIDeltaState& DeltaState, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas);
	static void GetDerivedDeltasForGraph3d(UModumateDocument* Doc, const FGraph3DDelta* GraphDelta, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas);
	static bool CreateDeltasForNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* SymbolGroup, TArray<FDeltaPtr>& OutDeltas);
	static bool CreatePresetDataForNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* SymbolGroup, FBIMSymbolPresetData& OutPreset);
	static bool CreateDeltasForNewSymbolInstance(UModumateDocument* Doc, int32 GroupID, int32& NextID, FBIMSymbolPresetData& Preset, const FTransform& Transform,
		TArray<FDeltaPtr>& OutDeltas);
	static void PropagateChangedSymbolInstances(UModumateDocument* Doc, int32& NextID, const TSet<int32>& GroupIDs, TArray<FDeltaPtr>& OutDeltas);
	static void PropagateChangedSymbolInstance(UModumateDocument* Doc, int32& NextID, int32 GroupID, TArray<FDeltaPtr>& OutDeltas);
	static bool CreateNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* Group);
	static bool DetachSymbol(UModumateDocument* Doc, const AModumateObjectInstance* Group);

private:
	static bool RemoveGroupMembersFromSymbol(const TSet<int32> GroupMembers, FBIMSymbolPresetData& SymbolData);
};
