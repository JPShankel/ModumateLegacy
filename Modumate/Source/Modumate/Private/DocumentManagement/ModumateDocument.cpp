// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateDocument.h"

#include "Algo/Accumulate.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"

#include "DocumentManagement/DocumentDelta.h"
#include "Drafting/DraftingManager.h"
#include "Drafting/ModumateDraftingView.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph3DDelta.h"
#include "Internationalization/Internationalization.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateMitering.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "Objects/ModumateSymbolDeltaStatics.h"
#include "ModumateCore/PlatformFunctions.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "DrawingDesigner/DrawingDesignerAutomation.h"
#include "Objects/MOIFactory.h"
#include "Objects/DesignOption.h"
#include "Objects/FaceHosted.h"
#include "Objects/SurfaceGraph.h"
#include "Objects/CameraView.h"
#include "Objects/MetaEdgeSpan.h"
#include "Objects/StructureLine.h"
#include "Objects/Mullion.h"
#include "Objects/MetaGraph.h"
#include "Objects/MetaPlaneSpan.h"
#include "Objects/PlaneHostedObj.h"
#include "Objects/EdgeHosted.h"
#include "Objects/CutPlane.h"
#include "Objects/MetaEdge.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "Quantities/QuantitiesManager.h"
#include "UI/DimensionManager.h"
#include "UI/EditModelUserWidget.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/DrawingDesigner/DrawingDesignerWebBrowserWidget.h"
#include "UI/Custom/ModumateWebBrowser.h"
#include "UI/ModumateSettingsMenu.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelInputAutomation.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/Modumate.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/ModumateObjectComponent.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "UnrealClasses/SkyActor.h"
#include "ModumateCore/EnumHelpers.h"
#include "ModumateCore/ModumateCameraViewStatics.h"
#include "ModumateCore/ModumateStats.h"
#include "Objects/EdgeDetailObj.h"

#include "DrawingDesigner/DrawingDesignerDocumentDelta.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"
#include "DrawingDesigner/DrawingDesignerRenderControl.h"
#include "UnrealClasses/EditModelInputHandler.h"

#define LOCTEXT_NAMESPACE "ModumateDocument"

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Objects Cleaned"), STAT_ModumateNumObjectsCleaned, STATGROUP_Modumate)

const FName UModumateDocument::DocumentHideRequestTag(TEXT("DocumentHide"));


// Set up a reasonable default for infinite loop detection while cleaning objects and resolving dependencies.
const int32 UModumateDocument::CleanIterationSafeguard = 128;

// 32-bit object IDs with 4 user index bits and 1 negation bit caps us out at 16 simultaneous users and up to ~137 million lifetime object IDs per user for a given project.
// TODO: These could be constexpr without a .cpp definition, but clang 10.0.0.1 for Linux generates `ld.lld: error: undefined symbol` errors for these if we do so,
// which may be fixed if we explicitly enable C++17 in our module and/or target's CppCompileEnvironment.CppStandardVersion.
const int32 UModumateDocument::UserIdxBits = 4;
const int32 UModumateDocument::MaxUserIdx = (1 << UserIdxBits) - 1;
const int32 UModumateDocument::LocalObjIDBits = 32 - UserIdxBits - 1;
const int32 UModumateDocument::ObjIDUserMask = MaxUserIdx << LocalObjIDBits;
const int32 UModumateDocument::LocalObjIDMask = ~ObjIDUserMask;

UModumateDocument::UModumateDocument()
	: Super()
	, NextID(1)
	, PrePreviewNextID(1)
	, ReservingObjectID(MOD_ID_NONE)
	, bApplyingPreviewDeltas(false)
	, bFastClearingPreviewDeltas(false)
	, bSlowClearingPreviewDeltas(false)
	, DrawingDesignerRenderControl(new FDrawingDesignerRenderControl(this))
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ModumateDocument"));

	//Set Default Values
	//Values will be deprecated in favor of instance-level overrides for assemblies
	DefaultWallHeight = 243.84f;
	ElevationDelta = 0;
	DefaultWindowHeight = 91.44f;
	DefaultDoorHeight = 0.f;
}

UModumateDocument::UModumateDocument(FVTableHelper& Helper)
{ }

UModumateDocument::~UModumateDocument() = default;

void UModumateDocument::SetLocalUserInfo(const FString& InLocalUserID, int32 InLocalUserIdx)
{
	CachedLocalUserID = InLocalUserID;
	CachedLocalUserIdx = InLocalUserIdx;
}

void UModumateDocument::ApplyInvertedDeltas(UWorld* World, const TArray<FDeltaPtr>& Deltas)
{
	for (int32 i = Deltas.Num() - 1; i >= 0; --i)
	{
		Deltas[i]->MakeInverse()->ApplyTo(this, World);
	}
}

void UModumateDocument::PerformUndoRedo(UWorld* World, TArray<TSharedPtr<UndoRedo>>& FromBuffer, TArray<TSharedPtr<UndoRedo>>& ToBuffer)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateDocumentUndoRedo);

	if (FromBuffer.Num() > 0)
	{
		TSharedPtr<UndoRedo> ur = FromBuffer.Last();
		FromBuffer.RemoveAt(FromBuffer.Num() - 1);
		ApplyInvertedDeltas(World, ur->Deltas);

		Algo::Reverse(ur->Deltas);
		for (int32 i = 0; i < ur->Deltas.Num(); ++i)
		{
			auto& delta = ur->Deltas[i];
			if (delta.IsValid())
			{
				ur->Deltas[i] = delta->MakeInverse();
			}
		}
		ToBuffer.Add(ur);

#if WITH_EDITOR
		int32 fromBufferSize = FromBuffer.Num();
		int32 toBufferSize = ToBuffer.Num();
#endif

		PostApplyDeltas(World, true, true);

#if WITH_EDITOR
		ensureAlways(fromBufferSize == FromBuffer.Num());
		ensureAlways(toBufferSize == ToBuffer.Num());
#endif
	}
}

void UModumateDocument::UpdateSpanData(const FMOIDeltaState& SpanDelta)
{
	int32 spanID = SpanDelta.NewState.ID;
	if (SpanDelta.DeltaType == EMOIDeltaType::Mutate || SpanDelta.DeltaType == EMOIDeltaType::Destroy)
	{
		TArray<int32>* elementIDs = SpanToGraphElementsMap.Find(spanID);
		if (ensureAlways(elementIDs))
		{
			for (int32 id : *elementIDs)
			{
				GraphElementToSpanMap.Remove(id, spanID);
			}
			SpanToGraphElementsMap.Remove(spanID);
		}
	}

	if (SpanDelta.DeltaType == EMOIDeltaType::Mutate || SpanDelta.DeltaType == EMOIDeltaType::Create)
	{
		FMOIMetaEdgeSpanData edgeData;
		FMOIMetaPlaneSpanData faceData;
		TArray<int32>* newGraphIDs;
		if (SpanDelta.NewState.ObjectType == EObjectType::OTMetaEdgeSpan)
		{
			newGraphIDs = SpanDelta.NewState.CustomData.LoadStructData(edgeData) ? &edgeData.GraphMembers : nullptr;
		}
		else
		{
			newGraphIDs = SpanDelta.NewState.CustomData.LoadStructData(faceData) ? &faceData.GraphMembers : nullptr;
		}

		if (ensure(newGraphIDs))
		{
			for (int id : *newGraphIDs)
			{
				GraphElementToSpanMap.Add(id, spanID);
			}
			SpanToGraphElementsMap.Add(spanID, MoveTemp(*newGraphIDs));
		}
	}
}

void UModumateDocument::UpdateSpanData(const AModumateObjectInstance* Moi)
{
	const int32 spanID = Moi->ID;
	const TArray<int32>* graphIDs = nullptr;
	if (Moi->GetObjectType() == EObjectType::OTMetaEdgeSpan)
	{
		graphIDs = &Cast<AMOIMetaEdgeSpan>(Moi)->InstanceData.GraphMembers;
	}
	else if (Moi->GetObjectType() == EObjectType::OTMetaPlaneSpan)
	{
		graphIDs = &Cast<AMOIMetaPlaneSpan>(Moi)->InstanceData.GraphMembers;
	}
	else
	{
		ensure(false);
		return;
	}

	SpanToGraphElementsMap.Add(spanID, *graphIDs);
	for (int32 id : *graphIDs)
	{
		GraphElementToSpanMap.Add(id, spanID);
	}
}

void UModumateDocument::Undo(UWorld *World)
{
	auto* localPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	auto* controller = localPlayer ? Cast<AEditModelPlayerController>(localPlayer->GetPlayerController(World)) : nullptr;
	if (!ensure(controller && controller->EMPlayerState))
	{
		return;
	}

	if (controller->EMPlayerState->IsNetMode(NM_Client))
	{
		controller->EMPlayerState->TryUndo();
	}
	else if (ensureAlways(!InUndoRedoMacro()))
	{
		PerformUndoRedo(World, UndoBuffer, RedoBuffer);
	}
}

void UModumateDocument::Redo(UWorld *World)
{
	auto* localPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	auto* controller = localPlayer ? Cast<AEditModelPlayerController>(localPlayer->GetPlayerController(World)) : nullptr;
	if (!ensure(controller && controller->EMPlayerState))
	{
		return;
	}

	if (controller->EMPlayerState->IsNetMode(NM_Client))
	{
		controller->EMPlayerState->TryRedo();
	}
	else if (ensureAlways(!InUndoRedoMacro()))
	{
		PerformUndoRedo(World, RedoBuffer, UndoBuffer);
	}
}

bool UModumateDocument::GetUndoRecordsFromClient(UWorld* World, const FString& UserID, TArray<uint32>& OutUndoRecordHashes) const
{
	OutUndoRecordHashes.Reset();

	if (!ensure(World && !World->IsNetMode(NM_Client)))
	{
		return false;
	}

	int32 numVerifiedDeltasRecords = VerifiedDeltasRecords.Num();
	int32 firstDeltaIdx = INDEX_NONE;

	for (int32 recordIdx = numVerifiedDeltasRecords - 1; recordIdx >= 0; --recordIdx)
	{
		auto& deltasRecord = VerifiedDeltasRecords[recordIdx];
		if (deltasRecord.OriginUserID == UserID && !deltasRecord.bDDCleaningDelta)
		{
			firstDeltaIdx = recordIdx;
			break;
		}
	}

	if (firstDeltaIdx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("There aren't any DeltasRecords from %s to undo!"), *UserID);
		return false;
	}

	TSet<int32> affectedObjects;
	TSet<FGuid> affectedPresets;
	FBox affectedBounds(EForceInit::ForceInitToZero);

	auto& firstDeltasRecord = VerifiedDeltasRecords[firstDeltaIdx];

	for (int32 recordIdx = firstDeltaIdx; recordIdx < numVerifiedDeltasRecords; ++recordIdx)
	{
		auto& deltasRecord = VerifiedDeltasRecords[recordIdx];
		if ((recordIdx == firstDeltaIdx) || deltasRecord.ConflictsWithResults(affectedObjects, affectedPresets, &affectedBounds))
		{
			OutUndoRecordHashes.Add(deltasRecord.TotalHash);
			deltasRecord.GatherResults(affectedObjects, affectedPresets, &affectedBounds);
		}
	}

	return true;
}

void UModumateDocument::BeginUndoRedoMacro()
{
	UndoRedoMacroStack.Push(UndoBuffer.Num());
}

void UModumateDocument::EndUndoRedoMacro()
{
	if (!InUndoRedoMacro())
	{
		return;
	}

	int32 start = UndoRedoMacroStack.Pop();

	if (start >= UndoBuffer.Num())
	{
		return;
	}

	TArray<TSharedPtr<UndoRedo>> section;
	for (int32 i = start; i < UndoBuffer.Num(); ++i)
	{
		section.Add(UndoBuffer[i]);
	}

	TSharedPtr<UndoRedo> ur = MakeShared<UndoRedo>();
	for (auto s : section)
	{
		ur->Deltas.Append(s->Deltas);
	}

	UndoBuffer.SetNum(start, true);

	UndoBuffer.Add(ur);
}

bool UModumateDocument::InUndoRedoMacro() const
{
	return (UndoRedoMacroStack.Num() > 0);
}

void UModumateDocument::SetDefaultWallHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultWallHeight"));
	ClearRedoBuffer();
	float orig = DefaultWallHeight;
	DefaultWallHeight = height;
}

void UModumateDocument::SetDefaultRailHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultRailHeight"));
	ClearRedoBuffer();
	float orig = DefaultRailHeight;
	DefaultRailHeight = height;
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultRailHeight::Redo"));
}

void UModumateDocument::SetDefaultCabinetHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultCabinetHeight"));
	ClearRedoBuffer();
	float orig = DefaultCabinetHeight;
	DefaultCabinetHeight = height;
}

void UModumateDocument::SetDefaultDoorHeightWidth(float height, float width)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultDoorHeightWidth"));
	ClearRedoBuffer();
	float origHeight = DefaultDoorHeight;
	float origWidth = DefaultDoorWidth;
	DefaultDoorHeight = height;
	DefaultDoorWidth = width;
}

void UModumateDocument::SetDefaultWindowHeightWidth(float height, float width)
{
	float origHeight = DefaultWindowHeight;
	float origWidth = DefaultWindowWidth;
	DefaultWindowHeight = height;
	DefaultWindowWidth = width;
}

void UModumateDocument::SetDefaultJustificationZ(float newValue)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationZ"));
	ClearRedoBuffer();
	float orig = DefaultJustificationZ;
	DefaultJustificationZ = newValue;
}

void UModumateDocument::SetDefaultJustificationXY(float newValue)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationXY"));
	ClearRedoBuffer();
	float orig = DefaultJustificationXY;
	DefaultJustificationXY = newValue;
}

void UModumateDocument::SetAssemblyForObjects(UWorld *world,TArray<int32> ids, const FBIMAssemblySpec &assembly)
{
	if (ids.Num() == 0)
	{
		return;
	}

	auto delta = MakeShared<FMOIDelta>();
	for (auto id : ids)
	{
		AModumateObjectInstance* obj = GetObjectById(id);
		if (obj != nullptr)
		{
			auto& newState = delta->AddMutationState(obj);
			newState.AssemblyGUID = assembly.UniqueKey();
		}
	}

	ApplyDeltas({ delta }, world);
}

void UModumateDocument::RestoreDeletedObjects(const TArray<int32> &ids)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::RestoreDeletedObjects"));
	if (ids.Num() == 0)
	{
		return;
	}

	ClearRedoBuffer();
	TArray<AModumateObjectInstance*> oldObs;
	for (auto id : ids)
	{
		AModumateObjectInstance *oldOb = TryGetDeletedObject(id);
		if (oldOb != nullptr)
		{
			RestoreObjectImpl(oldOb);
			oldObs.AddUnique(oldOb);
		}
	}
}

bool UModumateDocument::DeleteObjectImpl(AModumateObjectInstance *ObjToDelete)
{
	if (ObjToDelete && !ObjToDelete->IsDestroyed())
	{
		// Store off the connected objects, in case they will be affected by this deletion
		TArray<AModumateObjectInstance *> connectedMOIs;
		ObjToDelete->GetConnectedMOIs(connectedMOIs);
		int32 objID = ObjToDelete->ID;
		ObjToDelete->MarkDirty(EObjectDirtyFlags::Structure);  // Dirty any children

		bool bKeepDeletedObj = !bSlowClearingPreviewDeltas;
		bool bFullyDestroy = !bFastClearingPreviewDeltas && !bApplyingPreviewDeltas;

		if (bKeepDeletedObj)
		{
			// If we already had an object of the same ID in the deleted list, then we want to preserve it less than the current object being deleted.
			if (DeletedObjects.Contains(ObjToDelete->ID))
			{
				ensure(bFastClearingPreviewDeltas || bSlowClearingPreviewDeltas);
				DeletedObjects.FindAndRemoveChecked(ObjToDelete->ID)->Destroy();
			}

			ObjToDelete->DestroyMOI(bFullyDestroy);
			DeletedObjects.Add(ObjToDelete->ID, ObjToDelete);
		}
		else
		{
			ObjToDelete->Destroy();
		}

		ObjectInstanceArray.Remove(ObjToDelete);
		ObjectsByID.Remove(objID);
		ObjectsByType.FindOrAdd(ObjToDelete->GetObjectType()).Remove(objID);

		// Update mitering, visibility & collision enabled on neighbors, in case they were dependent on this MOI.
		for (AModumateObjectInstance *connectedMOI : connectedMOIs)
		{
			connectedMOI->MarkDirty(EObjectDirtyFlags::Mitering | EObjectDirtyFlags::Visuals);
		}

		if (bTrackingDeltaObjects)
		{
			DeltaAffectedObjects.FindOrAdd(EMOIDeltaType::Create).Remove(ObjToDelete->ID);
			DeltaAffectedObjects.FindOrAdd(EMOIDeltaType::Destroy).Add(ObjToDelete->ID);
		}

		UpdateWebMOIs(ObjToDelete->GetObjectType());

		return true;
	}

	return false;
}

bool UModumateDocument::RestoreObjectImpl(AModumateObjectInstance *obj)
{
	if (obj && (DeletedObjects.FindRef(obj->ID) == obj))
	{
		DeletedObjects.Remove(obj->ID);
		ObjectInstanceArray.AddUnique(obj);
		ObjectsByID.Add(obj->ID, obj);
		ObjectsByType.FindOrAdd(obj->GetObjectType()).Add(obj->ID);
		obj->RestoreMOI();
		UpdateWebMOIs(obj->GetObjectType());

		return true;
	}

	return false;
}

AModumateObjectInstance* UModumateDocument::CreateOrRestoreObj(UWorld* World, const FMOIStateData& StateData)
{
	// Check to make sure NextID represents the next highest ID we can allocate to a new object.
	// For multiplayer clients, they will only allocate IDs in a certain block, so we can ignore IDs outside of that block.
	int32 localObjID, userIdx;
	SplitMPObjID(StateData.ID, localObjID, userIdx);
	if ((userIdx == CachedLocalUserIdx) && (StateData.ID >= NextID))
	{
		NextID = StateData.ID + 1;
	}

	// See if we can restore an identical object that was previously deleted (i.e. by undo, or by clearing a preview delta that created an object)
	AModumateObjectInstance* deletedObj = TryGetDeletedObject(StateData.ID);
	if (deletedObj)
	{
		if ((deletedObj->GetStateData() == StateData) && RestoreObjectImpl(deletedObj))
		{
			return deletedObj;
		}

		// If there was a deleted object with the same ID as our new object, but cannot be restored for the given state data,
		// then fully delete it since its ID must no longer be safely referenced.
		DeletedObjects.FindAndRemoveChecked(StateData.ID)->Destroy();
	}

	if (!ensureAlwaysMsgf(!ObjectsByID.Contains(StateData.ID),
		TEXT("Tried to create a new object with the same ID (%d) as an existing one!"), StateData.ID))
	{
		return nullptr;
	}

	UClass* moiClass = FMOIFactory::GetMOIClass(StateData.ObjectType);
	FActorSpawnParameters moiSpawnParams;
	moiSpawnParams.Owner = World->GetGameState<AEditModelGameState>();
	moiSpawnParams.bNoFail = true;
	AModumateObjectInstance* newObj = World->SpawnActor<AModumateObjectInstance>(moiClass, moiSpawnParams);
	if (!ensure(newObj))
	{
		return newObj;
	}

	ObjectInstanceArray.AddUnique(newObj);
	ObjectsByID.Add(StateData.ID, newObj);
	ObjectsByType.FindOrAdd(StateData.ObjectType).Add(StateData.ID);

	newObj->SetStateData(StateData);
	newObj->PostCreateObject(true);

	if (bTrackingDeltaObjects)
	{
		DeltaAffectedObjects.FindOrAdd(EMOIDeltaType::Destroy).Remove(newObj->ID);
		DeltaAffectedObjects.FindOrAdd(EMOIDeltaType::Create).Add(newObj->ID);
	}

	return newObj;
}

bool UModumateDocument::ReconcileRemoteDeltas(const FDeltasRecord& DeltasRecord, UWorld* World, FDeltasRecord& OutReconciledRecord, uint32& OutVerifiedDocHash)
{
	OutReconciledRecord = FDeltasRecord();
	OutVerifiedDocHash = GetLatestVerifiedDocHash();

	// For now, only reject deltas that come from clients that are out of date, which will eventually receive the deltas that *were* verified.
	// TODO: handle delta merging, conflict resolution, rollbacks, etc. so that deltas from clients that are out of date can still be combined
	// with verified deltas if they don't affect the same data in conflicting ways. The OutCorrections parameter would be used to fix the order
	// of deltas on the client that's out of date.
	if (DeltasRecord.PrevDocHash == OutVerifiedDocHash)
	{
		return true;
	}
	else
	{
		// NOTE: for stability, if deltas that actually conflict, but aren't reported as conflicting, are corrupting the document,
		// this can universally return false. This will negatively impact the experience of laggy clients, because they'll lose work.

		// Gather the set of all objects, presets, and locations affected by the deltas that have been verified since the incoming delta was made.
		TSet<int32> affectedObjects;
		TSet<FGuid> affectedPresets;
		FBox affectedBounds(EForceInit::ForceInitToZero);

		int32 numVerifiedRecords = VerifiedDeltasRecords.Num();
		for (int32 i = numVerifiedRecords - 1; i >= 0; --i)
		{
			const auto& verifiedRecord = VerifiedDeltasRecords[i];
			if (verifiedRecord.TotalHash == DeltasRecord.PrevDocHash)
			{
				break;
			}
			else
			{
				verifiedRecord.GatherResults(affectedObjects, affectedPresets, &affectedBounds);
			}
		}

		// If the incoming delta had no effects that conflict with any of the verified deltas, then make a reconciled delta on top of the latest verified one.
		if (!DeltasRecord.ConflictsWithResults(affectedObjects, affectedPresets, &affectedBounds))
		{
			OutReconciledRecord = FDeltasRecord(DeltasRecord.RawDeltaPtrs, DeltasRecord.OriginUserID, OutVerifiedDocHash);
		}

		return false;
	}
}

bool UModumateDocument::RollBackUnverifiedDeltas(uint32 VerifiedDocHash, UWorld* World)
{
	bool bRolledBackDeltas = false;

	while (UnverifiedDeltasRecords.Num() > 0)
	{
		auto& unverifiedDeltasRecord = UnverifiedDeltasRecords.Last();

		// Undo all unverified deltas, since we wouldn't expect an earlier record's verification to lag behind a later record's rollback.
		ApplyInvertedDeltas(World, unverifiedDeltasRecord.RawDeltaPtrs);
		UnverifiedDeltasRecords.RemoveAt(UnverifiedDeltasRecords.Num() - 1);
		bRolledBackDeltas = true;
	}

	uint32 localVerifiedDocHash = GetLatestVerifiedDocHash();
	if (!ensure(VerifiedDocHash == localVerifiedDocHash))
	{
		UE_LOG(LogTemp, Error, TEXT("The local verified doc hash %08x disagrees with the expected value: %08x!"), localVerifiedDocHash, VerifiedDocHash);
	}

	return bRolledBackDeltas;
}

bool UModumateDocument::ApplyRemoteDeltas(const FDeltasRecord& DeltasRecord, UWorld* World, bool bApplyOwnDeltas)
{
	uint32 latestVerifiedDocHash = GetLatestVerifiedDocHash();
	if (!ensure(DeltasRecord.PrevDocHash == latestVerifiedDocHash))
	{
		// TODO: handle desync
		return false;
	}

	bool bReceivingOwnDeltas = (DeltasRecord.OriginUserID == CachedLocalUserID);
	if (!bReceivingOwnDeltas || bApplyOwnDeltas)
	{
		// If we receive remote deltas from another user while we still have some unverified deltas,
		// then it means we didn't send them to the server fast enough;
		// otherwise, another user's deltas should've caused the server to tell us to roll back directly.
		if (RollBackUnverifiedDeltas(latestVerifiedDocHash, World))
		{
			UE_LOG(LogTemp, Log, TEXT("Rolled back local deltas received before remote delta %08x from user ID %s"),
				DeltasRecord.TotalHash, *DeltasRecord.OriginUserID);
		}
	}

	VerifiedDeltasRecords.Add(DeltasRecord);

	// For clients receiving their own deltas, move an entry from the unverified list to the undo-redo list now that we've verified the deltas,
	// and we know that all clients will have the same order.
	ULocalPlayer* localPlayer = World->GetFirstLocalPlayerFromController();
	APlayerController* localController = localPlayer ? localPlayer->GetPlayerController(World) : nullptr;
	if (localController && localController->IsNetMode(NM_Client) && bReceivingOwnDeltas && !bApplyOwnDeltas)
	{
		uint32 recordHash = DeltasRecord.TotalHash;
		int32 unverifiedIdx = UnverifiedDeltasRecords.IndexOfByPredicate([recordHash](const FDeltasRecord& UnverifiedDeltasRecord)
			{ return (UnverifiedDeltasRecord.TotalHash == recordHash); });

		// If we're receiving our own deltas, then we wait until now to remove them from the unverified list.
		if (unverifiedIdx != INDEX_NONE)
		{
			UnverifiedDeltasRecords.RemoveAt(unverifiedIdx);
		}
		// But if we already removed them before (i.e. from being rolled back for being out of date), then we want to reapply the reconciled deltas here.
		else
		{
			bApplyOwnDeltas = true;
		}
	}

	for (auto& deltaPtr : DeltasRecord.RawDeltaPtrs)
	{
		if ((!bReceivingOwnDeltas || bApplyOwnDeltas) && ensure(deltaPtr))
		{
			deltaPtr->ApplyTo(this, World);
		}
	}

	PostApplyDeltas(World, true, true);

	return true;
}

bool UModumateDocument::ApplyRemoteUndo(UWorld* World, const FString& UndoingUserID, const TArray<uint32>& UndoRecordHashes)
{
	int32 numUndoRecords = UndoRecordHashes.Num();
	if (!ensure(numUndoRecords > 0))
	{
		return false;
	}

	// If we receive an undo request (either originating from us, or any other user) while we still have some unverified deltas,
	// then it means we didn't send them to the server fast enough.
	if (RollBackUnverifiedDeltas(GetLatestVerifiedDocHash(), World))
	{
		UE_LOG(LogTemp, Log, TEXT("Rolled back local deltas received before remote undo from user ID %s"), *UndoingUserID);
	}

	UE_LOG(LogTemp, Log, TEXT("Undoing %d deltas from user %s"), numUndoRecords, *UndoingUserID);

	for (int32 recordIDIdx = numUndoRecords - 1; recordIDIdx >= 0; --recordIDIdx)
	{
		int32 recordIdx = FindDeltasRecordIdxByHash(UndoRecordHashes[recordIDIdx]);

		if (!ensure(recordIdx != INDEX_NONE))
		{
			return false;
		}

		auto& undoRecord = VerifiedDeltasRecords[recordIdx];
		ApplyInvertedDeltas(World, undoRecord.RawDeltaPtrs);

		VerifiedDeltasRecords.RemoveAt(recordIdx);

		// After removing the final (oldest) DeltasRecord, re-hash the subsequent deltas
		if (recordIDIdx == 0)
		{
			int32 numRecords = VerifiedDeltasRecords.Num();
			for (int32 rehashIdx = recordIdx; rehashIdx < numRecords; ++rehashIdx)
			{
				int32 prevRecordIdx = rehashIdx - 1;
				uint32 prevDocHash = VerifiedDeltasRecords.IsValidIndex(prevRecordIdx) ? VerifiedDeltasRecords[prevRecordIdx].TotalHash : 0;

				auto& rehashRecord = VerifiedDeltasRecords[rehashIdx];
				rehashRecord.PrevDocHash = prevDocHash;
				rehashRecord.ComputeHash();
			}
		}
	}

	PostApplyDeltas(World, true, true);

	return true;
}

bool UModumateDocument::ApplyMOIDelta(const FMOIDelta& Delta, UWorld* World)
{
	for (auto& deltaState : Delta.States)
	{
		auto& targetState = deltaState.NewState;

		switch (deltaState.DeltaType)
		{
		case EMOIDeltaType::Create:
		{
			int32 localObjID, userIdx;
			SplitMPObjID(targetState.ID, localObjID, userIdx);

			AModumateObjectInstance* newInstance = CreateOrRestoreObj(World, targetState);
			if (ensureAlways(newInstance) && ensure(newInstance->ID == targetState.ID) &&
				(userIdx == CachedLocalUserIdx) && (NextID <= newInstance->ID))
			{
				NextID = newInstance->ID + 1;
			}
		}
		break;

		case EMOIDeltaType::Destroy:
		{
			DeleteObjectImpl(GetObjectById(targetState.ID));
		}
		break;

		case EMOIDeltaType::Mutate:
		{
			AModumateObjectInstance* MOI = GetObjectById(targetState.ID);

			if (ensureAlways(MOI))
			{
				if (targetState.ParentID != MOI->GetParentID())
				{
					MOI->SetParentID(targetState.ParentID);
				}
				
				MOI->SetStateData(targetState);
			}
			else
			{
				return false;
			}
		}
		break;

		default:
			ensureAlways(false);
			return false;
		};

		if (targetState.ObjectType == EObjectType::OTMetaEdgeSpan || targetState.ObjectType == EObjectType::OTMetaPlaneSpan)
		{
			UpdateSpanData(deltaState);
		}
	}

	return true;
}

void UModumateDocument::ApplyGraph2DDelta(const FGraph2DDelta &Delta, UWorld *World)
{
	TSharedPtr<FGraph2D> targetSurfaceGraph;

	switch (Delta.DeltaType)
	{
	case EGraph2DDeltaType::Add:
		if (!ensure(!SurfaceGraphs.Contains(Delta.ID)))
		{
			return;
		}
		targetSurfaceGraph = SurfaceGraphs.Add(Delta.ID, MakeShared<FGraph2D>(Delta.ID, Delta.Epsilon));
		break;
	case EGraph2DDeltaType::Edit:
	case EGraph2DDeltaType::Remove:
		targetSurfaceGraph = SurfaceGraphs.FindRef(Delta.ID);
		break;
	default:
		break;
	}

	if (!ensure(targetSurfaceGraph.IsValid()))
	{
		return;
	}

	// update graph itself
	if (!targetSurfaceGraph->ApplyDelta(Delta))
	{
		return;
	}

	int32 surfaceGraphID = targetSurfaceGraph->GetID();
	AModumateObjectInstance* surfaceGraphObj = GetObjectById(surfaceGraphID);
	EObjectType vertexType = EObjectType::OTNone, edgeType = EObjectType::OTNone, polygonType = EObjectType::OTNone;

	if (ensure(surfaceGraphObj))
	{
		// mark the surface graph dirty, so that its children can update their visual and world-coordinate-space representations
		surfaceGraphObj->MarkDirty(EObjectDirtyFlags::All);

		switch (surfaceGraphObj->GetObjectType())
		{
		case EObjectType::OTSurfaceGraph:
			vertexType = EObjectType::OTSurfaceVertex;
			edgeType = EObjectType::OTSurfaceEdge;
			polygonType = EObjectType::OTSurfacePolygon;
			break;
		case EObjectType::OTTerrain:
			vertexType = EObjectType::OTTerrainVertex;
			edgeType = EObjectType::OTTerrainEdge;
			polygonType = EObjectType::OTTerrainPolygon;
			break;
		default:
			ensureMsgf(false, TEXT("Unsupported Graph2D object type!"));
			return;
		}

		// add objects
		for (auto& kvp : Delta.PolygonAdditions)
		{
			// It would be ideal to only create SurfacePolgyon objects for interior polygons, but if we don't then the graph will try creating
			// deltas that use IDs that the document will try to re-purpose for other objects.
			// TODO: allow allocating IDs from graph deltas in a way that the document can't use them
			CreateOrRestoreObj(World, FMOIStateData(kvp.Key, polygonType, surfaceGraphID));
		}

		for (auto& kvp : Delta.EdgeAdditions)
		{
			CreateOrRestoreObj(World, FMOIStateData(kvp.Key, edgeType, surfaceGraphID));
		}

		for (auto& kvp : Delta.VertexAdditions)
		{
			CreateOrRestoreObj(World, FMOIStateData(kvp.Key, vertexType, surfaceGraphID));
		}
	}

	// delete surface objects
	for (auto &kvp : Delta.VertexDeletions)
	{
		DeleteObjectImpl(GetObjectById(kvp.Key));
	}

	for (auto &kvp : Delta.EdgeDeletions)
	{
		DeleteObjectImpl(GetObjectById(kvp.Key));
	}

	for (auto &kvp : Delta.PolygonDeletions)
	{
		DeleteObjectImpl(GetObjectById(kvp.Key));
	}

	// Use the surface graph modified flags, to decide which surface graph object MOIs need to be updated
	TArray<int32> modifiedVertices, modifiedEdges, modifiedPolygons;
	if (targetSurfaceGraph->ClearModifiedObjects(modifiedVertices, modifiedEdges, modifiedPolygons))
	{
		for (int32 polygonID : modifiedPolygons)
		{
			AModumateObjectInstance* polygonObj = GetObjectById(polygonID);
			if (ensureAlways(polygonObj && (polygonObj->GetObjectType() == polygonType)))
			{
				polygonObj->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}

		for (int32 edgeID : modifiedEdges)
		{
			AModumateObjectInstance *edgeObj = GetObjectById(edgeID);
			if (ensureAlways(edgeObj && (edgeObj->GetObjectType() == edgeType)))
			{
				edgeObj->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}

		for (int32 vertexID : modifiedVertices)
		{
			AModumateObjectInstance* vertexObj = GetObjectById(vertexID);
			if (ensureAlways(vertexObj && (vertexObj->GetObjectType() == vertexType)))
			{
				vertexObj->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}
	}

	if (Delta.DeltaType == EGraph2DDeltaType::Remove)
	{
		ensureAlwaysMsgf(targetSurfaceGraph->IsEmpty(), TEXT("Tried to delete a Graph2D that wasn't already emptied by Graph2DDeltas!"));
		SurfaceGraphs.Remove(Delta.ID);
	}
}

void UModumateDocument::ApplyGraph3DDelta(const FGraph3DDelta &Delta, UWorld *World)
{
	const int32 graphID = Delta.GraphID;
	ensureAlways(graphID != MOD_ID_NONE);
	if (Delta.DeltaType == EGraph3DDeltaType::Add && ensure(!VolumeGraphs.Contains(graphID)) )
	{
		VolumeGraphs.Add(graphID, MakeShared<FGraph3D>(graphID));
		return;
	}
	else if (Delta.DeltaType == EGraph3DDeltaType::Remove && ensure(VolumeGraphs.Contains(graphID)) && ensure(graphID != GetRootVolumeGraphID()) )
	{
		const auto& allObjects = VolumeGraphs[graphID]->GetAllObjects();
		// Deleted graphs should be empty.
		ensureAlways(allObjects.Num() == 0);
		for (const auto& item: allObjects)
		{
			GraphElementsToGraph3DMap.Remove(item.Key);
		}
		if (graphID == GetActiveVolumeGraphID())
		{   // Deletion of active group - move to root group.
			UModumateObjectStatics::GetGroupIdsForGroupChange(this, GetRootVolumeGraphID(), ChangedGroupIDs);
			SetActiveVolumeGraphID(GetRootVolumeGraphID());
		}
		VolumeGraphs.Remove(graphID);
		return;
	}

	// TODO: the graph may need an understanding of "net" deleted objects
	// objects that are deleted, but their metadata (like hosted obj) is not
	// passed on to another object
	// if it is passed on to another object, the MarkDirty here is redundant

	FGraph3D* volumeGraph = GetVolumeGraph(graphID);

	for (auto& kvp : Delta.VertexDeletions)
	{
		auto vertex = volumeGraph->FindVertex(kvp.Key);
		if (vertex == nullptr)
		{
			continue;
		}
	}
	for (auto& kvp : Delta.EdgeDeletions)
	{
		auto edge = volumeGraph->FindEdge(kvp.Key);
		if (edge == nullptr)
		{
			continue;
		}
	}
	for (auto& kvp : Delta.FaceDeletions)
	{
		auto face = volumeGraph->FindFace(kvp.Key);
		if (face == nullptr)
		{
			continue;
		}
	}

	volumeGraph->ApplyDelta(Delta);

	for (auto &kvp : Delta.VertexAdditions)
	{
		check(!GraphElementsToGraph3DMap.Contains(kvp.Key));
		GraphElementsToGraph3DMap.Add(kvp.Key) = graphID;
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTMetaVertex));
	}

	for (auto &kvp : Delta.VertexDeletions)
	{
		AModumateObjectInstance *deletedVertexObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedVertexObj))
		{
			DeleteObjectImpl(deletedVertexObj);
			check(GraphElementsToGraph3DMap.Contains(kvp.Key));
			GraphElementsToGraph3DMap.Remove(kvp.Key);
		}
	}

	for (auto &kvp : Delta.EdgeAdditions)
	{
		check(!GraphElementsToGraph3DMap.Contains(kvp.Key));
		GraphElementsToGraph3DMap.Add(kvp.Key) = graphID;
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTMetaEdge));
	}

	for (auto &kvp : Delta.EdgeDeletions)
	{
		AModumateObjectInstance *deletedEdgeObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedEdgeObj))
		{
			DeleteObjectImpl(deletedEdgeObj);
			check(GraphElementsToGraph3DMap.Contains(kvp.Key));
			GraphElementsToGraph3DMap.Remove(kvp.Key);
		}
	}

	for (auto &kvp : Delta.FaceAdditions)
	{
		check(!GraphElementsToGraph3DMap.Contains(kvp.Key));
		GraphElementsToGraph3DMap.Add(kvp.Key) = graphID;
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTMetaPlane));
	}

	for (auto &kvp : Delta.FaceDeletions)
	{
		AModumateObjectInstance *deletedFaceObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedFaceObj))
		{
			DeleteObjectImpl(deletedFaceObj);
			check(GraphElementsToGraph3DMap.Contains(kvp.Key));
			GraphElementsToGraph3DMap.Remove(kvp.Key);
		}
	}
}

bool UModumateDocument::ApplyPresetDelta(const FBIMPresetDelta& PresetDelta, UWorld* World)
{
	/*
	* Valid GUID->Valid GUID == Add or Update existing preset
	* Valid GUID->Invalid GUID == Delete existing preset
	*/
	
	if (PresetDelta.NewState.GUID.IsValid())
	{
		bool bIsNewPreset = !BIMPresetCollection.ContainsNonCanon(PresetDelta.NewState.GUID);


		FBIMPresetInstance CollectionPreset;
		BIMPresetCollection.AddOrUpdatePreset(PresetDelta.NewState, CollectionPreset);
		BIMPresetCollection.ProcessPreset(CollectionPreset.GUID);

		// Find all affected presets and update affected assemblies
		TSet<FGuid> affectedPresets {PresetDelta.NewState.GUID};
		BIMPresetCollection.GetAllAncestorPresets(CollectionPreset.GUID, affectedPresets);
		if (CollectionPreset.ObjectType != EObjectType::OTNone)
		{
			affectedPresets.Add(CollectionPreset.GUID);
		}

		TSet<FGuid> affectedAssemblies;
		for (auto& affectedPreset : affectedPresets)
		{
			// Skip presets that don't define assemblies
			FBIMPresetInstance* preset = BIMPresetCollection.PresetFromGUID(affectedPreset);
			if (!ensureAlways(preset != nullptr) || preset->ObjectType == EObjectType::OTNone)
			{
				continue;
			}
			
			FBIMAssemblySpec newSpec;
			if (ensureAlways(newSpec.FromPreset(FBIMPresetCollectionProxy(BIMPresetCollection), affectedPreset) == EBIMResult::Success))
			{
				affectedAssemblies.Add(affectedPreset);
				BIMPresetCollection.UpdateProjectAssembly(newSpec);
			}
		}

		AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(World->GetFirstPlayerController());
		if (controller && controller->DynamicIconGenerator)
		{
			controller->DynamicIconGenerator->UpdateCachedAssemblies(FBIMPresetCollectionProxy(GetPresetCollection()), affectedAssemblies.Array());
		}
		
		for (auto& moi : ObjectInstanceArray)
		{
			if (affectedAssemblies.Contains(moi->GetAssembly().PresetGUID))
			{
				moi->OnAssemblyChanged();
			}
		}

		if (bTrackingDeltaObjects)
		{
			DeltaAffectedPresets.Add(CollectionPreset.GUID);
		}
		
		EWebPresetChangeVerb verb = bIsNewPreset ? EWebPresetChangeVerb::Add : EWebPresetChangeVerb::Update;
		TriggerWebPresetChange(verb, TArray<FGuid>({CollectionPreset.GUID}));

		return true;
	}
	// Remove if we're deleting an old one (new state has no guid)
	else if (PresetDelta.OldState.GUID.IsValid())
	{
		// TODO: removing presets not yet supported, add scope impact here (fallback assemblies, etc)
		BIMPresetCollection.RemovePreset(PresetDelta.OldState.GUID);
		if (PresetDelta.OldState.ObjectType != EObjectType::OTNone)
		{
			BIMPresetCollection.RemoveProjectAssemblyForPreset(PresetDelta.OldState.GUID);
		}

		AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(World->GetFirstPlayerController());
		if (controller && controller->EditModelUserWidget)
		{
			controller->EditModelUserWidget->RefreshAssemblyList();
		}

		if (bTrackingDeltaObjects)
		{
			DeltaAffectedPresets.Add(PresetDelta.OldState.GUID);
		}
		
		TriggerWebPresetChange(EWebPresetChangeVerb::Remove, TArray<FGuid>({PresetDelta.OldState.GUID}));

		return true;
	}

	// Who sent us a delta with no guids?
	return ensure(false);
}

bool UModumateDocument::ApplySettingsDelta(const FDocumentSettingDelta& SettingsDelta, UWorld* World)
{
	if (CurrentSettings != SettingsDelta.NextSettings)
	{
		CurrentSettings = SettingsDelta.NextSettings;

		auto* gameInstance = World->GetGameInstance<UModumateGameInstance>();
		if (gameInstance && gameInstance->DimensionManager)
		{
			gameInstance->DimensionManager->UpdateAllUnits();
		}

		AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(World->GetFirstPlayerController());
		if (controller)
		{
			// Update bim designer
			if (controller->EditModelUserWidget && controller->EditModelUserWidget->IsBIMDesingerActive())
			{
				controller->EditModelUserWidget->BIMDesigner->RefreshNodes();
			}
			// Update setting menu
			if (controller->EditModelUserWidget && controller->EditModelUserWidget->SettingsMenuWidget
				&& controller->EditModelUserWidget->SettingsMenuWidget->IsVisible())
			{
				controller->EditModelUserWidget->SettingsMenuWidget->UpdateSettingsFromDoc();
			}
			// Update sky actor
			if (controller->SkyActor)
			{
				controller->SkyActor->UpdateCoordinate(CurrentSettings.Latitude, CurrentSettings.Longitude, CurrentSettings.TrueNorthDegree);
			}
		}
		// Tell web UI to update its menu
		UpdateWebProjectSettings();
	}

	return true;
}

void UModumateDocument::ApplySymbolDeltas(TArray<FDeltaPtr>& Deltas)
{
	if (GetDirtySymbolGroups().Num() != 0)
	{
		int32 nextID = NextID;
		TArray<FDeltaPtr> symbolDeltas;

		TArray<int32> affectedGroups;
		FModumateSymbolDeltaStatics::PropagateAllDirtySymbols(this, nextID, symbolDeltas, affectedGroups);
		if (symbolDeltas.Num() > 0)
		{
			TArray<FDeltaPtr> sideEffectDeltas;

			for (auto& delta : symbolDeltas)
			{
				delta->ApplyTo(this, GetWorld());
				Deltas.Add(delta);
			}

			int32 guard = 8;
			do
			{
				sideEffectDeltas.Reset();
				CleanObjects(&sideEffectDeltas);

				for (auto& delta : sideEffectDeltas)
				{
					delta->ApplyTo(this, GetWorld());
					Deltas.Add(delta);
				}

			} while (sideEffectDeltas.Num() != 0 && guard-- > 0);
		}

		auto* localPlayer = GetWorld()->GetFirstLocalPlayerFromController();
		auto* controller = localPlayer ? Cast<AEditModelPlayerController>(localPlayer->GetPlayerController(GetWorld())) : nullptr;
		if (controller)
		{
			controller->EMPlayerState->PostGroupChanged(affectedGroups);
		}

	}

}

bool UModumateDocument::ApplyDeltas(const TArray<FDeltaPtr>& Deltas, UWorld* World, bool bRedoingDeltas, bool bDDCleaningDelta)
{
	// Vacuous success if there are no deltas to apply
	if (Deltas.Num() == 0)
	{
		return true;
	}

	auto* localPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	auto* controller = localPlayer ? Cast<AEditModelPlayerController>(localPlayer->GetPlayerController(World)) : nullptr;

	bool bMultiplayerClient = controller && controller->EMPlayerState && controller->EMPlayerState->IsNetMode(NM_Client);

	// Fail immediately if we're playing back recorded input
	if (controller && controller->InputAutomationComponent && controller->InputAutomationComponent->ShouldDocumentSkipDeltas())
	{
		return false;
	}

	if (bMultiplayerClient && !controller->EMPlayerState->ReplicatedProjectPermissions.CanEdit)
	{
		return false;
	}

	StartTrackingDeltaObjects();

	SetDirtyFlags(true);
	ClearPreviewDeltas(World, false);

	if (!bRedoingDeltas)
	{
		ClearRedoBuffer();
	}

	bool resendDDViews = false;
	BeginUndoRedoMacro();

	TSharedPtr<UndoRedo> ur = MakeShared<UndoRedo>();
	ur->Deltas = Deltas;

	// If we're a multiplayer client, don't add to the undo buffer until we've received verification from the server
	if (!bMultiplayerClient && !bDDCleaningDelta)
	{
			UndoBuffer.Add(ur);
	}

	// For Symbol propagation:
	TArray<FDeltaPtr> derivedDestroyDeltas;
	FModumateSymbolDeltaStatics::GetDerivedDeltasFromDeltas(this, EMOIDeltaType::Destroy, Deltas, derivedDestroyDeltas);
	ur->Deltas.Append(derivedDestroyDeltas);

	TSet<EObjectType> affectedTypes;

	// First, apply the input deltas, generated from the first pass of user intent
	for (auto& delta : ur->Deltas)
	{
		delta->ApplyTo(this, World);
		TArray<TPair<int32, EMOIDeltaType>> affectedObjectList;
		delta->GetAffectedObjects(affectedObjectList);
		if (affectedObjectList.Num() > 0)
		{
			resendDDViews = true;

			for (auto& dao : affectedObjectList)
			{
				auto* moi = GetObjectById(dao.Key);
				if (dao.Value != EMOIDeltaType::Destroy && ensure(moi != nullptr))
				{
					affectedTypes.Add(moi->GetObjectType());
				}
			}
		}
	}

	CalculateSideEffectDeltas(ur->Deltas, World, false);

	ApplySymbolDeltas(ur->Deltas);

	EndUndoRedoMacro();
	if (controller && controller->InputAutomationComponent)
	{
		controller->InputAutomationComponent->PostApplyUserDeltas(ur->Deltas);
	}

	// If we make a DeltasRecord immediately after one of our own unverified DeltasRecords, then we need to use that unverified hash,
	// rather than re-use the same verified document hash that hasn't been updated yet.
	uint32 latestDocHash = (UnverifiedDeltasRecords.Num() > 0) ? UnverifiedDeltasRecords.Last().TotalHash : GetLatestVerifiedDocHash();
	FDeltasRecord deltasRecord(ur->Deltas, CachedLocalUserID, latestDocHash);
	deltasRecord.bDDCleaningDelta = bDDCleaningDelta;

	FBox deltaAffectedBounds = GetAffectedBounds(DeltaAffectedObjects, DeltaDirtiedObjects);
	deltasRecord.SetResults(DeltaAffectedObjects, DeltaDirtiedObjects, DeltaAffectedPresets.Array(), deltaAffectedBounds);


	for (auto& dao : DeltaDirtiedObjects)
	{
		auto* moi = GetObjectById(dao);
		if (moi != nullptr)
		{
			affectedTypes.Add(moi->GetObjectType());
		}
	}

	// If we're a multiplayer client, then send the deltas generated by this user to the server
	if (bMultiplayerClient)
	{
		UnverifiedDeltasRecords.Add(deltasRecord);
		controller->EMPlayerState->SendClientDeltas(deltasRecord);
	}
	// Otherwise, they're already considered "verified"
	else
	{
		VerifiedDeltasRecords.Add(deltasRecord);
	}

	EndTrackingDeltaObjects();

	// TODO: only send changed MOIS to Web
	// TODO: move this to after object cleaning or a more efficient location. maybe "Mark yourself dirty for network and send all dirty network devices"
	if (resendDDViews)
	{
		for (auto& at : affectedTypes)
		{
			UpdateWebMOIs(at);
		}
		//This forces an update of the renders on the DD side
		UpdateWebMOIs(EObjectType::OTCutPlane);
	}

	return true;
}

void UModumateDocument::PurgeDeltas()
{
	InitialDocHash = GetLatestVerifiedDocHash();
	UnverifiedDeltasRecords.Empty();
	VerifiedDeltasRecords.Empty();
}

bool UModumateDocument::StartPreviewing()
{
	if (bApplyingPreviewDeltas || bFastClearingPreviewDeltas)
	{
		return false;
	}

	bApplyingPreviewDeltas = true;
	PrePreviewNextID = NextID;

	FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());

	return true;
}

bool UModumateDocument::ApplyPreviewDeltas(const TArray<FDeltaPtr> &Deltas, UWorld *World)
{
	ClearPreviewDeltas(World, true);

	// Skip preview deltas if the input automation is playing back recorded deltas, because they might interfere with one another
	AEditModelPlayerController* controller = World ? World->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;
	if (controller && controller->InputAutomationComponent && controller->InputAutomationComponent->ShouldDocumentSkipDeltas())
	{
		return false;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateDocumentApplyPreviewDeltas);

	if (!bApplyingPreviewDeltas)
	{
		StartPreviewing();
	}

	PreviewDeltas = Deltas;

	// First, apply the input deltas, generated from the first pass of user intent
	for (auto& delta : PreviewDeltas)
	{
		delta->ApplyTo(this, World);
	}

	CalculateSideEffectDeltas(PreviewDeltas, World, true);

	return true;
}

bool UModumateDocument::IsPreviewingDeltas() const
{
	return bApplyingPreviewDeltas;
}

void UModumateDocument::ClearPreviewDeltas(UWorld *World, bool bFastClear)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateDocumentClearPreviewDeltas);

	if (!bApplyingPreviewDeltas || !ensure(!bFastClearingPreviewDeltas && !bSlowClearingPreviewDeltas))
	{
		return;
	}

	if (bFastClear)
	{
		bFastClearingPreviewDeltas = true;
	}
	else
	{
		bSlowClearingPreviewDeltas = true;
	}

	TArray<FDeltaPtr> inversePreviewDeltas = PreviewDeltas;
	Algo::Reverse(inversePreviewDeltas);

	// First, apply the input deltas, generated from the first pass of user intent
	for (auto& delta : inversePreviewDeltas)
	{
		delta->MakeInverse()->ApplyTo(this, World);
	}

	PreviewDeltas.Reset();

	if (bSlowClearingPreviewDeltas)
	{
		// Fully delete any objects that were created by preview deltas, in this case found and defined by
		// their ID being greater than the non-preview NextID.
		TArray<int32> invalidDeletedIDs;
		for (auto kvp : DeletedObjects)
		{
			if (kvp.Key >= NextID)
			{
				invalidDeletedIDs.Add(kvp.Key);
			}
		}

		for (int32 invalidDeletedID : invalidDeletedIDs)
		{
			DeletedObjects.FindAndRemoveChecked(invalidDeletedID)->Destroy();
		}
	}

	NextID = PrePreviewNextID;

	PostApplyDeltas(World, true, false);

	bApplyingPreviewDeltas = false;
	bFastClearingPreviewDeltas = false;
	bSlowClearingPreviewDeltas = false;
}

void UModumateDocument::CalculateSideEffectDeltas(TArray<FDeltaPtr>& Deltas, UWorld* World, bool bIsPreview)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateDocumentCalculateSideEffectDeltas);

	// Next, clean objects while gathering potential side effect deltas,
	// apply side effect deltas, and add them to the undo/redo-able list of deltas.
	// Prevent infinite loops, but allow for iteration due to multiple levels of dependency.
	static constexpr int32 maxSideEffectIteration = 8;
	int32 sideEffectIterationGuard = maxSideEffectIteration;
	TArray<FDeltaPtr> sideEffectDeltas;

	// Deltas created by Symbol reflection logic:
	TArray<FDeltaPtr> derivedDeltas;
	const bool bCreateDerived = !IsPreviewingDeltas();

	do
	{
		sideEffectDeltas.Reset();
		CleanObjects(&sideEffectDeltas);

		for (auto& delta : sideEffectDeltas)
		{
			Deltas.Add(delta);
			delta->ApplyTo(this, World);
		}

		if (bCreateDerived && sideEffectIterationGuard == maxSideEffectIteration)
		{
			FModumateSymbolDeltaStatics::GetDerivedDeltasFromDeltas(this, EMOIDeltaType::Mutate, Deltas, derivedDeltas);
			FModumateSymbolDeltaStatics::GetDerivedDeltasFromDeltas(this, EMOIDeltaType::Create, Deltas, derivedDeltas);

			if (derivedDeltas.Num() > 0 || GetDirtySymbolGroups().Num() > 0)
			{
				for (auto& delta : derivedDeltas)
				{
					Deltas.Add(delta);
					delta->ApplyTo(this, World);
				}

				sideEffectDeltas.Reset();
				CleanObjects(&sideEffectDeltas);
				for (auto& delta : sideEffectDeltas)
				{
					Deltas.Add(delta);
					delta->ApplyTo(this, World);
				}
			}
		}

		PostApplyDeltas(World, false, false);

		if (!ensure(--sideEffectIterationGuard > 0))
		{
			UE_LOG(LogTemp, Error, TEXT("Iterative CleanObjects generated too many side effects; preventing infinite loop!"));
			break;
		}

	} while (sideEffectDeltas.Num() > 0);
}

void UModumateDocument::UpdateVolumeGraphObjects(UWorld *World)
{
	// TODO: unclear whether this is correct or the best place -
	// set the faces containing or contained by dirty faces dirty as well
	for (auto& volumeGraphKvp : VolumeGraphs)
	{
		FGraph3D* volumeGraph = volumeGraphKvp.Value.Get();

		for (auto& kvp : volumeGraph->GetFaces())
		{
			auto& face = kvp.Value;
			if (!face.bDirty)
			{
				continue;
			}

			if (auto containingFace = volumeGraph->FindFace(face.ContainingFaceID))
			{
				containingFace->Dirty(false);
			}
			for (int32 containedFaceID : face.ContainedFaceIDs)
			{
				if (auto containedFace = volumeGraph->FindFace(containedFaceID))
				{
					containedFace->Dirty(false);
				}
			}
		}

		TArray<int32> cleanedVertices, cleanedEdges, cleanedFaces;

		if (volumeGraph->CleanGraph(cleanedVertices, cleanedEdges, cleanedFaces))
		{
			for (int32 vertexID : cleanedVertices)
			{
				FGraph3DVertex* graphVertex = volumeGraph->FindVertex(vertexID);
				AModumateObjectInstance* vertexObj = GetObjectById(vertexID);
				if (graphVertex && vertexObj && (vertexObj->GetObjectType() == EObjectType::OTMetaVertex))
				{
					vertexObj->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}

			for (int32 edgeID : cleanedEdges)
			{
				FGraph3DEdge* graphEdge = volumeGraph->FindEdge(edgeID);
				AModumateObjectInstance* edgeObj = GetObjectById(edgeID);
				if (graphEdge && edgeObj && (edgeObj->GetObjectType() == EObjectType::OTMetaEdge))
				{
					edgeObj->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}

			for (int32 faceID : cleanedFaces)
			{
				FGraph3DFace* graphFace = volumeGraph->FindFace(faceID);
				AModumateObjectInstance* faceObj = GetObjectById(faceID);
				if (graphFace && faceObj && (faceObj->GetObjectType() == EObjectType::OTMetaPlane))
				{
					faceObj->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}

			AModumateObjectInstance* graphObj = GetObjectById(volumeGraphKvp.Key);
			if (graphObj)
			{
				graphObj->MarkDirty(EObjectDirtyFlags::Structure);
			}

			if (cleanedVertices.Num() > 0)
			{   // If any vertices have moved then recalculate graph boxes.
				AModumateObjectInstance* graphObject = GetObjectById(volumeGraph->GraphID);
				if (ensure(graphObject))
				{
					graphObject->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}

		}
	}
}

bool UModumateDocument::FinalizeGraphDeltas(const TArray<FGraph3DDelta> &InDeltas, TArray<FDeltaPtr> &OutDeltas, int32 GraphID /*= MOD_ID_NONE*/)
{
	FGraph3D moiTempGraph;
	FGraph3D::CloneFromGraph(moiTempGraph, *GetVolumeGraph(GraphID));

	GraphDeltaElementChanges.Empty();

	for (auto& delta : InDeltas)
	{
		TArray<FDeltaPtr> sideEffectDeltas;
		if (!FinalizeGraphDelta(moiTempGraph, delta, sideEffectDeltas))
		{
			return false;
		}

		OutDeltas.Add(MakeShared<FGraph3DDelta>(delta));
		OutDeltas.Append(sideEffectDeltas);
	}

	TArray<AMOIMetaPlaneSpan*> spans;
	GetObjectsOfTypeCasted(EObjectType::OTMetaPlaneSpan, spans);

	if (GraphDeltaElementChanges.Num() > 0)
	{   // TODO: Do we still need this?
		TArray<int32> faceChangeArray = GraphDeltaElementChanges.Array();
		for (auto* span : spans)
		{
			// Only update spans who have lost a face (negative ID in faceChangeArray) 
			// otherwise new faces added to the side of a span are automatically added
			for (int32 faceChange : faceChangeArray)
			{
				if (span->InstanceData.GraphMembers.Contains(-faceChange))
				{
					// copy whole array because if we have lost a face, all bets are off and any added face is legit
					span->PostGraphChanges = faceChangeArray;
					span->MarkDirty(EObjectDirtyFlags::Structure);
					break;
				}
			}
		}
	}

	return true;
}

bool UModumateDocument::PostApplyDeltas(UWorld *World, bool bCleanObjects, bool bMarkDocumentDirty)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateDocumentPostApplyDeltas);

	if (bCleanObjects)
	{
		CleanObjects(nullptr);
	}
	
	if (DrawingDesignerDocument.bDirty)
	{
		DrawingPushDD();
	}

	// Now that objects may have been deleted, validate the player state so that none of them are incorrectly referenced.
	ULocalPlayer* localPlayer = World->GetFirstLocalPlayerFromController();
	APlayerController* localController = localPlayer ? localPlayer->GetPlayerController(World) : nullptr;
	AEditModelPlayerState* playerState = localController ? Cast<AEditModelPlayerState>(localController->PlayerState) : nullptr;
	if (playerState)
	{
		playerState->ValidateSelectionsAndView();
		if (ChangedGroupIDs.Num() > 0)
		{   // Group change due to, eg, deleting active group. 
			playerState->PostGroupChanged(ChangedGroupIDs);
			ChangedGroupIDs.Empty();
		}
	}

	// TODO: Find a better way to determine what objects were or are now dependents of CutPlanes,
	// so we don't always have to update them in order to have always-correct lines.
	// Also, this involves a very unfortunate workaround for preventing deltas from serializing cut planes dirtied by their application.

	if (!IsPreviewingDeltas())
	{
		DirtyAllCutPlanes();
	}


	if (bMarkDocumentDirty)
	{
		// TODO: keep track of document dirtiness by a unique identifier of which delta is at the top of the stack,
		// but that refactor could wait until the multiplayer refactor which would also affect the definition of autosave.
		SetDirtyFlags(true);
	}

	UModumateObjectStatics::UpdateDesignOptionVisibility(this);

	return true;
}

void UModumateDocument::DirtyAllCutPlanes()
{
	bool bWasTrackingDeltaObjects = bTrackingDeltaObjects;
	bTrackingDeltaObjects = false;

	TArray<AModumateObjectInstance*> cutPlanes = GetObjectsOfType(EObjectType::OTCutPlane);
	for (auto cutPlane : cutPlanes)
	{
		cutPlane->MarkDirty(EObjectDirtyFlags::Visuals);
	}

	bTrackingDeltaObjects = bWasTrackingDeltaObjects;
}

void UModumateDocument::StartTrackingDeltaObjects()
{
	if (!ensure(!bTrackingDeltaObjects))
	{
		return;
	}

	for (auto& kvp : DeltaAffectedObjects)
	{
		kvp.Value.Reset();
	}

	DeltaDirtiedObjects.Reset();
	DeltaAffectedPresets.Reset();

	bTrackingDeltaObjects = true;
}

void UModumateDocument::EndTrackingDeltaObjects()
{
	if (!ensure(bTrackingDeltaObjects))
	{
		return;
	}

	// For analytics, we want to keep track of which objects were added/removed aggregated on object type,
	// which requires a bit more processing than if we only cared about the identities of affected objects.
	static TMap<EObjectType, TSet<int32>> affectedObjectsByType;
	auto gatherAffectedObjectsByType = [this](const TSet<int32>& ObjectIDs)
	{
		for (auto& kvp : affectedObjectsByType)
		{
			kvp.Value.Reset();
		}

		for (int32 objectID : ObjectIDs)
		{
			auto* object = GetObjectById(objectID);
			if (object)
			{
				affectedObjectsByType.FindOrAdd(object->GetObjectType()).Add(objectID);
			}
		}
	};

	gatherAffectedObjectsByType(DeltaAffectedObjects.FindOrAdd(EMOIDeltaType::Create));
	for (auto& kvp : affectedObjectsByType)
	{
		EObjectType objectType = kvp.Key;
		int32 numCreatedObjects = kvp.Value.Num();
		if (numCreatedObjects > 0)
		{
			UModumateAnalyticsStatics::RecordObjectCreation(this, objectType, numCreatedObjects);
			OnAppliedMOIDeltas.Broadcast(objectType, numCreatedObjects, EMOIDeltaType::Create);
		}

		kvp.Value.Reset();
	}

	gatherAffectedObjectsByType(DeltaAffectedObjects.FindOrAdd(EMOIDeltaType::Destroy));
	for (auto& kvp : affectedObjectsByType)
	{
		EObjectType objectType = kvp.Key;
		int32 numDeletedObjects = kvp.Value.Num();
		if (numDeletedObjects > 0)
		{
			UModumateAnalyticsStatics::RecordObjectDeletion(this, objectType, numDeletedObjects);
			OnAppliedMOIDeltas.Broadcast(objectType, numDeletedObjects, EMOIDeltaType::Destroy);
		}

		kvp.Value.Reset();
	}

	bTrackingDeltaObjects = false;
}

FBox UModumateDocument::GetAffectedBounds(const FAffectedObjMap& AffectedObjects, const TSet<int32>& DirtiedObjects) const
{
	static TArray<FVector> allMOIPoints;
	static TArray<FStructurePoint> curMOIPoints;
	static TArray<FStructureLine> curMOILines;
	allMOIPoints.Reset();

	auto gatherPointsForObjects = [this](const TSet<int32>& ObjectIDs) {
		for (int32 objID : ObjectIDs)
		{
			const AModumateObjectInstance* affectedObj = GetObjectById(objID);
			if (affectedObj)
			{
				curMOIPoints.Reset();
				curMOILines.Reset();
				affectedObj->GetStructuralPointsAndLines(curMOIPoints, curMOILines);

				for (const FStructurePoint& point : curMOIPoints)
				{
					allMOIPoints.Add(point.Point);
				}
			}
		}
	};

	for (auto& kvp : AffectedObjects)
	{
		gatherPointsForObjects(kvp.Value);
	}
	gatherPointsForObjects(DirtiedObjects);

	return FBox(allMOIPoints);
}

void UModumateDocument::DeleteObjects(const TArray<int32> &obIds, bool bDeleteConnected)
{
	TArray<AModumateObjectInstance*> mois;
	Algo::Transform(obIds,mois,[this](int32 id) { return GetObjectById(id);});

	DeleteObjects(mois.FilterByPredicate([](AModumateObjectInstance *ob) { return ob != nullptr; }),bDeleteConnected);
}

void UModumateDocument::DeleteObjects(const TArray<AModumateObjectInstance*>& initialObjectsToDelete, bool bDeleteConnected)
{
	TArray<FDeltaPtr> deleteDeltas;
	if (!GetDeleteObjectsDeltas(deleteDeltas, initialObjectsToDelete, bDeleteConnected))
	{
		return;
	}

	UWorld *world = initialObjectsToDelete.Num() > 0 ? initialObjectsToDelete[0]->GetWorld() : nullptr;
	if (world == nullptr)
	{
		return;
	}
	ApplyDeltas(deleteDeltas, world);
}

bool UModumateDocument::GetDeleteObjectsDeltas(TArray<FDeltaPtr> &OutDeltas, const TArray<AModumateObjectInstance*> &InitialObjectsToDelete, bool bDeleteConnected, bool bDeleteDescendents)
{
	if (InitialObjectsToDelete.Num() == 0)
	{
		return false;
	}

	UWorld *world = GetWorld();
	if (!ensureAlways(world != nullptr))
	{
		return false;
	}

	TArray<AModumateObjectInstance*> currentObjectsToDelete = InitialObjectsToDelete;
	if (bDeleteDescendents)
	{
		for (auto curObjToDelete : InitialObjectsToDelete)
		{
			currentObjectsToDelete.Append(curObjToDelete->GetAllDescendents());
		}
	}

	// Keep track of all of the IDs of objects that the various kinds of deltas (Graph3D, Graph2D, non-Graph, etc.) will delete.
	TSet<int32> objIDsToDelete;
	TArray<TPair<int32, EMOIDeltaType>> affectedObjects;
	TSet<AModumateObjectInstance*> groupsToDelete;

	auto gatherDeletedIDs = [&objIDsToDelete, &affectedObjects](const TArray<FDeltaPtr>& Deltas) {
		affectedObjects.Reset();
		for (auto& delta : Deltas)
		{
			delta->GetAffectedObjects(affectedObjects);
		}

		for (auto& kvp : affectedObjects)
		{
			if (kvp.Value == EMOIDeltaType::Destroy)
			{
				objIDsToDelete.Add(kvp.Key);
			}
		}
	};

	// Gather 2D/3D graph objects, so we can generated deltas that will potentially delete connected objects.
	// Group surface graph elements by surface graph, so we can generate deltas for each surface graph separately
	// NOTE: for consistency and less code (at the cost of performance), deleting an entire surface graph (say, as a result of deleting its parent wall)
	// is accomplished by getting the deltas for deleting all of its elements. This also allows undo to add them back as Graph2D element addition deltas.
	TSet<int32> graph3DObjIDsToDelete;
	TMap<int32, TArray<int32>> surfaceGraphDeletionMap;
	for (AModumateObjectInstance* objToDelete : currentObjectsToDelete)
	{
		if (objToDelete == nullptr)
		{
			continue;
		}

		int32 objID = objToDelete->ID;
		EObjectType objType = objToDelete->GetObjectType();
		EGraph3DObjectType graph3DObjType;
		bool bObjIsGraph2D = SurfaceGraphs.Contains(objID);

		if (objToDelete->GetParentObject() && UModumateTypeStatics::IsSpanObject(objToDelete->GetParentObject()))
		{
			objToDelete->GetParentObject()->MarkDirty(EObjectDirtyFlags::Structure);
		}

		if (IsObjectInVolumeGraph(objToDelete->ID, graph3DObjType))
		{
			graph3DObjIDsToDelete.Add(objID);
		}
		else if ((UModumateTypeStatics::Graph2DObjectTypeFromObjectType(objType) != EGraphObjectType::None) || bObjIsGraph2D)
		{
			int32 surfaceGraphID = bObjIsGraph2D ? objID : objToDelete->GetParentID();
			if (!ensureMsgf(SurfaceGraphs.Contains(surfaceGraphID),
				TEXT("Tried to delete Graph2D-related MOI #%d, but there's no Graph2D #%d!"), objID, surfaceGraphID))
			{
				continue;
			}

			TSharedPtr<FGraph2D>& graph2D = SurfaceGraphs[surfaceGraphID];
			TArray<int32>& graph2DObjsToDelete = surfaceGraphDeletionMap.FindOrAdd(surfaceGraphID);
			if (bObjIsGraph2D)
			{
				graph2D->GetAllObjects().GenerateKeyArray(graph2DObjsToDelete);
			}
			else
			{
				graph2DObjsToDelete.AddUnique(objToDelete->ID);
			}
		}
		else if (objToDelete->GetObjectType() == EObjectType::OTMetaGraph)
		{
			groupsToDelete.Add(objToDelete);
		}
	}

	// Generate the delta to apply to the 3D volume graph for object deletion
	TArray<FDeltaPtr> graph3DDeltaPtrs;
	if (graph3DObjIDsToDelete.Num() > 0)
	{
		TArray<FGraph3DDelta> graph3DDeltas;
		if (!TempVolumeGraph.GetDeltaForDeleteObjects(graph3DObjIDsToDelete.Array(), graph3DDeltas, NextID, bDeleteConnected, true))
		{
			FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
			return false;
		}

		if (!FinalizeGraphDeltas(graph3DDeltas, graph3DDeltaPtrs))
		{
			FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
			return false;
		}
	}

	// Accumulate objects to delete from Graph3D deletion deltas
	gatherDeletedIDs(graph3DDeltaPtrs);
	OutDeltas.Append(graph3DDeltaPtrs);

	// Generate the deltas to apply to the 2D surface graphs for object deletion
	TArray<FGraph2DDelta> tempSurfaceGraphDeltas;
	TArray<FDeltaPtr> combinedSurfaceGraphDeltas;
	TSet<int32> combinedSurfaceGraphObjIDsToDelete;
	TSet<AModumateObjectInstance*> surfaceGraphDescendents;
	for (auto& kvp : surfaceGraphDeletionMap)
	{
		int32 surfaceGraphID = kvp.Key;

		// If the surface graph has already been deleted as a result of deleting some selected Graph3D elements,
		// then skip the Graph2D deletion.
		if (objIDsToDelete.Contains(surfaceGraphID))
		{
			continue;
		}

		const TArray<int32>& surfaceGraphObjToDelete = kvp.Value;
		auto surfaceGraph = FindSurfaceGraph(surfaceGraphID);
		tempSurfaceGraphDeltas.Reset();

		if (!surfaceGraph->DeleteObjects(tempSurfaceGraphDeltas, NextID, surfaceGraphObjToDelete))
		{
			return false;
		}

		if (!FinalizeGraph2DDeltas(tempSurfaceGraphDeltas, NextID, combinedSurfaceGraphDeltas))
		{
			return false;
		}
	}

	// Accumulate objects to delete from Graph2D deletion deltas
	gatherDeletedIDs(combinedSurfaceGraphDeltas);
	OutDeltas.Append(combinedSurfaceGraphDeltas);

	// Delete non-graph objects that were not deleted as a result of Graph 2D/3D deltas
	TArray<AModumateObjectInstance*> nonGraphObjsToDelete;
	for (AModumateObjectInstance* objToDelete : currentObjectsToDelete)
	{
		if (objToDelete && !objIDsToDelete.Contains(objToDelete->ID) && !groupsToDelete.Contains(objToDelete))
		{
			nonGraphObjsToDelete.Add(objToDelete);
		}
	}

	if (nonGraphObjsToDelete.Num() > 0)
	{
		auto nonGraphDeleteDelta = MakeShared<FMOIDelta>();
		for (const AModumateObjectInstance* nonGraphObj : nonGraphObjsToDelete)
		{
			nonGraphDeleteDelta->AddCreateDestroyState(nonGraphObj->GetStateData(), EMOIDeltaType::Destroy);
			objIDsToDelete.Add(nonGraphObj->ID);
		}
		OutDeltas.Add(nonGraphDeleteDelta);
	}

	for (auto* groupToDelete : groupsToDelete)
	{
		if (ensureAlways(groupToDelete->ID != GetRootVolumeGraphID()))
		{
			FModumateObjectDeltaStatics::GetDeltasForGraphDelete(this, groupToDelete->ID, OutDeltas);
			GetDeleteObjectsDeltas(OutDeltas, groupToDelete->GetChildObjects(), false, false);
		}
	}

	return true;
}

AModumateObjectInstance *UModumateDocument::TryGetDeletedObject(int32 id)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::TryGetDeletedObject"));
	return DeletedObjects.FindRef(id);
}

bool UModumateDocument::GetVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas)
{
	TArray<FGraph3DDelta> deltas;

	if (!TempVolumeGraph.GetDeltaForVertexMovements(VertexIDs, VertexPositions, deltas, NextID))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
		return false;
	}

	if (!FinalizeGraphDeltas(deltas, OutDeltas))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
		return false;
	}

	return true;
}

bool UModumateDocument::GetPreviewVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas)
{
	TArray<FGraph3DDelta> deltas;

	if (!TempVolumeGraph.MoveVerticesDirect(VertexIDs, VertexPositions, deltas, NextID))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
		return false;
	}

	if (!FinalizeGraphDeltas(deltas, OutDeltas))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
		return false;
	}

	return true;
}

bool UModumateDocument::MoveMetaVertices(UWorld* World, const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions)
{
	TArray<FDeltaPtr> deltas;
	if (GetVertexMovementDeltas(VertexIDs, VertexPositions, deltas))
	{
		return ApplyDeltas(deltas, World);
	}

	return false;
}

bool UModumateDocument::JoinMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs)
{
	if (ObjectIDs.Num() < 2)
	{
		return false;
	}

	EGraph3DObjectType objectType;
	if (!IsObjectInVolumeGraph(ObjectIDs[0], objectType))
	{
		return false;
	}

	TArray<FGraph3DDelta> graphDeltas;
	if (!TempVolumeGraph.GetDeltasForObjectJoin(graphDeltas, ObjectIDs, NextID, objectType))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
		return false;
	}

	TArray<FDeltaPtr> deltaPtrs;
	if (!FinalizeGraphDeltas(graphDeltas, deltaPtrs))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
		return false;
	}
	return ApplyDeltas(deltaPtrs, World);
}

bool UModumateDocument::ReverseMetaObjects(UWorld* World, const TArray<int32>& EdgeObjectIDs, const TArray<int32>& FaceObjectIDs)
{
	if (EdgeObjectIDs.Num() == 0 && FaceObjectIDs.Num() == 0)
	{
		return false;
	}
	TArray<FDeltaPtr> deltaPtrs;
	if (!GetDeltaForReverseMetaObjects(World, EdgeObjectIDs, FaceObjectIDs, deltaPtrs))
	{
		return false;
	}
	return ApplyDeltas(deltaPtrs, World);
}

bool UModumateDocument::GetDeltaForReverseMetaObjects(UWorld* World, const TArray<int32>& EdgeObjectIDs, const TArray<int32>& FaceObjectIDs, TArray<FDeltaPtr>& OutDeltas)
{
	TArray<FGraph3DDelta> graphDeltas;
	if (!TempVolumeGraph.GetDeltasForObjectReverse(graphDeltas, EdgeObjectIDs, FaceObjectIDs))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
		return false;
	}
	if (!FinalizeGraphDeltas(graphDeltas, OutDeltas))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
		return false;
	}
	return true;
}

bool UModumateDocument::GetGraph2DDeletionDeltas(int32 Graph2DID, int32& InNextID, TArray<FDeltaPtr>& OutDeltas) const
{
	auto surfaceGraphObj = GetObjectById(Graph2DID);
	if (!surfaceGraphObj || !SurfaceGraphs.Contains(surfaceGraphObj->ID))
	{
		return false;
	}

	// delete all objects in the surface graph
	auto surfaceGraph = SurfaceGraphs[surfaceGraphObj->ID];
	TArray<FGraph2DDelta> deltas;
	TArray<int32> allIDs;
	surfaceGraph->GetAllObjects().GenerateKeyArray(allIDs);
	surfaceGraph->DeleteObjects(deltas, InNextID, allIDs);

	return FinalizeGraph2DDeltas(deltas, InNextID, OutDeltas);
}

bool UModumateDocument::MakeMetaObject(UWorld* world, const TArray<FVector>& points,
	TArray<int32>& OutObjectIDs, TArray<FDeltaPtr>& OutDeltaPtrs, TArray<FGraph3DDelta>& OutGraphDeltas, bool bSplitAndUpdateFaces)
{
	// TODO: Pass in nextID ref, DO NOT increment your own copy

	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeMetaObject"));
	OutObjectIDs.Reset();
	OutDeltaPtrs.Reset();
	OutGraphDeltas.Reset();

	bool bValidDelta = false;
	int32 numPoints = points.Num();

	int32 id = MOD_ID_NONE;

	if (numPoints == 1)
	{
		FGraph3DDelta graphDelta(ActiveVolumeGraph);
		bValidDelta = (numPoints == 1) && TempVolumeGraph.GetDeltaForVertexAddition(points[0], graphDelta, NextID, id);
		OutObjectIDs = { id };
		OutGraphDeltas = { graphDelta };
	}
	else if (numPoints == 2)
	{
		bValidDelta = TempVolumeGraph.GetDeltaForEdgeAdditionWithSplit(points[0], points[1], OutGraphDeltas, NextID, OutObjectIDs, true, bSplitAndUpdateFaces);
	}
	else if (numPoints >= 3)
	{
		bValidDelta = TempVolumeGraph.GetDeltaForFaceAddition(points, OutGraphDeltas, NextID, OutObjectIDs, bSplitAndUpdateFaces);
	}
	else
	{
		return false;
	}

	if (!bValidDelta || !FinalizeGraphDeltas(OutGraphDeltas, OutDeltaPtrs))
	{
		// delta will be false if the object exists, out object ids should contain the existing id
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
		return false;
	}
	// Note: Test only, cause not sure when span creation happen
#if 0
	if (bValidDelta)
	{
		int32 newSpanID = NextID + 1;
		AMOIMetaPlaneSpan::MakeMetaPlaneSpanDeltaFromGraph(&TempVolumeGraph, newSpanID, OutObjectIDs, OutDeltaPtrs);
	}
#endif

	return bValidDelta;
}

bool UModumateDocument::PasteMetaObjects(const FGraph3DRecord* InRecord, TArray<FDeltaPtr>& OutDeltaPtrs, TMap<int32, TArray<int32>>& OutCopiedToPastedIDs, const FVector &Offset, bool bIsPreview)
{
	// TODO: potentially make this function a bool
	TArray<FGraph3DDelta> OutDeltas;
	TempVolumeGraph.GetDeltasForPaste(InRecord, Offset, NextID, OutDeltas, OutCopiedToPastedIDs, bIsPreview);

	if (!FinalizeGraphDeltas(OutDeltas, OutDeltaPtrs))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());
		return false;
	}

	return true;
}

bool UModumateDocument::MakeScopeBoxObject(UWorld *world, const TArray<FVector> &points, TArray<int32> &OutObjIDs, const float Height)
{
	// TODO: reimplement
	return false;
}


bool UModumateDocument::FinalizeGraph2DDeltas(const TArray<FGraph2DDelta>& InDeltas, int32 &InNextID, TArray<FDeltaPtr>& OutDeltas) const
{
	for (auto& delta : InDeltas)
	{
		TArray<FDeltaPtr> sideEffectDeltas;
		if (!FinalizeGraph2DDelta(delta, InNextID, sideEffectDeltas))
		{
			return false;
		}

		OutDeltas.Add(MakeShared<FGraph2DDelta>(delta));
		OutDeltas.Append(sideEffectDeltas);
	}

	return true;
}

bool UModumateDocument::FinalizeGraph2DDelta(const FGraph2DDelta &Delta, int32 &InNextID, TArray<FDeltaPtr> &OutSideEffectDeltas) const
{
	switch (Delta.DeltaType)
	{
	case EGraph2DDeltaType::Add:
		// Currently, Graph2D creation involves manually creating an accompanying MOIDelta to create the object that owns the graph.
		// TODO: either the object owning the graph should contain its data, or this function should have enough information to create the object delta.
		return true;
	case EGraph2DDeltaType::Edit:
	{
		TMap<int32, TArray<int32>> parentIDToChildrenIDs;

		for (auto& kvp : Delta.PolygonAdditions)
		{
			for (int32 parentID : kvp.Value.ParentObjIDs)
			{
				if (parentIDToChildrenIDs.Contains(parentID))
				{
					parentIDToChildrenIDs[parentID].Add(kvp.Key);
				}
				else
				{
					parentIDToChildrenIDs.Add(parentID, { kvp.Key });
				}
			}
		}

		for (auto& kvp : Delta.EdgeAdditions)
		{
			for (int32 parentID : kvp.Value.ParentObjIDs)
			{
				parentID = FMath::Abs(parentID);
				if (parentIDToChildrenIDs.Contains(parentID))
				{
					parentIDToChildrenIDs[parentID].Add(FMath::Abs(kvp.Key));
				}
				else
				{
					parentIDToChildrenIDs.Add(parentID, { FMath::Abs(kvp.Key) });
				}
			}
		}

		TMap<int32, int32> childFaceIDToHostedID;
		TSet<int32> idsWithObjects;
		for (auto& kvp : parentIDToChildrenIDs)
		{
			int32 parentID = kvp.Key;
			auto parentObj = GetObjectById(parentID);

			if (parentObj == nullptr || parentObj->GetChildIDs().Num() == 0)
			{
				continue;
			}

			for (int32 childIdx = 0; childIdx < parentObj->GetChildIDs().Num(); childIdx++)
			{
				auto childObj = GetObjectById(parentObj->GetChildIDs()[childIdx]);

				for (int32 childFaceID : kvp.Value)
				{
					if (!idsWithObjects.Contains(childFaceID))
					{
						FMOIStateData stateData = childObj->GetStateData();
						stateData.ID = InNextID++;
						stateData.ParentID = childFaceID;

						auto delta = MakeShared<FMOIDelta>();
						delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
						OutSideEffectDeltas.Add(delta);

						idsWithObjects.Add(childFaceID);
					}
				}
			}
		}

		TArray<int32> deletedObjIDs;
		for (auto& kvp : Delta.PolygonDeletions)
		{
			deletedObjIDs.Add(kvp.Key);
		}
		for (auto& kvp : Delta.EdgeDeletions)
		{
			deletedObjIDs.Add(kvp.Key);
		}

		for (int32 objID : deletedObjIDs)
		{
			auto obj = GetObjectById(objID);

			if (obj && obj->GetChildIDs().Num() > 0)
			{
				auto& childIDs = obj->GetChildIDs();
				for (int32 childIdx = 0; childIdx < childIDs.Num(); childIdx++)
				{
					auto childObj = GetObjectById(childIDs[childIdx]);
					if (!childObj)
					{
						continue;
					}

					auto deleteObjectDelta = MakeShared<FMOIDelta>();
					deleteObjectDelta->AddCreateDestroyState(childObj->GetStateData(), EMOIDeltaType::Destroy);
					OutSideEffectDeltas.Add(deleteObjectDelta);
				}
			}
		}

		return true;
	}
	case EGraph2DDeltaType::Remove:
	{
		auto* graphObj = GetObjectById(Delta.ID);
		if (!ensureMsgf(graphObj, TEXT("Tried to remove Graph2D #%d that didn't have a corresponding MOI!"), Delta.ID))
		{
			// This can still be considered a success because there's no more additional work to do here;
			// the MOI delta isn't necessary since the graph is already missing, but we ensure because this shouldn't happen.
			return true;
		}

		auto deleteGraphObjDelta = MakeShared<FMOIDelta>();
		deleteGraphObjDelta->AddCreateDestroyState(graphObj->GetStateData(), EMOIDeltaType::Destroy);
		OutSideEffectDeltas.Add(deleteGraphObjDelta);
		return true;
	}
	}

	return false;
}

bool UModumateDocument::FinalizeGraphDelta(FGraph3D &TempGraph, const FGraph3DDelta &Delta, TArray<FDeltaPtr> &OutSideEffectDeltas)
{
	TMap<int32, TArray<int32>> parentIDToChildrenIDs;

	for (auto &kvp : Delta.FaceAdditions)
	{
		GraphDeltaElementChanges.Add(kvp.Key);
		for (int32 parentID : kvp.Value.ParentObjIDs)
		{
			if (parentID != MOD_ID_NONE)
			{
				parentIDToChildrenIDs.FindOrAdd(parentID).Add(kvp.Key);
			}
		}
	}

	for (auto &kvp : Delta.EdgeAdditions)
	{
		for (int32 parentID : kvp.Value.ParentObjIDs)
		{
			if (parentID != MOD_ID_NONE)
			{
				parentIDToChildrenIDs.FindOrAdd(FMath::Abs(parentID)).Add(kvp.Key);
			}
		}
	}

	// both the temp graph and the original graph are used here -
	// it is necessary to compare the new faces to the old faces in order to figure out where the 
	// child object are supposed to go (the original graph is used to populate parentFace below)
	TempGraph.ApplyDelta(Delta);

	TSet<int32> idsWithObjects;

	for (auto &kvp : parentIDToChildrenIDs)
	{
		// TODO: when spans v2 implemented clean up this mess.
		int32 parentID = kvp.Key;
		auto parentObj = GetObjectById(parentID);

		if (parentObj)
		{
			if (parentObj->GetChildIDs().Num() > 0)
			{
				for (int32 childIdx = 0; childIdx < parentObj->GetChildIDs().Num(); childIdx++)
				{
					auto childObj = GetObjectById(parentObj->GetChildIDs()[childIdx]);
					// wall/floor objects need to be cloned to each child object, and will be deleted during face deletions
					for (int32 childFaceID : kvp.Value)
					{
						if (!idsWithObjects.Contains(childFaceID))
						{
							DeepCloneForFinalize(TempGraph, childObj, childFaceID, OutSideEffectDeltas);
							idsWithObjects.Add(childFaceID);
						}
					}
				}
			}

			// Treat spans & descendants as children to be cloned. MOD-3457
			TArray<int32> spanIDs;
			UModumateObjectStatics::GetSpansForFaceObject(this, parentObj, spanIDs);
			UModumateObjectStatics::GetSpansForEdgeObject(this, parentObj, spanIDs);
			for (int32 spanID: spanIDs)
			{
				for (int32 newElementID : kvp.Value)
				{
					if (!idsWithObjects.Contains(newElementID))
					{
						const auto* span = GetObjectById(spanID);
						FMOIStateData spanState(span->GetStateData());
						spanState.ID = NextID++;

						auto spanDelta = MakeShared<FMOIDelta>();
						if (span->GetObjectType() == EObjectType::OTMetaPlaneSpan)
						{
							FMOIMetaPlaneSpanData spanData(Cast<AMOIMetaPlaneSpan>(span)->InstanceData);
							spanData.GraphMembers = { newElementID };
							spanState.CustomData.SaveStructData(spanData);
						}
						else
						{   // Edge span:
							FMOIMetaEdgeSpanData spanData(Cast<AMOIMetaEdgeSpan>(span)->InstanceData);
							spanData.GraphMembers = { newElementID };
							spanState.CustomData.SaveStructData(spanData);
						}

						spanDelta->AddCreateDestroyState(spanState, EMOIDeltaType::Create);
						OutSideEffectDeltas.Add(spanDelta);

						for (int32 childIdx = 0; childIdx < span->GetChildIDs().Num(); ++childIdx)
						{
							auto childObj = GetObjectById(span->GetChildIDs()[childIdx]);
							DeepCloneForFinalize(TempGraph, childObj, spanState.ID, OutSideEffectDeltas);
						}
						idsWithObjects.Add(newElementID);
					}
				}
			}
		}
	}

	TArray<int32> deletedObjIDs;
	for (auto &kvp : Delta.FaceDeletions)
	{
		deletedObjIDs.Add(kvp.Key);
		GraphDeltaElementChanges.Add(-kvp.Key);
	}

	for (auto &kvp : Delta.EdgeDeletions)
	{
		deletedObjIDs.Add(kvp.Key);
	}

	for (int32 objID : deletedObjIDs)
	{
		const auto* obj = GetObjectById(objID);

		if (obj)
		{
			TArray<int32> childIDsForDeletion (obj->GetChildIDs());

			// Remove spans & descendants.
			TArray<int32> spansForDeletion;
			UModumateObjectStatics::GetSpansForFaceObject(this, obj, spansForDeletion);
			UModumateObjectStatics::GetSpansForEdgeObject(this, obj, spansForDeletion);
			for (int32 spanID : spansForDeletion)
			{
				const auto* spanObj = GetObjectById(spanID);
				childIDsForDeletion.Append(spanObj->GetChildIDs());
				auto deleteObjectDelta = MakeShared<FMOIDelta>();
				deleteObjectDelta->AddCreateDestroyState(spanObj->GetStateData(), EMOIDeltaType::Destroy);
				OutSideEffectDeltas.Add(deleteObjectDelta);
			}

			if (childIDsForDeletion.Num() > 0)
			{
				auto& childIDs = obj->GetChildIDs();
				for (int32 childIdx = 0; childIdx < childIDsForDeletion.Num(); childIdx++)
				{
					auto childObj = GetObjectById(childIDsForDeletion[childIdx]);
					if (!childObj)
					{
						continue;
					}

					auto deleteObjectDelta = MakeShared<FMOIDelta>();
					deleteObjectDelta->AddCreateDestroyState(childObj->GetStateData(), EMOIDeltaType::Destroy);
					OutSideEffectDeltas.Add(deleteObjectDelta);

					if (childObj->GetChildIDs().Num() == 0)
					{
						continue;
					}

					auto& grandchildIDs = childObj->GetChildIDs();
					for (int32 grandchildIdx = 0; grandchildIdx < grandchildIDs.Num(); grandchildIdx++)
					{
						GetGraph2DDeletionDeltas(grandchildIDs[grandchildIdx], NextID, OutSideEffectDeltas);
					}
				}
			}
		}
	}

	return true;
}

bool UModumateDocument::DeepCloneForFinalize(FGraph3D& TempGraph, const AModumateObjectInstance* ChildObj, int32 ChildFaceID, TArray<FDeltaPtr>& OutDerivedDeltas)
{
	FMOIStateData stateData = ChildObj->GetStateData();
	stateData.ID = NextID++;
	stateData.ParentID = ChildFaceID;

	auto delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
	OutDerivedDeltas.Add(delta);

	// MOD-1542 clone over decendents (finish, etc).
	for(int32 grandchildObjID: ChildObj->GetChildIDs())
	{
		auto grandchildObj = GetObjectById(grandchildObjID);
		if (grandchildObj && grandchildObj->GetObjectType() == EObjectType::OTSurfaceGraph)
		{
			// If surface graph is 'simple' then clone also.
			auto surfaceGraph = FindSurfaceGraph(grandchildObj->ID);
			if (surfaceGraph->GetPolygons().Num() == 2)
			{
				// create new surface graph MOI, create empty graph 2D based on new face
				const int32 graphID = NextID++;
				FDeltaPtr newSurfaceGraph = MakeShared<FGraph2DDelta>(graphID, EGraph2DDeltaType::Add);
				FMOIStateData newSurfaceGraphObj(graphID, EObjectType::OTSurfaceGraph, stateData.ID);
				newSurfaceGraphObj.CustomData = grandchildObj->GetStateData().CustomData;
				auto newSurfaceGraphObjdelta = MakeShared<FMOIDelta>();
				newSurfaceGraphObjdelta->AddCreateDestroyState(newSurfaceGraphObj, EMOIDeltaType::Create);
				auto tempGraph2d = MakeShared<FGraph2D>(graphID, THRESH_POINTS_ARE_NEAR);

				const FGraph3DFace* newMetaPlane = TempGraph.FindFace(ChildFaceID);
				if (!ensure(newMetaPlane) || newMetaPlane->VertexIDs.Num() == 0)
				{
					continue;
				}

				// Create simple surface graph for new metaplane. Based on USurfaceGraphTool::CreateGraphFromFaceTarget().
				TArray<int32> newFaceVerts;
				newMetaPlane->GetVertexIDs(newFaceVerts);
				if (newFaceVerts.Num() < 3)
				{
					return false;
				}

				// New graph origin has rotation of old surface-graph object and position of new meta-plane.
				FTransform faceTransform = grandchildObj->GetWorldTransform();
				faceTransform.SetTranslation(TempGraph.FindVertex(newMetaPlane->VertexIDs[0])->Position);

				TArray<FVector2D> perimeterPolygon;
				for (int32 vert3dID: newFaceVerts)
				{
					FVector2D vert2d = UModumateGeometryStatics::ProjectPoint2DTransform(TempGraph.FindVertex(vert3dID)->Position, faceTransform);
					perimeterPolygon.Add(vert2d);
				}
				TMap<int32, TArray<FVector2D>> graphPolygonsToAdd;
				graphPolygonsToAdd.Add(MOD_ID_NONE, perimeterPolygon);
				TArray<FGraph2DDelta> fillGraphDeltas;
				TMap<int32, int32> outFaceToPoly, outGraphToSurfaceVertices;
				TMap<int32, TArray<int32>> graphFaceToVertices;
				int32 newRootPoly = MOD_ID_NONE;

				if (!tempGraph2d->PopulateFromPolygons(fillGraphDeltas, NextID, graphPolygonsToAdd, graphFaceToVertices, outFaceToPoly, outGraphToSurfaceVertices, true, newRootPoly))
				{
					return false;
				}

				OutDerivedDeltas.Add(newSurfaceGraphObjdelta);
				OutDerivedDeltas.Add(newSurfaceGraph);

				FinalizeGraph2DDeltas(fillGraphDeltas, NextID, OutDerivedDeltas);

				// Check for finish, etc, parented to interior polygon of surface graph and copy.
				int32 oldRootPoly = surfaceGraph->GetRootInteriorPolyID();
				const AModumateObjectInstance* polyObject = GetObjectById(oldRootPoly);
				auto oldGraphChildren = polyObject->GetChildObjects();

				for(const auto* oldGraphChild: oldGraphChildren)
				{   // Apparently surface polys can have multiple children.
					FMOIStateData finishStateData = oldGraphChild->GetStateData();
					finishStateData.ID = NextID++;
					finishStateData.ParentID = newRootPoly;
					auto finishDelta = MakeShared<FMOIDelta>();
					finishDelta->AddCreateDestroyState(finishStateData, EMOIDeltaType::Create);
					OutDerivedDeltas.Add(finishDelta);
				}

			}
		}

	}
	return true;
}

void UModumateDocument::ClearRedoBuffer()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ClearRedoBuffer"));
	RedoBuffer.Empty();
}

void UModumateDocument::ClearUndoBuffer()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ClearUndoBuffer"));
	UndoBuffer.Empty();
	UndoRedoMacroStack.Empty();
}

uint32 UModumateDocument::GetLatestVerifiedDocHash() const
{
	return (VerifiedDeltasRecords.Num() > 0) ? VerifiedDeltasRecords.Last().TotalHash : InitialDocHash;
}

int32 UModumateDocument::FindDeltasRecordIdxByHash(uint32 RecordHash) const
{
	return VerifiedDeltasRecords.IndexOfByPredicate([RecordHash](const FDeltasRecord& VerifiedDeltasRecord)
		{ return (VerifiedDeltasRecord.TotalHash == RecordHash); });
}

FBoxSphereBounds UModumateDocument::CalculateProjectBounds() const
{
	TArray<FVector> allMOIPoints;
	TArray<FStructurePoint> curMOIPoints;
	TArray<FStructureLine> curMOILines;

	for (const AModumateObjectInstance *moi : ObjectInstanceArray)
	{
		// TODO: add this functionality to MOIs - IsMetaObject(), meta-objects do not contribute 
		// to project bounds
		if (moi->GetObjectType() == EObjectType::OTCutPlane || moi->GetObjectType() == EObjectType::OTScopeBox)
		{
			continue;
		}

		curMOIPoints.Reset();
		curMOILines.Reset();
		moi->GetStructuralPointsAndLines(curMOIPoints, curMOILines);

		for (const FStructurePoint &point : curMOIPoints)
		{
			allMOIPoints.Add(point.Point);
		}
	}

	FBoxSphereBounds projectBounds(allMOIPoints.GetData(), allMOIPoints.Num());
	return projectBounds;
}

bool UModumateDocument::IsDirty(bool bUserFile) const
{
	return bUserFile ? bUserFileDirty : bAutoSaveDirty;
}

void UModumateDocument::SetDirtyFlags(bool bNewDirty)
{
	bUserFileDirty = bNewDirty;
	bAutoSaveDirty = bNewDirty;

	UpdateWindowTitle();
}

int32 UModumateDocument::GetNextAvailableID() const 
{ 
	return NextID; 
}

int32 UModumateDocument::ReserveNextIDs(int32 reservingObjID)
{ 
	if (!ensureAlways(ReservingObjectID == MOD_ID_NONE))
	{
		return MOD_ID_NONE;
	}

	ReservingObjectID = reservingObjID;

	return NextID;
}

void UModumateDocument::SetNextID(int32 ID, int32 reservingObjID)
{
	if (!ensureAlways(ReservingObjectID == reservingObjID) ||
		!ensureAlways(ID >= NextID))
	{
		return;
	}

	NextID = ID;
	ReservingObjectID = MOD_ID_NONE;
}

bool UModumateDocument::CleanObjects(TArray<FDeltaPtr>* OutSideEffectDeltas /*= nullptr*/, bool bDeleteUncleanableObjects /*= false*/, bool bInitialLoad /*= false*/)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateDocumentCleanObjects);

	UWorld* world = GetWorld();
	UModumateGameInstance* gameInstance = world->GetGameInstance<UModumateGameInstance>();

	if (bInitialLoad)
	{
		gameInstance->GetCloudConnection()->NetworkTick(world);
	}

	UpdateVolumeGraphObjects(world);
	FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());

	for (auto& kvp : SurfaceGraphs)
	{
		kvp.Value->CleanDirtyObjects(true);
	}

	static TMap<int32, EObjectDirtyFlags> curCleanedFlags;
	curCleanedFlags.Reset();

	static TArray<AModumateObjectInstance*> curDirtyList;
	curDirtyList.Reset();

	// While iterating over cleaning all objects, or all objects dirty with the same flag, keep track of the list of dirty objects.
	// If we finish a pass of cleaning objects, and the same objects are dirty as in a previous pass,
	// then we can't expect the results to change with subsequent passes and we can exit.
	// (This is intended to detect if objects are uncleanable because they're dependent on missing data,
	//  or if objects are swapping dirty flags with each other so will never resolve.)
	static TSet<uint32> combinedDirtyStateHashes, perFlagDirtyStateHashes;
	combinedDirtyStateHashes.Reset();
	bool bOldDirtyState = false;

	int32 totalObjectCleans = 0;
	int32 totalObjectsDirty = 0;

	// Prevent iterating over all combined dirty flags too many times while trying to clean all objects
	int32 combinedDirtySafeguard = CleanIterationSafeguard;
	do
	{
		uint32 combinedDirtyStateHash = 0;

		for (EObjectDirtyFlags flagToClean : UModumateTypeStatics::OrderedDirtyFlags)
		{
			perFlagDirtyStateHashes.Reset();

			combinedDirtyStateHash = HashCombine(combinedDirtyStateHash, GetTypeHash(flagToClean));

			// Arbitrarily cut off object cleaning iteration, in case there's bad logic that
			// creates circular dependencies that will never resolve in a single frame.
			bool bModifiedAnyObjects = false;
			int32 objectCleans = 0;
			TArray<AModumateObjectInstance*>& dirtyObjList = DirtyObjectMap.FindOrAdd(flagToClean);

			// Prevent iterating over objects dirtied with this flag too many times while trying to clean all objects
			int32 sameFlagSafeguard = CleanIterationSafeguard;
			do
			{
				// Save off the current list of dirty objects, since it may change while cleaning them.
				bModifiedAnyObjects = false;
				curDirtyList = dirtyObjList;

				combinedDirtyStateHash = HashCombine(combinedDirtyStateHash, GetTypeHash(curDirtyList.Num()));
				uint32 perFlagDirtyStateHash = HashCombine(GetTypeHash(flagToClean), GetTypeHash(curDirtyList.Num()));

				for (AModumateObjectInstance *objToClean : curDirtyList)
				{
					if (!ensure(objToClean))
					{
						continue;
					}

					combinedDirtyStateHash = HashCombine(combinedDirtyStateHash, objToClean->ID);
					perFlagDirtyStateHash = HashCombine(perFlagDirtyStateHash, objToClean->ID);

					EObjectDirtyFlags& cleanedFlags = curCleanedFlags.FindOrAdd(objToClean->ID, EObjectDirtyFlags::None);
					if (!((cleanedFlags & flagToClean) == EObjectDirtyFlags::None))
					{
						UE_LOG(LogTemp, Error, TEXT("Already cleaned %s ID #%d flag %s this frame!"),
							*GetEnumValueString(objToClean->GetObjectType()), objToClean->ID,
							*GetEnumValueString(flagToClean));
					}

					bool bCleaned = objToClean->RouteCleanObject(flagToClean, OutSideEffectDeltas);
					if (bCleaned)
					{
						cleanedFlags |= flagToClean;
					}

					bModifiedAnyObjects |= bCleaned;
					if (bModifiedAnyObjects)
					{
						++objectCleans;
					}

					if (bInitialLoad)
					{
						// Tick network driver in case of long delays.
						gameInstance->GetCloudConnection()->NetworkTick(world);
					}

				}

				perFlagDirtyStateHashes.Add(perFlagDirtyStateHash, &bOldDirtyState);


			} while (bModifiedAnyObjects && !bOldDirtyState &&
				ensureMsgf(--sameFlagSafeguard > 0, TEXT("Infinite loop detected while cleaning objects with flag %s, breaking!"), *GetEnumValueString(flagToClean)));

			totalObjectCleans += objectCleans;
		}

		totalObjectsDirty = 0;
		for (auto& kvp : DirtyObjectMap)
		{
			totalObjectsDirty += kvp.Value.Num();
		}

		combinedDirtyStateHashes.Add(combinedDirtyStateHash, &bOldDirtyState);

	} while ((totalObjectsDirty > 0) && !bOldDirtyState &&
		ensureMsgf(--combinedDirtySafeguard > 0, TEXT("Infinite loop detected while cleaning combined dirty flags, breaking!")));

	// If objects are still dirty after exhausting the combinedDirtySafeguard, then delete the objects if we are generating side effects.
	if ((totalObjectsDirty > 0) && OutSideEffectDeltas && bDeleteUncleanableObjects)
	{
		TSet<AModumateObjectInstance*> uncleanableObjs;
		for (auto& kvp : DirtyObjectMap)
		{
			for (auto* object : kvp.Value)
			{   // Don't delete uncleanable groups due to danger.
				if (object->GetObjectType() != EObjectType::OTMetaGraph)
				{
					uncleanableObjs.Add(object);
				}
			}
		}

		UE_LOG(LogTemp, Error, TEXT("Deleting %d uncleanable objects!"), uncleanableObjs.Num());

		TArray<FDeltaPtr> deletionDeltas;
		if (GetDeleteObjectsDeltas(deletionDeltas, uncleanableObjs.Array(), false, false))
		{
			OutSideEffectDeltas->Append(deletionDeltas);
		}
	}

	return (totalObjectCleans > 0);
}

void UModumateDocument::RegisterDirtyObject(EObjectDirtyFlags DirtyType, AModumateObjectInstance *DirtyObj, bool bDirty)
{
	// Make sure only one dirty flag is used at a time
	int32 flagInt = (int32)DirtyType;
	if (!ensureAlwaysMsgf((DirtyObj != nullptr) && (DirtyType != EObjectDirtyFlags::None) && ((flagInt & (flagInt - 1)) == 0),
		TEXT("Must mark a non-null object with exactly one dirty flag type at a time!")))
	{
		return;
	}

	TArray<AModumateObjectInstance*> &dirtyObjList = DirtyObjectMap.FindOrAdd(DirtyType);

	if (bDirty)
	{
		// We can use TArray with Add rather than AddUnique or TSet because objects already check against their own
		// dirty flag before registering themselves with the Document.
		dirtyObjList.Add(DirtyObj);

		if (bTrackingDeltaObjects)
		{
			DeltaDirtiedObjects.Add(DirtyObj->ID);
		}
	}
	else
	{
		dirtyObjList.Remove(DirtyObj);
	}
}

void UModumateDocument::MakeNew(UWorld *World, bool bClearName)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeNew"));

	int32 numObjects = ObjectInstanceArray.Num();
	for (int32 i = numObjects - 1; i >= 0; --i)
	{
		AModumateObjectInstance* obj = ObjectInstanceArray[i];

		ObjectsByType.FindOrAdd(obj->GetObjectType()).Remove(obj->ID);
		ObjectsByID.Remove(obj->ID);
		ObjectInstanceArray.RemoveAt(i, 1, false);

		obj->Destroy();
	}

	for (auto& kvp : DeletedObjects)
	{
		if (kvp.Value)
		{
			kvp.Value->Destroy();
		}
	}
	DeletedObjects.Reset();

	for (auto &kvp : DirtyObjectMap)
	{
		kvp.Value.Reset();
	}

	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(World->GetFirstPlayerController());
	if (controller && controller->DynamicIconGenerator)
	{
		TArray<FGuid> affectedAssemblies;
		controller->DynamicIconGenerator->UpdateCachedAssemblies(FBIMPresetCollectionProxy(GetPresetCollection()), affectedAssemblies);
	}

	// Clear drafting render directories
	UModumateGameInstance* gameInstance = World ? World->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager *draftMan = gameInstance ? gameInstance->DraftingManager : nullptr;
	if (draftMan != nullptr)
	{
		draftMan->Reset();
	}

	NextID = MPObjIDFromLocalObjID(1, CachedLocalUserIdx);

	ClearRedoBuffer();
	ClearUndoBuffer();

	InitialDocHash = 0;
	UnverifiedDeltasRecords.Reset();
	VerifiedDeltasRecords.Reset();
	UndoRedoMacroStack.Reset();
	VolumeGraphs.Reset();
	GraphElementsToGraph3DMap.Reset();
	TempVolumeGraph.Reset();
	SurfaceGraphs.Reset();

	RootVolumeGraph = NextID++;
	FMOIStateData rootGraphState(RootVolumeGraph, EObjectType::OTMetaGraph);
	rootGraphState.CustomData.SaveStructData<FMOIMetaGraphData>(FMOIMetaGraphData());
	CreateOrRestoreObj(World, rootGraphState);

	VolumeGraphs.Add(RootVolumeGraph, MakeShared<FGraph3D>(RootVolumeGraph));
	ActiveVolumeGraph = RootVolumeGraph;
	TempVolumeGraph.GraphID = ActiveVolumeGraph;

	RootDesignOptionID = NextID++;
	FMOIStateData rootDesignOptionState(RootDesignOptionID, EObjectType::OTDesignOption);
	rootDesignOptionState.CustomData.SaveStructData<FMOIDesignOptionData>(FMOIDesignOptionData());
	CreateOrRestoreObj(World, rootDesignOptionState);

	if (bClearName)
	{
		SetCurrentProjectName();
	}

	SetDirtyFlags(false);
}

const AModumateObjectInstance *UModumateDocument::ObjectFromActor(const AActor *actor) const
{
	return const_cast<UModumateDocument*>(this)->ObjectFromActor(const_cast<AActor*>(actor));
}

AModumateObjectInstance *UModumateDocument::ObjectFromSingleActor(AActor *actor)
{
	auto *moiComponent = actor ? actor->FindComponentByClass<UModumateObjectComponent>() : nullptr;
	if (moiComponent && (moiComponent->ObjectID != MOD_ID_NONE))
	{
		return GetObjectById(moiComponent->ObjectID);
	}

	return nullptr;
}

AModumateObjectInstance *UModumateDocument::ObjectFromActor(AActor *actor)
{
	AModumateObjectInstance *moi = ObjectFromSingleActor(actor);

	while ((moi == nullptr) && (actor != nullptr))
	{
		actor = actor->GetAttachParentActor();
		moi = ObjectFromSingleActor(actor);
	}

	return moi;
}

TArray<AModumateObjectInstance*> UModumateDocument::GetObjectsOfType(EObjectType Type)
{
	TArray<AModumateObjectInstance*> objects;
	auto* objectsOfType = ObjectsByType.Find(Type);
	if (objectsOfType)
	{
		for (int32 objectID : *objectsOfType)
		{
			AModumateObjectInstance* obj = GetObjectById(objectID);
			if (obj)
			{
				objects.Add(obj);
			}
		}
	}

	return objects;
}

TArray<const AModumateObjectInstance*> UModumateDocument::GetObjectsOfType(EObjectType Type) const
{
	TArray<const AModumateObjectInstance*> objects;
	auto* objectsOfType = ObjectsByType.Find(Type);
	if (objectsOfType)
	{
		for (int32 objectID : *objectsOfType)
		{
			const AModumateObjectInstance* obj = GetObjectById(objectID);
			if (obj)
			{
				objects.Add(obj);
			}
		}
	}

	return objects;
}

TArray<const AModumateObjectInstance*> UModumateDocument::GetObjectsOfType(const FObjectTypeSet& Types) const
{
	TArray<const AModumateObjectInstance*> outArray;
	for (auto type: Types)
	{
		outArray.Append(GetObjectsOfType(type));
	}
	return outArray;
}

TArray<AModumateObjectInstance*> UModumateDocument::GetObjectsOfType(const FObjectTypeSet& Types)
{
	TArray<AModumateObjectInstance*> outArray;
	for (auto type: Types)
	{
		outArray.Append(GetObjectsOfType(type));
	}
	return outArray;
}

void UModumateDocument::GetObjectIdsByAssembly(const FGuid& AssemblyKey, TArray<int32>& OutIds) const
{
	for (const auto &moi : ObjectInstanceArray)
	{
		if (moi->GetAssembly().UniqueKey() == AssemblyKey)
		{
			OutIds.Add(moi->ID);
		}
	}
}

bool UModumateDocument::ExportDWG(UWorld * world, const TCHAR * filepath, TArray<int32> InCutPlaneIDs)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ExportDWG"));
	CurrentDraftingView = MakeShared<FModumateDraftingView>(world, this, UDraftingManager::kDWG);
	CurrentDraftingView->CurrentFilePath = FString(filepath);
	CurrentDraftingView->GeneratePagesFromCutPlanes(InCutPlaneIDs);

	return true;
}

bool UModumateDocument::SerializeRecords(UWorld* World, FModumateDocumentHeader& OutHeader, FMOIDocumentRecord& OutDocumentRecord)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Serialize"));

	// Header is its own object
	OutHeader.Version = DocVersion;
	OutHeader.DocumentHash = GetLatestVerifiedDocHash();
	OutHeader.Thumbnail = CurrentEncodedThumbnail;

	EToolMode modes[] = {
		EToolMode::VE_PLACEOBJECT,
		EToolMode::VE_WALL,
		EToolMode::VE_FLOOR,
		EToolMode::VE_DOOR,
		EToolMode::VE_WINDOW,
		EToolMode::VE_STAIR,
		EToolMode::VE_RAIL,
		EToolMode::VE_CABINET,
		EToolMode::VE_FINISH,
		EToolMode::VE_COUNTERTOP,
		EToolMode::VE_STRUCTURELINE,
		EToolMode::VE_TRIM,
		EToolMode::VE_ROOF_FACE,
		EToolMode::VE_CEILING
	};

	AEditModelPlayerController* emPlayerController = Cast<AEditModelPlayerController>(World->GetFirstPlayerController());
	if (emPlayerController)
	{
		for (auto& mode : modes)
		{
			TScriptInterface<IEditModelToolInterface> tool = emPlayerController->ModeToTool.FindRef(mode);
			if (ensureAlways(tool))
			{
				OutDocumentRecord.CurrentToolAssemblyGUIDMap.Add(mode, tool->GetAssemblyGUID());
			}
		}
	}
	
	// Capture object instances into doc struct
	for (AModumateObjectInstance* obj : ObjectInstanceArray)
	{
		// Don't save graph-reflected MOIs, since their information is stored in separate graph structures
		EObjectType objectType = obj ? obj->GetObjectType() : EObjectType::OTNone;
		EGraph3DObjectType graph3DType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);
		EGraphObjectType graph2DType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(objectType);
		if (obj && (graph3DType == EGraph3DObjectType::None) && (graph2DType == EGraphObjectType::None))
		{
			FMOIStateData& stateData = obj->GetStateData();
			stateData.CustomData.SaveJsonFromCbor();
			OutDocumentRecord.ObjectData.Add(stateData);
		}
	}

	// Web Presets do not get saved, only presets used in the document or edited.
	BIMPresetCollection.SavePresetsToDocRecord(OutDocumentRecord);

	// For compatibility with older clients
	GetVolumeGraph(RootVolumeGraph)->Save(&OutDocumentRecord.VolumeGraph);
	
	OutDocumentRecord.RootVolumeGraph = RootVolumeGraph;
	for (const auto& graph3dKvp: VolumeGraphs)
	{
		graph3dKvp.Value->Save(&OutDocumentRecord.VolumeGraphs.Add(graph3dKvp.Key));
	}

	// Save all of the surface graphs as records
	for (const auto &kvp : SurfaceGraphs)
	{
		auto& surfaceGraph = kvp.Value;
		FGraph2DRecord& surfaceGraphRecord = OutDocumentRecord.SurfaceGraphs.Add(kvp.Key);
		surfaceGraph->ToDataRecord(&surfaceGraphRecord, true, true);
	}

	OutDocumentRecord.TypicalEdgeDetails = TypicalEdgeDetails;
	OutDocumentRecord.DrawingDesignerDocument = DrawingDesignerDocument;
	OutDocumentRecord.RootDesignOptionID = RootDesignOptionID;

	// Potentially limit the number of undo records to save, based on user preferences
	auto* gameInstance = World->GetGameInstance<UModumateGameInstance>();
	int32 numUndoRecords = VerifiedDeltasRecords.Num();
	int32 numUndoRecordsToSave = numUndoRecords;
	if (gameInstance && (gameInstance->UserSettings.SaveFileUndoHistoryLength >= 0))
	{
		numUndoRecordsToSave = FMath::Min(numUndoRecords, gameInstance->UserSettings.SaveFileUndoHistoryLength);
	}

	for (int32 i = (numUndoRecords - numUndoRecordsToSave); i < numUndoRecords; ++i)
	{
		OutDocumentRecord.AppliedDeltas.Add(VerifiedDeltasRecords[i]);
	}

	OutDocumentRecord.Settings = CurrentSettings;

	return true;
}

bool UModumateDocument::SaveRecords(const FString& FilePath, const FModumateDocumentHeader& InHeader, const FMOIDocumentRecord& InDocumentRecord)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateDocumentSaveRecords);

	TSharedPtr<FJsonObject> FileJson = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> HeaderJson = MakeShared<FJsonObject>();
	FileJson->SetObjectField(FModumateSerializationStatics::DocHeaderField, FJsonObjectConverter::UStructToJsonObject<FModumateDocumentHeader>(InHeader));

	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<FMOIDocumentRecord>(InDocumentRecord);
	FileJson->SetObjectField(FModumateSerializationStatics::DocObjectInstanceField, docOb);

	FString ProjectJsonString;
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&ProjectJsonString);

	bool bWriteJsonSuccess = FJsonSerializer::Serialize(FileJson.ToSharedRef(), JsonStringWriter);
	if (!bWriteJsonSuccess)
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(ProjectJsonString, *FilePath);
}

bool UModumateDocument::SaveFile(UWorld* World, const FString& FilePath, bool bUserFile, bool bAsync, const TFunction<void(bool)>& OnSaveFunction)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateDocumentSaveFile);

	double saveStartTime = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Log, TEXT("Saving to %s..."), *FilePath);

	CachedHeader = FModumateDocumentHeader();
	CachedRecord = FMOIDocumentRecord();
	if (!SerializeRecords(World, CachedHeader, CachedRecord))
	{
		return false;
	}

	bool bFileSaveSuccess = false;
	auto weakThis = MakeWeakObjectPtr<UModumateDocument>(this);
	auto weakWorld = MakeWeakObjectPtr<UWorld>(World);

	TFuture<bool> saveFuture = Async(EAsyncExecution::ThreadPool,
		[FilePath, DocHeaderCopy = CachedHeader, DocRecordCopy = CachedRecord, &bFileSaveSuccess]()
		{
			bFileSaveSuccess = SaveRecords(FilePath, DocHeaderCopy, DocRecordCopy);
			return bFileSaveSuccess;
		},
		[FilePath, bAsync, bUserFile, OnSaveFunction, saveStartTime, &bFileSaveSuccess, weakThis, weakWorld]()
		{
			if (weakThis.IsValid() && weakWorld.IsValid())
			{
				if (bFileSaveSuccess)
				{
					int32 saveDurationMS = FMath::RoundToInt((FPlatformTime::Seconds() - saveStartTime) * 1000.0);
					int64 fileSizeKB = IFileManager::Get().FileSize(*FilePath) / 1024;
					UE_LOG(LogTemp, Log, TEXT("Document %ssave success after %dms (%lldkB)"),
						bAsync ? TEXT("async ") : TEXT(""), saveDurationMS, fileSizeKB);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Document error while saving to: %s"), *FilePath);
				}

				if (bFileSaveSuccess)
				{
					weakThis->RecordSavedProject(weakWorld.Get(), FilePath, bUserFile);
				}

				if (OnSaveFunction)
				{
					OnSaveFunction(bFileSaveSuccess);
				}
			}
		}
		);

	if (bAsync)
	{
		return true;
	}
	else
	{
		saveFuture.Wait();
		return saveFuture.Get();
	}
}

AModumateObjectInstance *UModumateDocument::GetObjectById(int32 id)
{
	return ObjectsByID.FindRef(id);
}

const AModumateObjectInstance *UModumateDocument::GetObjectById(int32 id) const
{
	return ObjectsByID.FindRef(id);
}

bool UModumateDocument::LoadRecord(UWorld* world, const FModumateDocumentHeader& InHeader, FMOIDocumentRecord& InDocumentRecord, bool bClearName)
{
#if !UE_SERVER
	FDateTime loadStartTime = FDateTime::Now();
#endif
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::LoadRecord"));

	//Get player state and tells it to empty selected object
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(world->GetFirstPlayerController());
	AEditModelPlayerState* playerState = controller ? controller->EMPlayerState : nullptr;
	if (playerState)
	{
		playerState->OnNewModel();
	}

	MakeNew(world, bClearName);
	bool bInitialDocumentDirty = false;

	for (auto* ob : ObjectInstanceArray)
	{
		ObjectsByType.FindOrAdd(ob->GetObjectType()).Remove(ob->ID);
		ObjectsByID.Remove(ob->ID);
		ob->Destroy();
	}

	ObjectInstanceArray.Empty();
	VolumeGraphs.Reset();

	UModumateGameInstance* gameInstance = world->GetGameInstance<UModumateGameInstance>();
	BIMPresetCollection.ReadPresetsFromDocRecord(InHeader.Version, InDocumentRecord, gameInstance);


	// Load the connectivity graphs now, which contain associations between object IDs,
	// so that any objects whose geometry setup needs to know about connectivity can find it.
	bool bSuccessfulGraphLoad = true;
	int32 legacyGraphID = 1;
	// Is loaded project a legacy project without metaGraph MOIs?
	const bool bNonGroupProject = InDocumentRecord.ObjectData.FindByPredicate([](const FMOIStateData& sd) { return sd.ObjectType == EObjectType::OTMetaGraph; }) == nullptr;
	if (bNonGroupProject)
	{	// Document has one global volume graph.
		VolumeGraphs.Add(legacyGraphID) = MakeShared<FGraph3D>(legacyGraphID);
		bSuccessfulGraphLoad = GetVolumeGraph(legacyGraphID)->Load(&InDocumentRecord.VolumeGraph);
		RootVolumeGraph = legacyGraphID;
	}
	else
	{
		for (const auto& volumeGraphKvp : InDocumentRecord.VolumeGraphs)
		{
			TSharedPtr<FGraph3D> volumeGraph = MakeShared<FGraph3D>(volumeGraphKvp.Key);
			bSuccessfulGraphLoad &= volumeGraph->Load(&volumeGraphKvp.Value);
			VolumeGraphs.Add(volumeGraphKvp.Key, volumeGraph);
		}
		RootVolumeGraph = InDocumentRecord.RootVolumeGraph;
	}
	ActiveVolumeGraph = RootVolumeGraph;


	FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph());

	// If some elements didn't load in correctly, then mark the document as initially dirty,
	// since it may indicate that undo operations could break the graph, or that it should be re-saved before clients join.
	if (!bSuccessfulGraphLoad)
	{
		bInitialDocumentDirty = true;
	}

	DrawingDesignerDocument = InDocumentRecord.DrawingDesignerDocument;

	// Load all of the surface graphs now
	for (const auto& kvp : InDocumentRecord.SurfaceGraphs)
	{
		const FGraph2DRecord& surfaceGraphRecord = kvp.Value;
		TSharedPtr<FGraph2D> surfaceGraph = MakeShared<FGraph2D>(kvp.Key);
		if (surfaceGraph->FromDataRecord(&surfaceGraphRecord))
		{
			SurfaceGraphs.Add(kvp.Key, surfaceGraph);
		}
	}

	TypicalEdgeDetails = InDocumentRecord.TypicalEdgeDetails;

	// Create the MOIs whose state data was stored
	NextID = MPObjIDFromLocalObjID(1, CachedLocalUserIdx);
	for (auto& stateData : InDocumentRecord.ObjectData)
	{
		CreateOrRestoreObj(world, stateData);
	}

	// Create MOIs reflected from the volume graphs
	for (const auto& graph3d : VolumeGraphs)
	{
		for (const auto& kvp : graph3d.Value->GetAllObjects())
		{
			GraphElementsToGraph3DMap.Add(kvp.Key) = graph3d.Key;
		}

		for (const auto& kvp : graph3d.Value->GetAllObjects())
		{
			EObjectType objectType = UModumateTypeStatics::ObjectTypeFromGraph3DType(kvp.Value);
			if (ensure(!ObjectsByID.Contains(kvp.Key)) && (objectType != EObjectType::OTNone))
			{
				CreateOrRestoreObj(world, FMOIStateData(kvp.Key, objectType));
			}
		}
	}

	RootDesignOptionID = InDocumentRecord.RootDesignOptionID;
	// Only the server can reliably fix this
	if (RootDesignOptionID == MOD_ID_NONE && ensure(!world->IsNetMode(NM_Client)))
	{
		TArray<AMOIDesignOption*> designOptions;
		GetObjectsOfTypeCasted(EObjectType::OTDesignOption,designOptions);

		RootDesignOptionID = NextID++;

		// No delta needed during load...server side with no undo/redo
		for (auto* designOption : designOptions)
		{
			if (designOption->GetParentID() == MOD_ID_NONE)
			{
				designOption->SetParentID(RootDesignOptionID);
			}
		}

		FMOIStateData rootDesignOptionState(RootDesignOptionID, EObjectType::OTDesignOption);
		rootDesignOptionState.CustomData.SaveStructData<FMOIDesignOptionData>(FMOIDesignOptionData());
		CreateOrRestoreObj(GetWorld(), rootDesignOptionState);
	}

	// Create MOIs reflected from the surface and terrain graphs
	TArray<int32> graph2DsToRemove;
	for (const auto& surfaceGraphKVP : SurfaceGraphs)
	{
		if (surfaceGraphKVP.Value.IsValid())
		{
			EToolCategories graphCategory = EToolCategories::SurfaceGraphs;
			const auto graphMOI = GetObjectById(surfaceGraphKVP.Key);
			if (!ensureMsgf(graphMOI, TEXT("Graph2D #%d didn't get a MOI in the Document!"), surfaceGraphKVP.Key))
			{
				graph2DsToRemove.Add(surfaceGraphKVP.Key);
				continue;
			}

			if (graphMOI->GetObjectType() == EObjectType::OTTerrain)
			{
				graphCategory = EToolCategories::SiteTools;
			}

			for (const auto& surfaceGraphObjKVP : surfaceGraphKVP.Value->GetAllObjects())
			{
				EObjectType objectType = UModumateTypeStatics::ObjectTypeFromGraph2DType(surfaceGraphObjKVP.Value, graphCategory);
				if (ensure(!ObjectsByID.Contains(surfaceGraphObjKVP.Key)) && (objectType != EObjectType::OTNone))
				{
					CreateOrRestoreObj(world, FMOIStateData(surfaceGraphObjKVP.Key, objectType, surfaceGraphKVP.Key));
				}
			}
		}
	}

	// If any Graph2D didn't have a corresponding MOI (due a deletion error), then remove it now so that it doesn't continue to pollute the document.
	for (int32 graph2DToRemove : graph2DsToRemove)
	{
		SurfaceGraphs.Remove(graph2DToRemove);
	}

	// Add a MetaGraph MOI for backwards compatibility.
	if (bNonGroupProject && ensureAlways(VolumeGraphs.Contains(legacyGraphID)))
	{   // Create MOI and remap to new ID
		auto volumeGraph = VolumeGraphs[legacyGraphID];
		legacyGraphID = NextID++;
		RootVolumeGraph = legacyGraphID;
		ActiveVolumeGraph = RootVolumeGraph;
		FMOIStateData rootGraphState(RootVolumeGraph, EObjectType::OTMetaGraph);
		rootGraphState.CustomData.SaveStructData<FMOIMetaGraphData>(FMOIMetaGraphData());
		CreateOrRestoreObj(world, rootGraphState);
		for (auto& graphMap : GraphElementsToGraph3DMap)
		{
			graphMap.Value = legacyGraphID;
		}
		VolumeGraphs.Reset();
		volumeGraph->GraphID = legacyGraphID;
		VolumeGraphs.Add(RootVolumeGraph) = volumeGraph;
	}

	// Prior to version 21, plane and edge hosted objects had graph objects for parents
	// After version 21, these objects require span parents
	if (InHeader.Version < 21 && !world->IsNetMode(NM_Client))
	{
		bInitialDocumentDirty = true;
		// ObjectInstanceArray will be affected in the loop, so copy the original
		TArray<AModumateObjectInstance*> obs = ObjectInstanceArray;
		for (auto* moi : obs)
		{
			FMOIStateData spanState;
			spanState.ParentID = 0;

			UClass* const moiClass = moi->GetClass();
			if (moiClass == AMOIPlaneHostedObj::StaticClass() ||
				moiClass == AMOIFaceHosted::StaticClass())
			{
				spanState.ID = NextID++;
				spanState.ObjectType = EObjectType::OTMetaPlaneSpan;

				FMOIMetaPlaneSpanData spanData;
				spanData.GraphMembers.Add(moi->GetParentID());
				spanState.CustomData.SaveStructData(spanData);
			}
			else if (moiClass == AMOIEdgeHosted::StaticClass() ||
				moiClass == AMOIStructureLine::StaticClass() ||
				moiClass == AMOIMullion::StaticClass())
			{
				spanState.ID = NextID++;
				spanState.ObjectType = EObjectType::OTMetaEdgeSpan;

				FMOIMetaEdgeSpanData spanData;
				spanData.GraphMembers.Add(moi->GetParentID());
				spanState.CustomData.SaveStructData(spanData);
			}

			if (spanState.ObjectType != EObjectType::OTNone)
			{
				CreateOrRestoreObj(world, spanState);
				moi->SetParentID(spanState.ID);
			}
		}
	}

	// Version 22, portals require span parents
	if (InHeader.Version < 22 && !world->IsNetMode(NM_Client))
	{
		bInitialDocumentDirty = true;
		TArray<AModumateObjectInstance*> portalObjs = GetObjectsOfType(EObjectType::OTDoor);
		TArray<AModumateObjectInstance*> objWindows = GetObjectsOfType(EObjectType::OTWindow);
		portalObjs.Append(objWindows);

		for (auto* portalObj : portalObjs)
		{
			// Create and reparent portal to MetaPlaneSpan
			if (portalObj->GetParentObject() &&
				portalObj->GetParentObject()->GetObjectType() != EObjectType::OTMetaPlaneSpan)
			{
				FMOIStateData spanState;
				spanState.ParentID = 0;
				spanState.ID = NextID++;
				spanState.ObjectType = EObjectType::OTMetaPlaneSpan;

				FMOIMetaPlaneSpanData spanData;
				spanData.GraphMembers.Add(portalObj->GetParentID());
				spanState.CustomData.SaveStructData(spanData);

				CreateOrRestoreObj(world, spanState);
				portalObj->SetParentID(spanState.ID);
			}
		}
	}
	
	if(InHeader.Version < 25 && !world->IsNetMode(NM_Client))
	{
	
		//Fix any drawing designer nodes that may have lost their children
		for(auto& node : DrawingDesignerDocument.nodes)
		{
			int32 childId = FCString::Atoi(*node.Key);
			if(node.Value.parent != INDEX_NONE)
			{
				auto parentId = FString::FromInt(node.Value.parent);
				if(ensureAlways(DrawingDesignerDocument.nodes.Contains(parentId)))
				{
					auto& parent = DrawingDesignerDocument.nodes[parentId];
					if(!parent.children.Contains(childId))
					{
						parent.children.Add(childId);
						UE_LOG(LogTemp, Warning, TEXT("Drawing node lost child, fixing. parent=%s, child=%s"), *parentId, *node.Key);
						bInitialDocumentDirty = true;
					}
				}			
			}
		}
	}
	
	
	//Even if no changes occur, push the doc version update to the cloud
	if (InHeader.Version < DocVersion)
	{
		bInitialDocumentDirty = true;
	}
    	
	for (const auto* spanMoi : GetObjectsOfType({ EObjectType::OTMetaEdgeSpan, EObjectType::OTMetaPlaneSpan }))
	{
		UpdateSpanData(spanMoi);
	}

	// Now that all objects have been created and parented correctly, we can clean all of them.
	// This should take care of anything that depends on relationships between objects, like mitering.
	TArray<FDeltaPtr> loadCleanSideEffects;

	CleanObjects(&loadCleanSideEffects, true, true);

	// If there were side effects generated while cleaning initial objects, then those deltas must be applied immediately.
	// This should not normally happen, but it's a sign that some objects relied on data that didn't get saved/loaded correctly.
	int32 numLoadCleanSideEffects = loadCleanSideEffects.Num();
	if (numLoadCleanSideEffects > 0)
	{
		// TODO: potentially alert the user that some cleanup step has occurred, which is why the document is now dirty and needs to be re-saved.
		// As long as this step happens deterministically, these deltas hopefully won't need to be shared among different clients all loading the same document.
		// However, they might break serialized the undo buffer, so we will have to skip deserializing it.
		UE_LOG(LogTemp, Warning, TEXT("CleanObjects generated %d side effects during initial Document load! Attemping to clean."), numLoadCleanSideEffects);
		
		bInitialDocumentDirty = ApplyDeltas(loadCleanSideEffects, world) || bInitialDocumentDirty;
	}

	// Check for objects that have errors, as a result of having been loaded and cleaned.
	// TODO: prompt the user to fix, or delete, these objects
	for (auto* obj : ObjectInstanceArray)
	{
		if (playerState && playerState->DoesObjectHaveAnyError(obj->ID))
		{
			FString objectTypeString = GetEnumValueString(obj->GetObjectType());
			UE_LOG(LogTemp, Warning, TEXT("MOI %d (%s) has an error!"), obj->ID, *objectTypeString);
		}
	}

	if (controller)
	{
		for (auto& cta : InDocumentRecord.CurrentToolAssemblyGUIDMap)
		{
			TScriptInterface<IEditModelToolInterface> tool = controller->ModeToTool.FindRef(cta.Key);
			if (ensureAlways(tool))
			{
				const FBIMAssemblySpec* obAsm = GetPresetCollection().GetAssemblyByGUID(cta.Key, cta.Value);
				if (obAsm != nullptr)
				{
					tool->SetAssemblyGUID(obAsm->UniqueKey());
				}
			}
		}
	}

	// Hide all cut planes on load
	TArray<AModumateObjectInstance*> cutPlanes = GetObjectsOfType(EObjectType::OTCutPlane);
	TArray<int32> hideCutPlaneIds;
	for (auto curCutPlane : cutPlanes)
	{
		hideCutPlaneIds.Add(curCutPlane->ID);
	}
	if (controller)
	{
		controller->GetPlayerState<AEditModelPlayerState>()->AddHideObjectsById(hideCutPlaneIds);
	}

	InitialDocHash = InHeader.DocumentHash;
	UnverifiedDeltasRecords.Reset();
	VerifiedDeltasRecords = InDocumentRecord.AppliedDeltas;

	ClearUndoBuffer();

	// Sanity-check our scheme for saving the document hash in the header, that should always match up with the DeltasRecords stored in the record.
	uint32 loadedDeltasHash = GetLatestVerifiedDocHash();
	if (InHeader.DocumentHash != loadedDeltasHash)
	{
		UE_LOG(LogTemp, Error, TEXT("Load failure; mismatch between DocumentRecord's DeltasRecords hash (%08x) and DocumentHeader's DocumentHash (%08x) with (%d) records!"),
			loadedDeltasHash, InHeader.DocumentHash, VerifiedDeltasRecords.Num());
	}

	// If the file version is out of date, or if side effects were done upon load, then DeltasRecords may not be safely used for undo.
	// Wipe the records so that they can't undone, but set InitialDocHash so that we can still get the oldest valid doc hash.
	if (world->IsNetMode(NM_DedicatedServer) || bInitialDocumentDirty || (DocVersion != InHeader.Version))
	{
		InitialDocHash = GetLatestVerifiedDocHash();
		VerifiedDeltasRecords.Reset();
	}

	CurrentSettings = InDocumentRecord.Settings;
	if (controller && controller->SkyActor)
	{
		controller->SkyActor->UpdateCoordinate(CurrentSettings.Latitude, CurrentSettings.Longitude, CurrentSettings.TrueNorthDegree);
	}

	SetDirtyFlags(bInitialDocumentDirty);

#if !UE_SERVER
	UpdateWebPresets();
	FTimespan duration = FDateTime::Now() - loadStartTime;
	static const FString eventName(TEXT("TimeClientDocumentLoad"));
	UModumateAnalyticsStatics::RecordEventCustomFloat(GetWorld(), EModumateAnalyticsCategory::Network, eventName, duration.GetTotalSeconds());
	UE_LOG(LogTemp, Log, TEXT("Loaded document in %0.2f seconds"), duration.GetTotalSeconds());
#endif

	return true;
}

bool UModumateDocument::LoadFile(UWorld* world, const FString& path, bool bSetAsCurrentProject, bool bRecordAsRecentProject)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::LoadFile"));

	CachedHeader = FModumateDocumentHeader();
	CachedRecord = FMOIDocumentRecord();
	if (!FModumateSerializationStatics::TryReadModumateDocumentRecord(path, CachedHeader, CachedRecord))
	{
		return false;
	}

	if (!LoadRecord(world, CachedHeader, CachedRecord))
	{
		return false;
	}

	if (bSetAsCurrentProject)
	{
		SetCurrentProjectName(path);
	}

	auto* gameInstance = world->GetGameInstance<UModumateGameInstance>();
	if (bRecordAsRecentProject && gameInstance)
	{
		gameInstance->UserSettings.RecordRecentProject(path, true);
	}

	return true;
}

bool UModumateDocument::LoadDeltas(UWorld* world, const FString& path, bool bSetAsCurrentProject, bool bRecordAsRecentProject)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Load"));

	//Get player state and tells it to empty selected object
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(world->GetFirstPlayerController());
	AEditModelPlayerState* playerState = controller ? controller->EMPlayerState : nullptr;
	if (playerState)
	{
		playerState->OnNewModel();
	}

	MakeNew(world);

	CachedHeader = FModumateDocumentHeader();
	CachedRecord = FMOIDocumentRecord();
	if (FModumateSerializationStatics::TryReadModumateDocumentRecord(path, CachedHeader, CachedRecord))
	{
		// Load all of the deltas into the redo buffer, as if the user had undone all the way to the beginning
		// the redo buffer expects the deltas to all be backwards
		for (int urIdx = CachedRecord.AppliedDeltas.Num() - 1; urIdx >= 0; urIdx--)
		{
			auto& deltaRecord = CachedRecord.AppliedDeltas[urIdx];
			TSharedPtr<UndoRedo> undoRedo = MakeShared<UndoRedo>();

			for (int deltaIdx = 0; deltaIdx < deltaRecord.DeltaStructWrappers.Num(); deltaIdx++)
			{
				auto& structWrapper = deltaRecord.DeltaStructWrappers[deltaIdx];
				if (!ensure(structWrapper.LoadFromJson()))
				{
					continue;
				}

				auto deltaPtr = structWrapper.CreateStructFromJSON<FDocumentDelta>();
				if (deltaPtr)
				{
					undoRedo->Deltas.Add(MakeShareable(deltaPtr));
				}
			}

			// Copied from PerformUndoRedo, apply deltas to the redo buffer in this way so that 
			// when they are popped off, they are applied in the right direction
			TArray<FDeltaPtr> fromDeltas = undoRedo->Deltas;
			Algo::Reverse(fromDeltas);

			undoRedo->Deltas.Empty();
			Algo::Transform(fromDeltas, undoRedo->Deltas, [](const FDeltaPtr& DeltaPtr) {return DeltaPtr->MakeInverse(); });
			RedoBuffer.Add(undoRedo);
		}
	}

	return true;
}

TArray<int32> UModumateDocument::CloneObjects(UWorld *world, const TArray<int32> &objs, const FTransform& offsetTransform)
{
	// make a list of MOI objects from the IDs, send them to the MOI version of this function return array of IDs.

	TArray<AModumateObjectInstance*> obs;
	Algo::Transform(objs,obs,[this](auto id) {return GetObjectById(id); });

	obs = CloneObjects(
		world,
		obs,
		offsetTransform)
		.FilterByPredicate([](AModumateObjectInstance *ob) {return ob != nullptr; });

	TArray<int32> ids;

	Algo::Transform(obs,ids,[this](auto *ob){return ob->ID;});

	return ids;
}

int32 UModumateDocument::CloneObject(UWorld *world, const AModumateObjectInstance *original)
{
	// TODO: reimplement
	return MOD_ID_NONE;
}

TArray<AModumateObjectInstance *> UModumateDocument::CloneObjects(UWorld *world, const TArray<AModumateObjectInstance *> &objs, const FTransform& offsetTransform)
{
	TMap<AModumateObjectInstance*, AModumateObjectInstance*> oldToNew;

	BeginUndoRedoMacro();

	// Step 1 of clone macro: deserialize the objects, each of which will have its own undo/redo step
	TArray<AModumateObjectInstance*> newObjs;
	Algo::Transform(
		objs,
		newObjs,
		[this,world,&oldToNew,offsetTransform](AModumateObjectInstance *obj)
		{
			AModumateObjectInstance *newOb = GetObjectById(CloneObject(world, obj));
			oldToNew.Add(obj, newOb);
			return newOb;
		}
	);

	// Set up the correct parenting for the newly-cloned objects
	for (auto newObj : newObjs)
	{
		AModumateObjectInstance *oldParent = newObj->GetParentObject();
		AModumateObjectInstance *newParent = oldToNew.FindRef(oldParent);

		if (oldParent && newParent)
		{
			newObj->SetParentID(newParent->ID);
		}
	}

	EndUndoRedoMacro();

	return newObjs;
}

void UModumateDocument::SetCurrentProjectName(const FString& NewProjectName, bool bAsPath)
{
	if (bAsPath)
	{
		CurrentProjectPath = NewProjectName;
		CurrentProjectName = FPaths::GetBaseFilename(CurrentProjectPath);
	}
	else
	{
		CurrentProjectPath.Empty();
		CurrentProjectName = NewProjectName;
	}

	UpdateWindowTitle();
}

void UModumateDocument::UpdateMitering(UWorld *world, const TArray<int32> &dirtyObjIDs)
{
	TSet<int32> dirtyPlaneIDs;

	FGraph3D* volumeGraph = GetVolumeGraph();
	// Gather all of the dirty planes that need to be updated with new miter geometry
	for (int32 dirtyObjID : dirtyObjIDs)
	{
		AModumateObjectInstance *dirtyObj = GetObjectById(dirtyObjID);
		if (dirtyObj)
		{
			switch (dirtyObj->GetObjectType())
			{
			case EObjectType::OTMetaEdge:
			{
				const FGraph3DEdge *graphEdge = volumeGraph->FindEdge(dirtyObjID);
				for (const FEdgeFaceConnection &edgeFaceConnection : graphEdge->ConnectedFaces)
				{
					dirtyPlaneIDs.Add(FMath::Abs(edgeFaceConnection.FaceID));
				}
				break;
			}
			case EObjectType::OTMetaPlane:
			{
				dirtyPlaneIDs.Add(dirtyObjID);

				const FGraph3DFace *graphFace = volumeGraph->FindFace(dirtyObjID);
				graphFace->GetAdjacentFaceIDs(dirtyPlaneIDs);
				break;
			}
			case EObjectType::OTMetaVertex:
			{
				const FGraph3DVertex *graphVertex = volumeGraph->FindVertex(dirtyObjID);
				graphVertex->GetConnectedFaces(dirtyPlaneIDs);
				break;
			}
			default:
			{
				AModumateObjectInstance *parent = dirtyObj->GetParentObject();
				if (parent && (parent->GetObjectType() == EObjectType::OTMetaPlane))
				{
					dirtyPlaneIDs.Add(parent->ID);
				}
				break;
			}
			}
		}
	}

	// Miter-aware geometry updates only require UpdateGeometry, which in turn will analyze neighboring objects in the graph.
	for (int32 dirtyPlaneID : dirtyPlaneIDs)
	{
		AModumateObjectInstance *dirtyPlane = GetObjectById(dirtyPlaneID);
		FGraph3DFace *dirtyFace = volumeGraph->FindFace(dirtyPlaneID);
		if (dirtyPlane && dirtyFace)
		{
			const TArray<int32> &childIDs = dirtyPlane->GetChildIDs();
			if (childIDs.Num() == 1)
			{
				if (AModumateObjectInstance *dirtyPlaneHostedObj = GetObjectById(childIDs[0]))
				{
					dirtyPlaneHostedObj->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}
		}
	}
}

FGraph3D* UModumateDocument::GetVolumeGraph(int32 GraphId /*= MOD_ID_NONE*/)
{
	return const_cast<FGraph3D*>(static_cast<const UModumateDocument*>(this)->GetVolumeGraph(GraphId));
}

const FGraph3D* UModumateDocument::GetVolumeGraph(int32 GraphId /*= MOD_ID_NONE*/) const
{
	const TSharedPtr<FGraph3D>* graph = VolumeGraphs.Find(GraphId == MOD_ID_NONE ? ActiveVolumeGraph : GraphId);
	return ensure(graph) ? graph->Get() : nullptr;
}

FGraph3D* UModumateDocument::FindVolumeGraph(int32 ElementID)
{
	return const_cast<FGraph3D*>(static_cast<const UModumateDocument*>(this)->FindVolumeGraph(ElementID));
}

const FGraph3D* UModumateDocument::FindVolumeGraph(int32 ElementID) const
{
	int32 graphID = FindGraph3DByObjID(ElementID);
	return graphID == MOD_ID_NONE ? nullptr : VolumeGraphs[graphID].Get();
}

void UModumateDocument::SetActiveVolumeGraphID(int32 NewID)
{
	if (NewID != ActiveVolumeGraph)
	{
		ActiveVolumeGraph = NewID;
		FGraph3D::CloneFromGraph(TempVolumeGraph, *GetVolumeGraph(NewID));
	}
}

int32 UModumateDocument::FindGraph3DByObjID(int32 MetaObjectID) const
{
	const int32 * graphID = GraphElementsToGraph3DMap.Find(FMath::Abs(MetaObjectID));
	return graphID ? *graphID : MOD_ID_NONE;
}

const TSharedPtr<FGraph2D> UModumateDocument::FindSurfaceGraph(int32 SurfaceGraphID) const
{
	return SurfaceGraphs.FindRef(SurfaceGraphID);
}

TSharedPtr<FGraph2D> UModumateDocument::FindSurfaceGraph(int32 SurfaceGraphID)
{
	return SurfaceGraphs.FindRef(SurfaceGraphID);
}

const TSharedPtr<FGraph2D> UModumateDocument::FindSurfaceGraphByObjID(int32 ObjectID) const
{
	return const_cast<UModumateDocument*>(this)->FindSurfaceGraphByObjID(ObjectID);
}

TSharedPtr<FGraph2D> UModumateDocument::FindSurfaceGraphByObjID(int32 ObjectID)
{
	auto moi = GetObjectById(ObjectID);
	if (moi == nullptr)
	{
		return nullptr;
	}
	int32 parentID = moi->GetParentID();
	TSharedPtr<FGraph2D> surfaceGraph = nullptr;

	// this handles the case where the object is a surface graph object
	surfaceGraph = FindSurfaceGraph(parentID);
	if (surfaceGraph)
	{
		return surfaceGraph;
	}

	// this handles the case where the object is a surface graph-hosted object
	auto parentMoi = GetObjectById(parentID);
	if (parentMoi)
	{
		surfaceGraph = FindSurfaceGraph(parentMoi->GetParentID());
	}

	return surfaceGraph;
}

bool UModumateDocument::IsObjectInVolumeGraph(int32 ObjID, EGraph3DObjectType &OutObjType) const
{
	bool bIsInGraph = false;

	const AModumateObjectInstance *moi = GetObjectById(ObjID);
	if (moi == nullptr)
	{
		return bIsInGraph;
	}

	OutObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(moi->GetObjectType());
	bool bIsVolumeGraphType = (OutObjType != EGraph3DObjectType::None);
	auto* graph = FindVolumeGraph(ObjID);
	bIsInGraph = graph ? graph->ContainsObject(ObjID) : false;
	ensureAlways(bIsVolumeGraphType == bIsInGraph);

	return bIsInGraph;
}

void UModumateDocument::GetSpansForGraphElement(int32 GraphElement, TArray<int32>& OutSpanIDs) const
{
	TArray<int32> spanIDs;
	GraphElementToSpanMap.MultiFind(GraphElement, spanIDs);
	OutSpanIDs.Append(MoveTemp(spanIDs));
}

void DisplayDebugMsg(const FString& Message)
{
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, Message, false);
}

void UModumateDocument::DisplayDebugInfo(UWorld* world)
{
	auto displayObjectCount = [this](EObjectType ot, const TCHAR *name)
	{
		TArray<AModumateObjectInstance*> obs = GetObjectsOfType(ot);
		if (obs.Num() > 0)
		{
			DisplayDebugMsg(FString::Printf(TEXT("%s - %d"), name, obs.Num()));
		}
	};

	AEditModelPlayerState* emPlayerState = Cast<AEditModelPlayerState>(world->GetFirstPlayerController()->PlayerState);

	DisplayDebugMsg(TEXT("OBJECT COUNTS"));
	auto objectTypeEnum = StaticEnum<EObjectType>();
	for (int32 objectTypeIdx = 0; objectTypeIdx < objectTypeEnum->NumEnums(); ++objectTypeIdx)
	{
		EObjectType objectType = static_cast<EObjectType>(objectTypeEnum->GetValueByIndex(objectTypeIdx));
		displayObjectCount(objectType, *GetEnumValueString(objectType));
	}

	TSet<FGuid> asms;

	Algo::Transform(ObjectInstanceArray, asms,
		[](const AModumateObjectInstance *ob)
		{
			return ob->GetAssembly().UniqueKey();
		}
	);

	for (auto &a : asms)
	{
		int32 instanceCount = Algo::Accumulate(
			ObjectInstanceArray, 0,
			[a](int32 total, const AModumateObjectInstance *ob)
			{
				return ob->GetAssembly().UniqueKey() == a ? total + 1 : total;
			}
		);

		DisplayDebugMsg(FString::Printf(TEXT("Assembly %s: %d"), *a.ToString(), instanceCount));
	}

	DisplayDebugMsg(FString::Printf(TEXT("Undo Buffer: %d"), UndoBuffer.Num()));
	DisplayDebugMsg(FString::Printf(TEXT("Redo Buffer: %d"), RedoBuffer.Num()));
	DisplayDebugMsg(FString::Printf(TEXT("Deleted Obs: %d"), DeletedObjects.Num()));
	DisplayDebugMsg(FString::Printf(TEXT("Active Obs: %d"), ObjectInstanceArray.Num()));
	DisplayDebugMsg(FString::Printf(TEXT("Next ID: %d"), NextID));
	DisplayDebugMsg(FString::Printf(TEXT("Current doc hash: %08x"), GetLatestVerifiedDocHash()));

	FString selected = FString::Printf(TEXT("Selected Obs: %d"), emPlayerState->SelectedObjects.Num());
	if (emPlayerState->SelectedObjects.Num() > 0)
	{
		selected += TEXT(" | ");
		for (auto &sel : emPlayerState->SelectedObjects)
		{
			selected += FString::Printf(TEXT("SEL: %d #CHILD: %d PARENTID: %d"), sel->ID, sel->GetChildIDs().Num(),sel->GetParentID());
		}
	}
	DisplayDebugMsg(selected);
}

void UModumateDocument::DrawDebugVolumeGraph(UWorld* world)
{
	const float drawVerticalOffset = 5.0f;
	const float pointThickness = 8.0f;
	const float lineThickness = 2.0f;
	const float arrowSize = 10.0f;
	const float faceEdgeOffset = 15.0f;
	const FVector textOffset = 20.0f * FVector::UpVector;

	const FGraph3D* volumeGraph = GetVolumeGraph();
	for (const auto &kvp : volumeGraph->GetVertices())
	{
		const FGraph3DVertex &graphVertex = kvp.Value;
		FVector vertexDrawPos = graphVertex.Position;

		world->LineBatcher->DrawPoint(vertexDrawPos, FLinearColor::Red, pointThickness, 0);
		FString vertexString = FString::Printf(TEXT("Vertex #%d: [%s]"), graphVertex.ID,
			*FString::JoinBy(graphVertex.ConnectedEdgeIDs, TEXT(", "), [](const FGraphSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); })
		);

		DrawDebugString(world, vertexDrawPos + textOffset, vertexString, nullptr, FColor::White, 0.0f, true);
	}

	for (const auto &kvp : volumeGraph->GetEdges())
	{
		const FGraph3DEdge &graphEdge = kvp.Value;
		const FGraph3DVertex *startGraphVertex = volumeGraph->FindVertex(graphEdge.StartVertexID);
		const FGraph3DVertex *endGraphVertex = volumeGraph->FindVertex(graphEdge.EndVertexID);
		if (startGraphVertex && endGraphVertex)
		{
			FVector startDrawPos = startGraphVertex->Position;
			FVector endDrawPos = endGraphVertex->Position;

			DrawDebugDirectionalArrow(world, startDrawPos, endDrawPos, arrowSize, FColor::Blue, false, -1.f, 0xFF, lineThickness);
			FString edgeString = FString::Printf(TEXT("Edge #%d: [%d, %d]"), graphEdge.ID, graphEdge.StartVertexID, graphEdge.EndVertexID);
			DrawDebugString(world, 0.5f * (startDrawPos + endDrawPos) + textOffset, edgeString, nullptr, FColor::White, 0.0f, true);
		}
	}

	for (const auto &kvp : volumeGraph->GetFaces())
	{
		const FGraph3DFace &face = kvp.Value;

		int32 numEdges = face.EdgeIDs.Num();
		for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
		{
			FGraphSignedID edgeID = face.EdgeIDs[edgeIdx];
			const FVector &edgeNormal = face.CachedEdgeNormals[edgeIdx];
			const FVector &prevEdgeNormal = face.CachedEdgeNormals[(edgeIdx + numEdges - 1) % numEdges];
			const FVector &nextEdgeNormal = face.CachedEdgeNormals[(edgeIdx + 1) % numEdges];

			const FGraph3DEdge *graphEdge = volumeGraph->FindEdge(edgeID);
			const FGraph3DVertex *startGraphVertex = volumeGraph->FindVertex(graphEdge->StartVertexID);
			const FGraph3DVertex *endGraphVertex = volumeGraph->FindVertex(graphEdge->EndVertexID);

			bool bEdgeForward = (edgeID > 0);
			FVector startDrawPos = bEdgeForward ? startGraphVertex->Position : endGraphVertex->Position;
			FVector endDrawPos = bEdgeForward ? endGraphVertex->Position : startGraphVertex->Position;
			startDrawPos += faceEdgeOffset * (edgeNormal + prevEdgeNormal).GetSafeNormal();
			endDrawPos += faceEdgeOffset * (edgeNormal + nextEdgeNormal).GetSafeNormal();

			FVector edgeNStartPos = (startDrawPos + endDrawPos) / 2.0f;
			FVector edgeNEndPos = edgeNStartPos + edgeNormal * faceEdgeOffset;

			DrawDebugDirectionalArrow(world, startDrawPos, endDrawPos, arrowSize, FColor::Green, false, -1.f, 0, lineThickness);
			DrawDebugDirectionalArrow(world, edgeNStartPos, edgeNEndPos, arrowSize, FColor::Blue, false, -1.f, 0, lineThickness);
		}

		FString faceString = FString::Printf(TEXT("Face #%d: [%s]%s%s"), face.ID,
			*FString::JoinBy(face.EdgeIDs, TEXT(", "), [](const FGraphSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); }),
			(face.ContainedFaceIDs.Num() == 0) ? TEXT("") : *FString::Printf(TEXT(" Contains{%s}"), *FString::JoinBy(face.ContainedFaceIDs, TEXT(", "), [](const int32& ContainedFaceID) { return FString::Printf(TEXT("%d"), ContainedFaceID); })),
			(face.ContainingFaceID == MOD_ID_NONE) ? TEXT("") : *FString::Printf(TEXT(" ContainedBy #%d"), face.ContainingFaceID)
		);
		DrawDebugString(world, face.CachedCenter, faceString, nullptr, FColor::White, 0.0f, true);

		FVector planeNormal = face.CachedPlane;
		DrawDebugDirectionalArrow(world, face.CachedCenter, face.CachedCenter + 2.0f * faceEdgeOffset * planeNormal, arrowSize, FColor::Green, false, -1.f, 0xFF, lineThickness);
	}

	for (const auto &kvp : volumeGraph->GetPolyhedra())
	{
		const FGraph3DPolyhedron &polyhedron = kvp.Value;
		FString polyhedronFacesString = FString::JoinBy(polyhedron.FaceIDs, TEXT(", "), [](const FGraphSignedID &faceID) { return FString::Printf(TEXT("%d"), faceID); });
		FString polyhedronTypeString = !polyhedron.bClosed ? FString(TEXT("open")) :
			FString::Printf(TEXT("closed, %s, %s"),
				polyhedron.bInterior ? TEXT("interior") : TEXT("exterior"),
				polyhedron.bConvex ? TEXT("convex") : TEXT("concave")
			);
		FString polyhedronString = FString::Printf(TEXT("Polyhedron: #%d: [%s], %s"),
			polyhedron.ID, *polyhedronFacesString, *polyhedronTypeString
		);
		GEngine->AddOnScreenDebugMessage(polyhedron.ID, 0.0f, FColor::White, polyhedronString, false);
	}
}

void UModumateDocument::DrawDebugSurfaceGraphs(UWorld* world)
{
	const float drawVerticalOffset = 5.0f;
	const float pointThickness = 8.0f;
	const float lineThickness = 2.0f;
	const float arrowSize = 10.0f;
	const float faceEdgeOffset = 15.0f;
	const FVector textOffset = 10.0f * FVector::UpVector;

	for (auto& kvp : SurfaceGraphs)
	{
		auto graph = kvp.Value;

		int32 surfaceGraphID = kvp.Key;
		const AModumateObjectInstance *surfaceGraphObj = GetObjectById(surfaceGraphID);
		const AModumateObjectInstance *surfaceGraphParent = surfaceGraphObj ? surfaceGraphObj->GetParentObject() : nullptr;
		const bool bIsTerrain = surfaceGraphObj && surfaceGraphObj->GetObjectType() == EObjectType::OTTerrain;

		int32 surfaceGraphFaceIndex = UModumateObjectStatics::GetParentFaceIndex(surfaceGraphObj);

		if (!ensure(bIsTerrain ||
			(surfaceGraphObj && surfaceGraphParent && (surfaceGraphFaceIndex != INDEX_NONE)) ))
		{
			continue;
		}

		const AMOISurfaceGraph* surfaceGraphImpl = static_cast<const AMOISurfaceGraph*>(surfaceGraphObj);
		FString surfaceGraphString = FString::Printf(TEXT("SurfaceGraph: #%d, face %d, %s"),
			surfaceGraphID, surfaceGraphFaceIndex,
			bIsTerrain ? TEXT("") : (surfaceGraphImpl->IsGraphLinked() ? TEXT("linked") : TEXT("unlinked")) );
		GEngine->AddOnScreenDebugMessage(surfaceGraphID, 0.0f, FColor::White, surfaceGraphString);

		TArray<FVector> facePoints;
		FVector faceNormal, faceAxisX, faceAxisY;
		if (bIsTerrain)
		{   // Terrain has a simple basis:
			faceNormal = FVector::UpVector;
			faceAxisX = FVector::ForwardVector;
			faceAxisY = FVector::RightVector;
			facePoints.Add(surfaceGraphObj->GetLocation());
		}
		else
		{
			if (!ensure(UModumateObjectStatics::GetGeometryFromFaceIndex(surfaceGraphParent, surfaceGraphFaceIndex, facePoints, faceNormal, faceAxisX, faceAxisY)))
			{
				continue;
			}
		}
		FVector faceOrigin = facePoints[0];

		for (auto& vertexkvp : graph->GetVertices())
		{
			const FGraph2DVertex &graphVertex = vertexkvp.Value;
			FVector2D vertexPos = graphVertex.Position;
			FVector vertexDrawPos = UModumateGeometryStatics::Deproject2DPoint(vertexPos, faceAxisX, faceAxisY, faceOrigin);

			world->LineBatcher->DrawPoint(vertexDrawPos, FLinearColor::Red, pointThickness, 0);
			FString vertexString = FString::Printf(TEXT("Vertex #%d: [%s]"), graphVertex.ID,
				*FString::JoinBy(graphVertex.Edges, TEXT(", "), [](const FGraphSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); })
			);

			DrawDebugString(world, vertexDrawPos + textOffset, vertexString, nullptr, FColor::White, 0.0f, true);
		}

		for (const auto &edgekvp : graph->GetEdges())
		{
			const FGraph2DEdge &graphEdge = edgekvp.Value;
			const FGraph2DVertex *startGraphVertex = graph->FindVertex(graphEdge.StartVertexID);
			const FGraph2DVertex *endGraphVertex = graph->FindVertex(graphEdge.EndVertexID);
			if (startGraphVertex && endGraphVertex)
			{
				FVector2D startPos = startGraphVertex->Position;
				FVector2D endPos = endGraphVertex->Position;
				FVector startDrawPos = UModumateGeometryStatics::Deproject2DPoint(startPos, faceAxisX, faceAxisY, faceOrigin);
				FVector endDrawPos = UModumateGeometryStatics::Deproject2DPoint(endPos, faceAxisX, faceAxisY, faceOrigin);

				DrawDebugDirectionalArrow(world, startDrawPos, endDrawPos, arrowSize, FColor::Blue, false, -1.f, 0xFF, lineThickness);
				FString edgeString = FString::Printf(TEXT("Edge #%d: [%d, %d], {%d, %d}"), graphEdge.ID, graphEdge.StartVertexID, graphEdge.EndVertexID, graphEdge.LeftPolyID, graphEdge.RightPolyID);
				DrawDebugString(world, 0.5f * (startDrawPos + endDrawPos) + textOffset, edgeString, nullptr, FColor::White, 0.0f, true);
			}
		}

		for (const auto &polykvp : graph->GetPolygons())
		{
			const FGraph2DPolygon &poly = polykvp.Value;

			int32 numEdges = poly.Edges.Num();

			TArray<FVector2D> edgeNormals;
			for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
			{
				FGraphSignedID edgeID = poly.Edges[edgeIdx];
				const FGraph2DEdge *graphEdge = graph->FindEdge(edgeID);
				const FGraph2DVertex *startGraphVertex = graph->FindVertex(graphEdge->StartVertexID);
				const FGraph2DVertex *endGraphVertex = graph->FindVertex(graphEdge->EndVertexID);

				FVector2D edgeNormal = FVector2D(graphEdge->CachedEdgeDir.Y, -graphEdge->CachedEdgeDir.X);
				edgeNormal *= edgeID < 0 ? -1 : 1;
				edgeNormals.Add(edgeNormal);
			}

			for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
			{
				FGraphSignedID edgeID = poly.Edges[edgeIdx];

				const FVector2D &edgeNormal = edgeNormals[edgeIdx];
				const FVector2D &prevEdgeNormal = edgeNormals[(edgeIdx + numEdges - 1) % numEdges];
				const FVector2D &nextEdgeNormal = edgeNormals[(edgeIdx + 1) % numEdges];

				const FGraph2DEdge *graphEdge = graph->FindEdge(edgeID);
				const FGraph2DVertex *startGraphVertex = graph->FindVertex(graphEdge->StartVertexID);
				const FGraph2DVertex *endGraphVertex = graph->FindVertex(graphEdge->EndVertexID);

				bool bEdgeForward = (edgeID > 0);
				FVector2D startPos = bEdgeForward ? startGraphVertex->Position : endGraphVertex->Position;
				FVector2D endPos = bEdgeForward ? endGraphVertex->Position : startGraphVertex->Position;
				startPos += faceEdgeOffset * (edgeNormal + prevEdgeNormal).GetSafeNormal();
				endPos += faceEdgeOffset * (edgeNormal + nextEdgeNormal).GetSafeNormal();

				FVector startDrawPos = UModumateGeometryStatics::Deproject2DPoint(startPos, faceAxisX, faceAxisY, faceOrigin);
				FVector endDrawPos = UModumateGeometryStatics::Deproject2DPoint(endPos, faceAxisX, faceAxisY, faceOrigin);

				FVector edgeNStartPos = UModumateGeometryStatics::Deproject2DPoint(
					(startPos + endPos) / 2.0f, faceAxisX, faceAxisY, faceOrigin);
				FVector edgeNEndPos = UModumateGeometryStatics::Deproject2DPoint(
					(startPos + endPos) / 2.0f + edgeNormal * faceEdgeOffset, faceAxisX, faceAxisY, faceOrigin);

				DrawDebugDirectionalArrow(world, startDrawPos, endDrawPos, arrowSize, FColor::Green, false, -1.f, 0, lineThickness);
				DrawDebugDirectionalArrow(world, edgeNStartPos, edgeNEndPos, arrowSize, FColor::Blue, false, -1.f, 0, lineThickness);
			}

			FString faceString = FString::Printf(TEXT("Face #%d: [%s] (%d)"), poly.ID,
				*FString::JoinBy(poly.Edges, TEXT(", "), [](const FGraphSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); }),
				poly.ContainingPolyID);

			FVector2D originPos = Algo::Accumulate(poly.CachedPoints, FVector2D::ZeroVector) / poly.CachedPoints.Num();
			FVector originDrawPos = UModumateGeometryStatics::Deproject2DPoint(originPos, faceAxisX, faceAxisY, faceOrigin);
			DrawDebugString(world, originDrawPos, faceString, nullptr, FColor::White, 0.0f, true);
		}
	}
}

void UModumateDocument::DrawDebugSpan(UWorld* world)
{
	for (const auto curObj : GetObjectsOfType(EObjectType::OTMetaPlaneSpan))
	{
		const AMOIMetaPlaneSpan* curSpan = Cast<AMOIMetaPlaneSpan>(curObj);
		if (curSpan->InstanceData.GraphMembers.Num() > 0)
		{
			FVector drawPos = curSpan->GetLocation() + FVector::UpVector * 100.f;//offset upward to avoid overlap with debug volume
			FString drawString = FString::Printf(TEXT("Span #%d: Graph:[%s]"),
				curSpan->ID,
				*FString::JoinBy(curSpan->InstanceData.GraphMembers, TEXT(", "), [](const int32& memberID) { return FString::Printf(TEXT("%d"), memberID); }));
			DrawDebugString(world, drawPos, drawString, nullptr, FColor::Purple, 0.0f, true);
		}
	}

	for (const auto curObj : GetObjectsOfType(EObjectType::OTMetaEdgeSpan))
	{
		const AMOIMetaEdgeSpan* curSpan = Cast<AMOIMetaEdgeSpan>(curObj);
		if (curSpan->InstanceData.GraphMembers.Num() > 0)
		{
			FVector drawPos = curSpan->GetLocation() + FVector::UpVector * 100.f;//offset upward to avoid overlap with debug volume
			FString drawString = FString::Printf(TEXT("Span #%d: Graph:[%s]"),
				curSpan->ID,
				*FString::JoinBy(curSpan->InstanceData.GraphMembers, TEXT(", "), [](const int32& memberID) { return FString::Printf(TEXT("%d"), memberID); }));
			DrawDebugString(world, drawPos, drawString, nullptr, FColor::Cyan, 0.0f, true);
		}
	}
}

void UModumateDocument::DisplayDesignOptionDebugInfo(UWorld* World)
{
	DisplayDebugMsg(TEXT("Design Option Debug"));
	DisplayDebugMsg(TEXT("--Options--"));
	TArray<AModumateObjectInstance*> obs = GetObjectsOfType(EObjectType::OTDesignOption);
	for (auto& ob : obs)
	{
		AMOIDesignOption* option = Cast<AMOIDesignOption>(ob);
		FString msg = FString::Printf(TEXT("ID: %d, Name: %s, Parent: %d, Groups:"), option->ID, *option->StateData.DisplayName, option->GetParentID());
		for (auto& group : option->InstanceData.groups)
		{
			msg += FString::Printf(TEXT(" %d"), group);
		}
		msg += TEXT(" SubOptions: ");
		for (auto& subop : option->InstanceData.subOptions)
		{
			msg += FString::Printf(TEXT("%d "), subop);
		}
		msg += TEXT(" Children: ");
		for (auto& child : option->GetChildIDs())
		{
			msg += FString::Printf(TEXT("%d "), child);
		}

		DisplayDebugMsg(msg);
	}
	DisplayDebugMsg(TEXT("--Groups--"));

	obs = GetObjectsOfType(EObjectType::OTMetaGraph);
	for (auto& ob : obs)
	{
		FString msg = FString::Printf(TEXT("ID: %d"),ob->ID);
		DisplayDebugMsg(msg);
	}
}

void UModumateDocument::DisplayMultiplayerDebugInfo(UWorld* world)
{
	if (!world->IsNetMode(NM_Client))
	{
		return;
	}

	for (auto& unverifiedDeltasRecord : UnverifiedDeltasRecords)
	{
		DisplayDebugMsg(FString::Printf(TEXT("Unverified delta (prev hash: %08x, total hash: %08x"), unverifiedDeltasRecord.PrevDocHash, unverifiedDeltasRecord.TotalHash));
	}
	if (VerifiedDeltasRecords.Num() > 0)
	{
		auto& lastVerifiedDeltasRecord = VerifiedDeltasRecords.Last();
		DisplayDebugMsg(FString::Printf(TEXT("Last verified delta source: %s"), *lastVerifiedDeltasRecord.OriginUserID));
	}
	DisplayDebugMsg(FString::Printf(TEXT("Last verified doc hash: %08x"), GetLatestVerifiedDocHash()));
}

const FBIMPresetCollection& UModumateDocument::GetPresetCollection() const
{
	return BIMPresetCollection;
}

FBIMPresetCollection& UModumateDocument::GetPresetCollection()
{
	return BIMPresetCollection;
}

bool UModumateDocument::MakeNewGUIDForPreset(FBIMPresetInstance& Preset)
{
	return BIMPresetCollection.GetAvailableGUID(Preset.GUID) == EBIMResult::Success;
}

bool UModumateDocument::DuplicatePreset(UWorld* World, const FGuid& OriginalPreset, FBIMPresetInstance& OutPreset)
{
	TSharedPtr<FBIMPresetDelta> delta = BIMPresetCollection.MakeDuplicateDelta(OriginalPreset, OutPreset);
	if (delta.IsValid() && ApplyDeltas({ delta },World))
	{
		OutPreset = delta->NewState;
		return true;
	}
	return false;
}

bool UModumateDocument::PresetIsInUse(const FGuid& InPreset) const
{
	const FBIMPresetInstance* preset = BIMPresetCollection.PresetFromGUID(InPreset);
	if (preset == nullptr)
	{
		return false;
	}

	// MetaGraph Presets (ie, Symbols) have uses handled separately.
	if (preset->ObjectType != EObjectType::OTNone && preset->ObjectType != EObjectType::OTMetaGraph)
	{
		TArray<const AModumateObjectInstance*> obs = GetObjectsOfType(preset->ObjectType);
		for (auto ob : obs)
		{
			if (ob->GetAssembly().PresetGUID == InPreset)
			{
				return true;
			}
		}
	}

	TSet<FGuid> ancestors;
	if (BIMPresetCollection.GetAllAncestorPresets(InPreset, ancestors) == EBIMResult::Success)
	{
		return ancestors.Num() > 0;
	}

	return false;
}

void UModumateDocument::DeletePreset(UWorld* World, const FGuid& DeleteGUID, const FGuid& ReplacementGUID)
{
	if (!ensureAlways(ReplacementGUID != DeleteGUID))
	{
		return;
	}

	TArray<FDeltaPtr> deltas;
	const FBIMPresetInstance* preset = BIMPresetCollection.PresetFromGUID(DeleteGUID);

	if (preset == nullptr)
	{
		return;
	}
	
	ClearPreviewDeltas(World);

	if (preset->NodeScope != EBIMValueScope::Symbol)
	{
		TArray<AModumateObjectInstance*> obs = GetObjectsOfType(preset->ObjectType).FilterByPredicate(
			[DeleteGUID](const AModumateObjectInstance* MOI) {return MOI->GetAssembly().PresetGUID == DeleteGUID; });

		if (obs.Num() > 0)
		{
			if (ReplacementGUID.IsValid())
			{
				auto delta = MakeShared<FMOIDelta>();
				for (auto ob : obs)
				{
					auto& newState = delta->AddMutationState(ob);
					newState.AssemblyGUID = ReplacementGUID;
				}
				deltas.Add(delta);
			}
			else if (!GetDeleteObjectsDeltas(deltas, obs))
			{
				return;
			}
		}
	}

	if (BIMPresetCollection.MakeDeleteDeltas(DeleteGUID, ReplacementGUID, deltas) == EBIMResult::Success)
	{
		// If we're replacing a top level assembly, add an assembly delta for all affected MOIs
		ApplyDeltas(deltas, World);
	}
}

int32 UModumateDocument::MPObjIDFromLocalObjID(int32 LocalObjID, int32 UserIdx)
{
	if (!ensure((UserIdx != INDEX_NONE) && (LocalObjID >= 0)))
	{
		return MOD_ID_NONE;
	}

	return LocalObjID | (UserIdx << LocalObjIDBits);
}

void UModumateDocument::SplitMPObjID(int32 MPObjID, int32& OutLocalObjID, int32& OutUserIdx)
{
	// Object IDs should all be non-negative, since we're not touching the sign bit when storing the user index.
	// TODO: this sign check should be unnecessary because object IDs should be unsigned, but this is effectively
	// guaranteeing that the sign bit is reserved for other use cases, such as graph directionality.
	ensure(MPObjID >= 0);
	OutLocalObjID = MPObjID & LocalObjIDMask;
	OutUserIdx = (MPObjID & ObjIDUserMask) >> LocalObjIDBits;
	ensure((OutLocalObjID >= 0) && (OutUserIdx >= 0) && (OutUserIdx <= MaxUserIdx));
}

void UModumateDocument::UpdateWindowTitle()
{
#if !UE_SERVER
	FText projectSuffix = bUserFileDirty ? LOCTEXT("DirtyProjectSuffix", "*") : FText::GetEmpty();
	UModumateFunctionLibrary::SetWindowTitle(CurrentProjectName, projectSuffix);
#endif
}

void UModumateDocument::RecordSavedProject(UWorld* World, const FString& FilePath, bool bUserFile)
{
	if (bUserFile)
	{
		bUserFileDirty = false;

		SetCurrentProjectName(FilePath);

		if (auto* gameInstance = World->GetGameInstance<UModumateGameInstance>())
		{
			gameInstance->UserSettings.RecordRecentProject(FilePath, true);
		}
	}
	else
	{
		bAutoSaveDirty = false;
	}
}

void UModumateDocument::DrawingPushDD()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::drawing_request_document"));

	FString documentJson;

	if (ensureAlways(DrawingDesignerDocument.WriteJson(documentJson)))
	{
		DrawingSendResponse(TEXT("onDocumentChanged"), documentJson);
	}
	DrawingDesignerDocument.bDirty = false;
	// TODO: else send error status
}

void UModumateDocument::DrawingSendResponse(const FString& FunctionName, const FString& Argument) const
{
	auto player = GetWorld()->GetFirstLocalPlayerFromController();
	auto controller = player ? Cast<AEditModelPlayerController>(player->GetPlayerController(GetWorld())) : nullptr;
	auto drawingDesigner = controller && controller->EditModelUserWidget ? controller->EditModelUserWidget->DrawingDesigner : nullptr;

	if (drawingDesigner && drawingDesigner->DrawingSetWebBrowser)
	{
		FString javaScript = TEXT("ue_event.") + FunctionName + TEXT("(") + Argument + TEXT(")");
		drawingDesigner->DrawingSetWebBrowser->ExecuteJavascript(javaScript);
	}
}

void UModumateDocument::UpdateWebPresets()
{
	FBIMWebPresetCollection collection;
	BIMPresetCollection.GetWebPresets(collection, GetWorld());
	FString jsonPresets;
	if (WriteJsonGeneric(jsonPresets, &collection))
	{
		DrawingSendResponse(TEXT("onPresetsChanged"), jsonPresets);
	}
}

void UModumateDocument::TriggerWebPresetChange(EWebPresetChangeVerb Verb, TArray<FGuid> Presets)
{

	FWebPresetChangePackage package;
	package.verb = Verb;

	for (auto& preset : Presets)
	{
		FBIMWebPreset& webPreset = package.presets.Add(preset);

		// if we are not deleting the preset, fill out the information
		if (Verb != EWebPresetChangeVerb::Remove)
		{
			auto presetInstance = BIMPresetCollection.PresetFromGUID(preset);
			if (presetInstance != nullptr) {
				presetInstance->ToWebPreset(webPreset, GetWorld());
			}
		}
	}
	
	FString jsonPresets;
	if (WriteJsonGeneric(jsonPresets, &package))
	{
		DrawingSendResponse(TEXT("onPresetsUpdate"), jsonPresets);
	}
}

void UModumateDocument::ForceShiftReleaseOnWeb() const
{
	DrawingSendResponse(TEXT("onForceShiftRelease"), TEXT(""));
}

void UModumateDocument::UpdateWebMOIs(const EObjectType ObjectType) const
{
	if (!NetworkClonedObjectTypes.Contains(ObjectType))
	{
		return;
	}

	FString json;
	FWebMOIPackage webMOIPackage;
	TArray<FWebMOI> webMOIs;
	
	TArray<const AModumateObjectInstance*> objects = GetObjectsOfType(ObjectType);
	UModumateObjectStatics::GetWebMOIArrayForObjects(objects, webMOIs);

	webMOIPackage.objectType = ObjectType;
	webMOIPackage.mois = webMOIs;

	if (WriteJsonGeneric(json, &webMOIPackage))
	{
		DrawingSendResponse(TEXT("onMOIsChanged"), json);
	}
}

FString UModumateDocument::GetNextMoiName(EObjectType ObjectType, FString Name) {
	int32 n = 0;

	auto existingObjects = GetObjectsOfType(ObjectType);
	TSet<FString> existingNames;
	for (const auto* object: existingObjects)
	{
		FMOIStateData stateData = object->GetStateData();
		
		if (ObjectType == EObjectType::OTCameraView)
		{
			FMOICameraViewData cameraViewData;
			stateData.CustomData.LoadStructData(cameraViewData);
			existingNames.Add(cameraViewData.Name);
		}
		else
		{
			existingNames.Add(stateData.DisplayName);
		}
	}

	FString candidate;
	do
	{
		candidate = Name + FString::Printf(TEXT("%d"), n++);
	} while (existingNames.Contains(candidate));

	return candidate;
}

void UModumateDocument::UpdateWebProjectSettings() 
{
	auto player = GetWorld()->GetFirstLocalPlayerFromController();
	auto controller = player ? Cast<AEditModelPlayerController>(player->GetPlayerController(GetWorld())) : nullptr;
	auto drawingDesigner = controller && controller->EditModelUserWidget ? controller->EditModelUserWidget->DrawingDesigner : nullptr;
	if (!drawingDesigner)
	{
		return;
	}

	FWebProjectSettings webProjectSettings;
	// From document settings
	CurrentSettings.ToWebProjectSettings(webProjectSettings);
	// From voice settings
	if (controller->VoiceClient)
	{
		controller->VoiceClient->ToWebProjectSettings(webProjectSettings);
	}
	// From graphics settings
	UModumateGameInstance* gameInstance = GetWorld()->GetGameInstance<UModumateGameInstance>();
	if (gameInstance)
	{
		gameInstance->UserSettings.GraphicsSettings.ToWebProjectSettings(webProjectSettings);
	}
	webProjectSettings.version.value = UModumateFunctionLibrary::GetProjectVersion();

	FString projectSettingsJson;
	if (WriteJsonGeneric<FWebProjectSettings>(projectSettingsJson, &webProjectSettings))
	{
		DrawingSendResponse(TEXT("onProjectSettingsChanged"), projectSettingsJson);
	}
}

bool UModumateDocument::TryMakeUniquePresetDisplayName(const FBIMPresetCollection& PresetCollection, const FEdgeDetailData& NewDetailData, FText& OutDisplayName)
{
	TArray<FGuid> sameNameDetailPresetIDs;
	int32 displayNameIndex = 1;
	static const int32 maxDisplayNameIndex = 20;
	bool bFoundUnique = false;
	while (!bFoundUnique && displayNameIndex <= maxDisplayNameIndex)
	{
		OutDisplayName = NewDetailData.MakeShortDisplayText(displayNameIndex++);
		sameNameDetailPresetIDs.Reset();
		EBIMResult searchResult = PresetCollection.GetPresetsByPredicate([OutDisplayName](const FBIMPresetInstance& Preset) {
			return Preset.DisplayName.IdenticalTo(OutDisplayName, ETextIdenticalModeFlags::DeepCompare | ETextIdenticalModeFlags::LexicalCompareInvariants); },
			sameNameDetailPresetIDs);
		bFoundUnique = (searchResult != EBIMResult::Error) && (sameNameDetailPresetIDs.Num() == 0);
	}
	if (!bFoundUnique)
	{
		OutDisplayName = NewDetailData.MakeShortDisplayText();
	}

	return bFoundUnique;
}

void UModumateDocument::OnCameraViewSelected(int32 ID)
{
	TArray<AMOIDesignOption*> designOptions;
	Algo::Transform(GetObjectsOfType(EObjectType::OTDesignOption), designOptions,

		[](AModumateObjectInstance* MOI)
		{
			return Cast<AMOIDesignOption>(MOI);
		}
	);

	AMOICameraView* cameraView = Cast<AMOICameraView>(GetObjectById(ID));
	if (cameraView == nullptr)
	{
		return;
	}

	for (auto* designOption : designOptions)
	{
		designOption->InstanceData.isShowing = cameraView->InstanceData.SavedVisibleDesignOptions.Contains(designOption->ID);
	}

	CachedCameraViewID = ID;
	UModumateObjectStatics::UpdateDesignOptionVisibility(this);

	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	AEditModelPlayerState* playerState = controller ? controller->EMPlayerState : nullptr;

	if (playerState)
	{
		TArray<int32> unselected;
		TArray<int32> selected;
		Algo::TransformIf(cameraView->InstanceData.SavedCutPlaneVisibilities, selected, [](const TPair<int32, bool>& a) { return a.Value; },
			[](const TPair<int32, bool>& a) {return a.Key; });
		Algo::TransformIf(cameraView->InstanceData.SavedCutPlaneVisibilities, unselected, [](const TPair<int32, bool>& a) { return !a.Value; },
			[](const TPair<int32, bool>& a) {return a.Key; });
		playerState->AddHideObjectsById(unselected, false);
		playerState->UnhideObjectsById(selected, false);

		controller->SetCurrentCullingCutPlane(cameraView->InstanceData.SavedCullingCutPlane, false);

		playerState->SendWebPlayerState();
	}

	UpdateWebMOIs(EObjectType::OTDesignOption); // TODO: visibility of design options should be moved to the player state
	UpdateWebMOIs(EObjectType::OTCutPlane);
}

void UModumateDocument::BeginDestroy()
{
	Super::BeginDestroy();
	// Destruct here to prevent crash on program shutdown.
	DrawingDesignerRenderControl.Reset(nullptr);
}

void UModumateDocument::NotifyWeb(ENotificationLevel lvl, const FString& text)
{
	FString level = GetEnumValueShortName<ENotificationLevel>(lvl).ToString();
	DrawingSendResponse(TEXT("notify"), TEXT("\"" + level + "\",\"" + text + "\""));
}

void UModumateDocument::OpenWebMarketplace(FBIMTagPath npc)
{
	FString tagpath;
	npc.ToString(tagpath);
	DrawingSendResponse(TEXT("openMarketplace"), TEXT("\"" + tagpath + "\""));
}

#undef LOCTEXT_NAMESPACE
