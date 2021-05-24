#include "ToolsAndAdjustments/Common/SelectedObjectToolMixin.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
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

	for (auto obj : playerState->SelectedObjects)
	{
		OriginalSelectedObjects.Add(obj->ID);
	}

	TSet<int32> vertexIDs;
	FModumateObjectDeltaStatics::GetTransformableIDs(OriginalSelectedObjects.Array(), ControllerPtr->GetDocument(), vertexIDs);
	auto doc = ControllerPtr->GetDocument();

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

		moi->RequestCollisionDisabled(StateRequestTag, true);
	}
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
	OriginalSelectedObjects.Empty();
}

void FSelectedObjectToolMixin::ReleaseObjectsAndApplyDeltas()
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

		FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, world, false);
	}

	ReleaseSelectedObjects();

}
