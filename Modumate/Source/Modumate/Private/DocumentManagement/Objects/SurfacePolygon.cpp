// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfacePolygon.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

namespace Modumate
{
	FMOISurfacePolygonImpl::FMOISurfacePolygonImpl(FModumateObjectInstance *moi)
		: FMOIPlaneImplBase(moi)
		, MeshPointOffset(0.25f)
		, bInteriorPolygon(false)
	{

	}

	void FMOISurfacePolygonImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
	{
		if (MOI && DynamicMeshActor.IsValid())
		{
			bOutVisible = false;
			bOutCollisionEnabled = false;

			if (bInteriorPolygon)
			{
				bool bHaveChildren = (MOI->GetChildIDs().Num() > 0);
				auto controller = MOI->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
				switch (controller->EMPlayerState->GetSelectedViewMode())
				{
				case EEditViewModes::SurfaceGraphs:
					bOutVisible = true;
					bOutCollisionEnabled = true;
					break;
				case EEditViewModes::ObjectEditing:
					bOutVisible = !bHaveChildren;
					bOutCollisionEnabled = !bHaveChildren;
					break;
				default:
					break;
				}
			}

			DynamicMeshActor->SetActorHiddenInGame(!bOutVisible);
			DynamicMeshActor->SetActorEnableCollision(bOutCollisionEnabled);
		}
	}

	bool FMOISurfacePolygonImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas)
	{
		if (!ensure(MOI))
		{
			return false;
		}

		switch (DirtyFlag)
		{
		case EObjectDirtyFlags::Structure:
		{
			if (!UpdateCachedGraphData())
			{
				return false;
			}

			// Skip exterior polygons; they can't be visible anyway, so they shouldn't set up any dynamic meshes.
			if (!bInteriorPolygon)
			{
				return true;
			}

			AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
			MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

			bool bEnableCollision = !MOI->GetIsInPreviewMode();
			DynamicMeshActor->SetupMetaPlaneGeometry(CachedOffsetPoints, MaterialData, GetAlpha(), true, &CachedHoles, bEnableCollision);
		}
		case EObjectDirtyFlags::Visuals:
			MOI->UpdateVisibilityAndCollision();
			break;
		default:
			break;
		}

		return true;
	}

	bool FMOISurfacePolygonImpl::UpdateCachedGraphData()
	{
		auto doc = MOI->GetDocument();
		const FModumateObjectInstance *surfaceGraphObj = MOI ? MOI->GetParentObject() : nullptr;
		const FGraph2D* surfaceGraph = doc->FindSurfaceGraph(surfaceGraphObj ? surfaceGraphObj->ID : MOD_ID_NONE);
		const FGraph2DPolygon* surfacePolgyon = surfaceGraph ? surfaceGraph->FindPolygon(MOI->ID) : nullptr;
		if (!ensure(surfaceGraphObj && surfaceGraph && surfacePolgyon && !surfacePolgyon->bDerivedDataDirty))
		{
			return false;
		}

		CachedOrigin = surfaceGraphObj->GetObjectLocation();
		FQuat graphRot = surfaceGraphObj->GetObjectRotation();
		CachedAxisX = graphRot.GetAxisX();
		CachedAxisY = graphRot.GetAxisY();
		CachedPlane = FPlane(CachedOrigin, surfaceGraphObj->GetNormal());
		bInteriorPolygon = surfacePolgyon->bInterior;
		FVector pointOffsetDelta = MeshPointOffset * FVector(CachedPlane);

		CachedOffsetPoints.Reset();
		for (int32 perimeterVertexID : surfacePolgyon->CachedPerimeterVertexIDs)
		{
			auto perimeterVertexObj = doc->GetObjectById(perimeterVertexID);
			if (!ensure(perimeterVertexObj) || perimeterVertexObj->IsDirty(EObjectDirtyFlags::Structure))
			{
				return false;
			}

			CachedOffsetPoints.Add(perimeterVertexObj->GetObjectLocation() + pointOffsetDelta);
		}

		// TODO: remove if we can replace the use of ControlPoints as the de-facto public API between tools, handles, and between objects
		// (such as finish objects, cabinet objects, polygon adjustment handles, move tool, etc.)
		MOI->SetControlPoints(CachedOffsetPoints);

		CachedHoles.Reset();
		for (auto interiorPolyID : surfacePolgyon->InteriorPolygons)
		{
			auto interiorSurfacePoly = surfaceGraph->FindPolygon(interiorPolyID);
			if (!ensure(interiorSurfacePoly))
			{
				return false;
			}

			auto& hole = CachedHoles.AddDefaulted_GetRef();
			for (int32 interiorPerimeterVertexID : interiorSurfacePoly->CachedPerimeterVertexIDs)
			{
				auto perimeterVertexObj = doc->GetObjectById(interiorPerimeterVertexID);
				if (!ensure(perimeterVertexObj) || perimeterVertexObj->IsDirty(EObjectDirtyFlags::Structure))
				{
					return false;
				}

				hole.Points.Add(perimeterVertexObj->GetObjectLocation() + pointOffsetDelta);
			}
		}

		return true;
	}

	float FMOISurfacePolygonImpl::GetAlpha() const
	{
		return 1.0f;
	}
}
