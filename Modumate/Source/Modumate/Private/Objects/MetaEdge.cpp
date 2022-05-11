// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaEdge.h"

#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/EdgeDetailObj.h"
#include "Objects/MiterNode.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UI/EditModelUserWidget.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UI/Properties/InstPropWidgetCycle.h"

FMOIMetaEdgeData::FMOIMetaEdgeData()
{}

AMOIMetaEdge::AMOIMetaEdge()
	: AMOIEdgeBase()
	, CachedEdgeDetailMOI(nullptr)
	, CachedEdgeDetailData(FEdgeDetailData::CurrentVersion)
	, CachedEdgeDetailDataID()
	, CachedEdgeDetailConditionHash(0)
	, BaseDefaultColor(FColor::Black)
	, BaseGroupedColor(FColor(0x0D, 0x0B, 0x55))
	, HoverDefaultColor(FColor::Black)
	, HoverGroupedColor(FColor(0x0D, 0x0B, 0x55))
{
	FWebMOIProperty prop;

	prop.Name = TEXT("FlipDirection");
	prop.Type = EWebMOIPropertyType::flipDirection;
	prop.DisplayName = TEXT("Flip Direction");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("CalculatedLength");
	prop.Type = EWebMOIPropertyType::label;
	prop.DisplayName = TEXT("Length");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("CachedEdgeDetail");
	prop.Type = EWebMOIPropertyType::edgeDetail;
	prop.DisplayName = TEXT("Detail");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("CachedMiterHash");
	prop.Type = EWebMOIPropertyType::edgeDetailHash;
	prop.isEditable = false;
	prop.isVisible = false;
	WebProperties.Add(prop.Name, prop);
}

bool AMOIMetaEdge::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	UModumateDocument* doc = GetDocument();

	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		GetConnectedMOIs(CachedConnectedMOIs);

		const auto* graph = doc->FindVolumeGraph(ID);
		auto edge = graph ? graph->FindEdge(ID) : nullptr;
		auto vertexStart = edge ? graph->FindVertex(edge->StartVertexID) : nullptr;
		auto vertexEnd = edge ? graph->FindVertex(edge->EndVertexID) : nullptr;
		if (!ensure(LineActor.IsValid() && vertexStart && vertexEnd))
		{
			return false;
		}

		LineActor->Point1 = vertexStart->Position;
		LineActor->Point2 = vertexEnd->Position;
		LineActor->UpdateTransform();

		BaseColor = BaseDefaultColor;
		HoveredColor = HoverDefaultColor;

		// If our own geometry has been updated, then we need to re-evaluate our mitering,
		// and if connectivity has changed then we need to update visuals.
		MarkDirty(EObjectDirtyFlags::Visuals | EObjectDirtyFlags::Mitering);
	}
	break;
	case EObjectDirtyFlags::Mitering:
	{
		// clean the miter flag by performing the mitering for this edge's connected plane-hosted objects
		if (!CachedMiterData.GatherDetails(this))
		{
			return false;
		}

		CacheEdgeDetail();

		uint32 cachedConditions = CachedEdgeDetailConditionHash;

		// If this edge has a detail, we may need to update it if conditions have changed.
		if (CachedEdgeDetailMOI && OutSideEffectDeltas)
		{
			// If the current orientation is no longer valid, see if there is another valid one.
			TArray<int32> tempDetailOrientations;
			if (CachedEdgeDetailMOI->GetAssembly().EdgeDetailData.CompareConditions(CachedEdgeDetailData, tempDetailOrientations) && (tempDetailOrientations.Num() > 0))
			{
				if (!tempDetailOrientations.Contains(CachedEdgeDetailMOI->InstanceData.OrientationIndex))
				{
					auto updateOrientationDelta = MakeShared<FMOIDelta>();
					auto& newStateData = updateOrientationDelta->AddMutationState(CachedEdgeDetailMOI);
					FMOIEdgeDetailData newDetailInstanceData = CachedEdgeDetailMOI->InstanceData;
					newDetailInstanceData.OrientationIndex = tempDetailOrientations[0];
					if (ensure(newStateData.CustomData.SaveStructData(newDetailInstanceData)))
					{
						OutSideEffectDeltas->Add(updateOrientationDelta);
						ResetEdgeDetail();
					}
				}
			}
			// If there is no valid orientation for the saved detail, then delete the detail.
			else
			{
				auto deleteObjectDelta = MakeShared<FMOIDelta>();
				deleteObjectDelta->AddCreateDestroyState(CachedEdgeDetailMOI->GetStateData(), EMOIDeltaType::Destroy);
				OutSideEffectDeltas->Add(deleteObjectDelta);
				ResetEdgeDetail();
			}

		}

		// If we don't have an edge detail, look up the typical
		if (!CachedEdgeDetailMOI && OutSideEffectDeltas)
		{
			auto* edgeDetailPreset = doc->TypicalEdgeDetails.Find(cachedConditions);
			if (edgeDetailPreset != nullptr)
			{
				FMOIStateData newDetailState;
				newDetailState = FMOIStateData(doc->GetNextAvailableID(), EObjectType::OTEdgeDetail, ID);

				newDetailState.AssemblyGUID = *edgeDetailPreset;
				newDetailState.CustomData.SaveStructData(FMOIEdgeDetailData());
				auto updateDetailMOIDelta = MakeShared<FMOIDelta>();

				updateDetailMOIDelta->AddCreateDestroyState(newDetailState, EMOIDeltaType::Create);
				OutSideEffectDeltas->Add(updateDetailMOIDelta);
				MarkDirty(EObjectDirtyFlags::Visuals | EObjectDirtyFlags::Mitering);
			}
		}

		CachedMiterData.CalculateMitering();

		// If the miter participants aren't already miter-dirty, then mark them dirty now so that they can update their layer extensions.
		for (auto& kvp : CachedMiterData.ParticipantsByID)
		{
			AModumateObjectInstance* miterParticipantMOI = doc->GetObjectById(kvp.Key);
			if (miterParticipantMOI)
			{
				miterParticipantMOI->MarkDirty(EObjectDirtyFlags::Mitering);
			}
		}
	}
	break;
	case EObjectDirtyFlags::Visuals:
	{
		for (AModumateObjectInstance* connectedMOI : CachedConnectedMOIs)
		{
			if ((connectedMOI->GetObjectType() == EObjectType::OTMetaPlane) && connectedMOI->IsDirty(EObjectDirtyFlags::Visuals))
			{
				return false;
			}
		}

		return TryUpdateVisuals();
	}
	break;
	default:
		break;
	}

	return true;
}

void AMOIMetaEdge::ShowAdjustmentHandles(AEditModelPlayerController* Controller, bool bShow)
{
	AMOIEdgeBase::ShowAdjustmentHandles(Controller, bShow);
	UpdateLineArrowVisual();

	UModumateDocument* doc = GetDocument();
	if (!ensure(doc) || IsDestroyed())
	{
		return;
	}

	const auto& graph = *doc->FindVolumeGraph(ID);
	auto edge = graph.FindEdge(ID);
	if (!ensure(edge))
	{
		return;
	}
}

void AMOIMetaEdge::RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI)
{
	static const FString cyclePropertyName(TEXT("Flip Direction"));
	if (auto cycleField = PropertiesUI->RequestPropertyField<UInstPropWidgetCycle>(this, cyclePropertyName))
	{
		cycleField->RegisterValue(this);
		cycleField->ValueChangedEvent.AddDynamic(this, &AMOIMetaEdge::OnInstPropUIChangedCycle);
	}

	// If there are no miter participants, then the edge cannot have a detail
	// (and if it did, it should have been deleted)
	if (CachedMiterData.SortedParticipantIDs.Num() == 0)
	{
		return;
	}

	static const FString edgeDetailPropertyName(TEXT("Detail"));
	if (auto edgeDetailField = PropertiesUI->RequestPropertyField<UInstPropWidgetEdgeDetail>(this, edgeDetailPropertyName))
	{
		edgeDetailField->RegisterValue(this, ID, CachedEdgeDetailDataID, CachedEdgeDetailConditionHash);
	}
}

const FMiterData& AMOIMetaEdge::GetMiterData() const
{
	return CachedMiterData;
}

void AMOIMetaEdge::ResetEdgeDetail()
{
	CachedEdgeDetailData.Reset();
	CachedEdgeDetailDataID.Invalidate();
	CachedEdgeDetailMOI = nullptr;
	CachedEdgeDetailConditionHash = 0;
}

void AMOIMetaEdge::CacheEdgeDetail()
{
	ResetEdgeDetail();

	for (int32 childID : CachedChildIDs)
	{
		auto childDetailObj = Cast<AMOIEdgeDetail>(Document->GetObjectById(childID));
		if (childDetailObj && ensureMsgf(CachedEdgeDetailMOI == nullptr, TEXT("Massing Edge #%d has more than one child Edge Detail!"), ID))
		{
			CachedEdgeDetailMOI = childDetailObj;
		}
	}

	if (CachedEdgeDetailMOI)
	{
		CachedEdgeDetailDataID = CachedEdgeDetailMOI->GetAssembly().UniqueKey();
	}

	CachedEdgeDetailData.FillFromMiterNode(GetMiterInterface());
	CachedEdgeDetailConditionHash = CachedEdgeDetailData.CachedConditionHash;
}

void AMOIMetaEdge::OnInstPropUIChangedCycle(int32 BasisValue)
{
	Document->ReverseMetaObjects(GetWorld(), {ID}, {});
}

bool AMOIMetaEdge::FromWebMOI(const FString& InJson)
{
	if (AModumateObjectInstance::FromWebMOI(InJson))
	{
		if (InstanceData.FlipDirection)
		{
			OnInstPropUIChangedCycle(1);
		}

		return true;
	}

	return false;
}

bool AMOIMetaEdge::ToWebMOI(FWebMOI& OutMOI) const
{
	if (AModumateObjectInstance::ToWebMOI(OutMOI))
	{
		FWebMOIProperty* prop = OutMOI.Properties.Find(TEXT("FlipDirection"));
		prop->ValueArray.Empty();
		prop->ValueArray.Add("false");

		const auto& graph = *GetDocument()->FindVolumeGraph(ID);
		auto edge = graph.FindEdge(ID);
		if (edge)
		{
			AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
			EUnit defaultUnit = Document->GetCurrentSettings().DimensionUnit;
			EDimensionUnits unitType = Document->GetCurrentSettings().DimensionType;
			FText calculatedLength = UModumateDimensionStatics::CentimetersToDisplayText(CalculateLength(controller), 1, unitType, defaultUnit);

			prop = OutMOI.Properties.Find(TEXT("CalculatedLength"));
			prop->ValueArray.Empty();
			prop->ValueArray.Add(calculatedLength.ToString());
		}

		FString edgeDetailDescription;
		if (CachedEdgeDetailMOI && CachedEdgeDetailDataID.IsValid())
		{
			edgeDetailDescription = CachedEdgeDetailDataID.ToString();
		}
		static const FString CachedEdgeDetailName(TEXT("CachedEdgeDetail"));
		static const FString CachedMiterHashName(TEXT("CachedMiterHash"));

		prop = &OutMOI.Properties.FindOrAdd(CachedEdgeDetailName);
		*prop = *WebProperties.Find(CachedEdgeDetailName);
		prop->ValueArray.Add(edgeDetailDescription);

		prop = &OutMOI.Properties.FindOrAdd(CachedMiterHashName);
		*prop = *WebProperties.Find(CachedMiterHashName);
		prop->ValueArray.Add(FString::Printf(TEXT("%u"), CachedEdgeDetailConditionHash));

		return true;
	}

	return false;
}

float AMOIMetaEdge::CalculateLength(AEditModelPlayerController* AEMPlayerController) const
{
	float totalLength = 0.0f;
	for (auto& ob : AEMPlayerController->EMPlayerState->SelectedObjects)
	{
		if (ob->GetAssembly().ObjectType == EObjectType::OTMetaEdge && ob->GetNumCorners() > 1)
		{
			totalLength += (ob->GetCorner(0) - ob->GetCorner(1)).Size();
		}
	}

	return totalLength;
}

