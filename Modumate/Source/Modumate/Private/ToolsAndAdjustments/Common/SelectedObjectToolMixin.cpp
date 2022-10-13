// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/SelectedObjectToolMixin.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "Objects/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelPlayerState.h"



const FName FSelectedObjectToolMixin::StateRequestTag(TEXT("SelectedObjectTool"));

FSelectedObjectToolMixin::FSelectedObjectToolMixin(AEditModelPlayerController *InController)
	: ControllerPtr(InController)
{

}

void FSelectedObjectToolMixin::AcquireSelectedObjects()
{
	if (!ControllerPtr.IsValid())
	{
		return;
	}

	ReleaseSelectedObjects();
	CurrentRecord = FMOIDocumentRecord();

	AEditModelPlayerState *playerState = ControllerPtr->EMPlayerState;
	UModumateDocument* doc = ControllerPtr->GetDocument();

	for (auto obj : playerState->SelectedObjects)
	{
		OriginalSelectedObjects.Add(obj->ID);
	}
	for (auto obj : playerState->SelectedGroupObjects)
	{
		OriginalSelectedGroupObjects.Add(obj->ID);
		TArray<AModumateObjectInstance*> groupDescendents(obj->GetAllDescendents());
		for (auto* moi: groupDescendents)
		{
			OriginalSelectedGroupObjects.Add(moi->ID);
		}
	}

	TSet<int32> vertexIDs;
	FModumateObjectDeltaStatics::GetTransformableIDs(OriginalSelectedObjects.Array(), doc, vertexIDs);

	FModumateObjectDeltaStatics::SaveSelection(OriginalSelectedObjects.Array(), doc, &CurrentRecord);

	for (int32 id : vertexIDs)
	{
		auto obj = doc->GetObjectById(id);
		if (obj == nullptr)
		{
			continue;
		}

		OriginalTransforms.Add(id, obj->GetWorldTransform());
	}

	// TODO: move this kind of logic into (and rename) GetVertexIDs
	// We didn't find any meta objects, so just gather the whole selection list
	if (OriginalTransforms.Num() == 0)
	{
		for (auto &ob : playerState->SelectedObjects)
		{
			auto objectAndDescendents = ob->GetAllDescendents();
			objectAndDescendents.Add(ob);
			for (auto &kid : objectAndDescendents)
			{
				OriginalTransforms.Add(kid->ID, kid->GetWorldTransform());
			}
		}
	}

	for (auto &kvp : OriginalTransforms)
	{
		auto moi = doc->GetObjectById(kvp.Key);
		if (moi == nullptr)
		{
			continue;
		}
		if (!moi->IsCollisionRequestedDisabled())
		{
			moi->RequestCollisionDisabled(StateRequestTag, true);
		}
	}

	vertexIDs.Reset();
	FModumateObjectDeltaStatics::GetTransformableIDs(OriginalSelectedGroupObjects.Array(), doc, vertexIDs);
	for (int32 id : vertexIDs)
	{
		auto obj = doc->GetObjectById(id);
		if (obj == nullptr)
		{
			continue;
		}

		OriginalGroupTransforms.Add(id, obj->GetWorldTransform());
	}

	int32 nextID = doc->GetNextAvailableID();
	FModumateObjectDeltaStatics::DuplicateGroups(doc, OriginalSelectedGroupObjects, nextID, GroupCopyDeltas);
}

void FSelectedObjectToolMixin::ReleaseSelectedObjects()
{
	auto doc = ControllerPtr->GetDocument();
	if (doc == nullptr)
	{
		return;
	}

	doc->ClearPreviewDeltas(ControllerPtr->GetWorld());

	for (auto &kvp : OriginalTransforms)
	{
		auto moi = doc->GetObjectById(kvp.Key);
		if (moi == nullptr)
		{
			continue;
		}

		moi->RequestCollisionDisabled(StateRequestTag, false);
	}

	OriginalTransforms.Empty();
	OriginalGroupTransforms.Empty();
	OriginalSelectedObjects.Empty();
	OriginalSelectedGroupObjects.Empty();
	GroupCopyDeltas.Empty();
}

void FSelectedObjectToolMixin::ReleaseObjectsAndApplyDeltas(const TArray<FDeltaPtr>* AdditionalDeltas /*= nullptr*/)
{
	UModumateDocument* doc = ControllerPtr->GetDocument();
	UWorld* world = ControllerPtr->GetWorld();

	if (!bPaste)
	{
		TMap<int32, FTransform> objectInfo;
		for (auto& kvp : OriginalTransforms)
		{
			AModumateObjectInstance* targetMOI = doc->GetObjectById(kvp.Key);
			if (!ensureAlways(targetMOI))
			{
				continue;
			}

			objectInfo.Add(kvp.Key, targetMOI->GetWorldTransform());
		}

		FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, world, false, AdditionalDeltas);
	}

	ReleaseSelectedObjects();

}
