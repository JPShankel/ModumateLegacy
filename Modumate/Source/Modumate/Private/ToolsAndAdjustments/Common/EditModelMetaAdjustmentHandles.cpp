// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelMetaAdjustmentHandles.h"

#include "AdjustmentHandleActor_CPP.h"
#include "DrawDebugHelpers.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "ModumateCommands.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateGeometryStatics.h"
#include "Graph3D.h"

namespace Modumate
{
	FAdjustMetaEdgeHandle::FAdjustMetaEdgeHandle(FModumateObjectInstance *moi)
		: FEditModelAdjustmentHandleBase(moi)
		, OriginalEdgeStart(ForceInitToZero)
		, OriginalEdgeEnd(ForceInitToZero)
		, OriginalEdgeCenter(ForceInitToZero)
		, EdgeDir(ForceInitToZero)
		, StartVertexID(MOD_ID_NONE)
		, EndVertexID(MOD_ID_NONE)
		, VolumeGraph(nullptr)
		, EdgeConstraintPlane(ForceInitToZero)
	{

	}

	bool FAdjustMetaEdgeHandle::CalculateConstraintPlaneFromVertices()
	{
		TArray<FVector> connectedVertexPositions;
		int32 vertexIDs[2] = { StartVertexID, EndVertexID };
		for (int32 vertexID : vertexIDs)
		{
			const FGraph3DVertex *vertex = VolumeGraph->FindVertex(vertexID);
			if (vertex == nullptr)
			{
				return false;
			}
			connectedVertexPositions.Add(vertex->Position);

			for (FSignedID connectedEdgeID : vertex->ConnectedEdgeIDs)
			{
				if (FMath::Abs(connectedEdgeID) == MOI->ID)
				{
					continue;
				}

				const FGraph3DEdge *connectedEdge = VolumeGraph->FindEdge(connectedEdgeID);
				if (connectedEdge == nullptr)
				{
					return false;
				}

				int32 connectedVertexID = (connectedEdgeID > 0) ?
					connectedEdge->EndVertexID : connectedEdge->StartVertexID;
				const FGraph3DVertex *connectedVertex = VolumeGraph->FindVertex(connectedVertexID);
				if (connectedVertex)
				{
					connectedVertexPositions.Add(connectedVertex->Position);
				}
			}
		}

		int32 numTotalConnectedVertices = connectedVertexPositions.Num();
		if (numTotalConnectedVertices >= 3)
		{
			if (!UModumateGeometryStatics::GetPlaneFromPoints(connectedVertexPositions, EdgeConstraintPlane))
			{
				return false;
			}
		}

		return true;
	}

	bool FAdjustMetaEdgeHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustMetaEdgeHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse() || (MOI->ControlPoints.Num() != 2))
		{
			return false;
		}

		AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		if (gameState == nullptr)
		{
			return false;
		}

		VolumeGraph = &gameState->Document.GetVolumeGraph();
		const FGraph3DEdge *graphEdge = VolumeGraph->FindEdge(MOI->ID);
		if (graphEdge == nullptr)
		{
			return false;
		}

		StartVertexID = graphEdge->StartVertexID;
		EndVertexID = graphEdge->EndVertexID;
		int32 numEdgeFaces = graphEdge->ConnectedFaces.Num();

		// TODO: do more advanced constraint analysis, based on the convexity of all neighboring
		// faces and planarity of all neighboring edge points (not all edges can be moved entirely along their plane).
		// But for now, err on the side of over-constraining so that faces remain planar.

		if (numEdgeFaces == 0)
		{
			if (!CalculateConstraintPlaneFromVertices())
			{
				return false;
			}
		}
		else if (numEdgeFaces == 1)
		{
			const FGraph3DFace *connectedFace = VolumeGraph->FindFace(graphEdge->ConnectedFaces[0].FaceID);
			if (connectedFace == nullptr)
			{
				return false;
			}

			EdgeConstraintPlane = connectedFace->CachedPlane;
		}
		else if (numEdgeFaces > 1)
		{
			// The edge is considered over-constrained.
			return false;
		}

		OriginalEdgeStart = MOI->ControlPoints[0];
		OriginalEdgeEnd = MOI->ControlPoints[1];
		OriginalEdgeCenter = 0.5f * (OriginalEdgeStart + OriginalEdgeEnd);
		EdgeDir = (OriginalEdgeEnd - OriginalEdgeStart).GetSafeNormal();

		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(OriginalEdgeCenter, FVector(EdgeConstraintPlane), EdgeDir);

		MOI->ShowAdjustmentHandles(Controller.Get(), false);

		return true;
	}

	bool FAdjustMetaEdgeHandle::OnUpdateUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustMetaEdgeHandle::OnUpdateUse"));

		FEditModelAdjustmentHandleBase::OnUpdateUse();

		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return true;
		}

		FVector hitPoint = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(Controller->EMPlayerState->SnappedCursor.WorldPosition);

		FVector dp = hitPoint - OriginalEdgeCenter;

		if (!dp.IsNearlyZero())
		{
			MOI->ControlPoints[0] = OriginalEdgeStart + dp;
			MOI->ControlPoints[1] = OriginalEdgeEnd + dp;
			MOI->UpdateGeometry();
		}

		return true;

#if 0
		if (!FMath::IsNearlyEqual(dp.Z, 0.0f, 0.01f))
		{
			FAffordanceLine affordance;
			affordance.Color = FLinearColor::Blue;
			affordance.EndPoint = hitPoint;
			affordance.StartPoint = FVector(hitPoint.X, hitPoint.Y, AnchorLoc.Z);
			affordance.Interval = 4.0f;
			Controller->EMPlayerState->AffordanceLines.Add(affordance);
		}

		if (OriginalP.Num() == CP.Num())
		{
			if (CP.Num() == 2)
			{
				int32 numTotalCPs = MOI->ControlPoints.Num();
				FVector closestPoint1, closestPoint2;
				FVector currentEdgeDirection = (MOI->ControlPoints[CP[1]] - MOI->ControlPoints[CP[0]]).GetSafeNormal();

				// Intersection test for first CP
				int32 startID0 = (CP[0] + numTotalCPs - 1) % numTotalCPs;
				int32 endID0 = CP[0];
				FVector lineDirection0 = (MOI->ControlPoints[endID0] - MOI->ControlPoints[startID0]).GetSafeNormal();
				bool b1 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, hitPoint, currentEdgeDirection, MOI->ControlPoints[endID0], lineDirection0);
				FVector newCP0 = closestPoint1;

				// Intersection test for second CP
				int32 startID1 = (CP[1] + 1) % numTotalCPs;
				int32 endID1 = CP[1];
				FVector lineDirection1 = (MOI->ControlPoints[endID1] - MOI->ControlPoints[startID1]).GetSafeNormal();
				bool b2 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, hitPoint, currentEdgeDirection, MOI->ControlPoints[endID1], lineDirection1);
				FVector newCP1 = closestPoint1;

				// Set MOI control points to new CP
				MOI->ControlPoints[CP[0]] = newCP0;
				MOI->ControlPoints[CP[1]] = newCP1;

				// Check if CPs are intersect. If it is, don't update geometry
				TArray<int32> intersected, conflicted, triangles;
				UModumateFunctionLibrary::CalculatePolygonTriangleWithError(MOI->ControlPoints, triangles, conflicted, intersected);

				// Store last good locations to fall back on if the new floor lacks proper triangulation
				if (conflicted.Num() == 0 && intersected.Num() == 0 && triangles.Num() > 2)
				{
					UpdateTargetGeometry();
					LastValidPendingCPLocations = { MOI->ControlPoints[CP[0]] , MOI->ControlPoints[CP[1]] };
				}
				else
				{
					MOI->ControlPoints[CP[0]] = LastValidPendingCPLocations[0];
					MOI->ControlPoints[CP[1]] = LastValidPendingCPLocations[1];
				}

				HandleOriginalPoint = (OriginalP[0] + OriginalP[1]) / 2.f;
				FVector curSideMidLoc = (MOI->ControlPoints[CP[0]] + MOI->ControlPoints[CP[1]]) / 2.f;
				currentEdgeDirection = (MOI->ControlPoints[CP[1]] - MOI->ControlPoints[CP[0]]).GetSafeNormal();
				bool b3 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, HandleOriginalPoint, Handle->HandleDirection, curSideMidLoc, currentEdgeDirection);
				HandleCurrentPoint = closestPoint1;

				// Dim string. between cursor pos and original handle pos. Delta only
				UModumateFunctionLibrary::AddNewDimensionString(Controller.Get(),
					HandleOriginalPoint,
					HandleCurrentPoint,
					currentEdgeDirection,
					Controller->DimensionStringGroupID_Default,
					Controller->DimensionStringUniqueID_Delta,
					0,
					MOI->GetActor(),
					EDimStringStyle::HCamera);

				// Show dim string next to the current handle
				Handle->ShowHoverFloorDimensionString(false);
			}
			else if (CP.Num() == 1)
			{
				MOI->ControlPoints[CP[0]] = OriginalP[0] + dp;

				TArray<int32> intersected, conflicted, triangles;
				UModumateFunctionLibrary::CalculatePolygonTriangleWithError(MOI->ControlPoints, triangles, conflicted, intersected);

				if (conflicted.Num() == 0 && intersected.Num() == 0)
				{
					UpdateTargetGeometry();
					LastValidPendingCPLocations = { MOI->ControlPoints[CP[0]] };
				}
				else
				{
					MOI->ControlPoints[CP[0]] = LastValidPendingCPLocations[0];
				}

				FVector offsetDir = dp.GetSafeNormal() ^ FVector(Controller->EMPlayerState->SketchPlane);

				// Dim string between cursor pos and original point handle position. Delta only
				UModumateFunctionLibrary::AddNewDimensionString(Controller.Get(),
					OriginalP[0],
					MOI->ControlPoints[CP[0]],
					offsetDir,
					Controller->DimensionStringGroupID_Default,
					Controller->DimensionStringUniqueID_Delta,
					0,
					MOI->GetActor(),
					EDimStringStyle::HCamera,
					EEnterableField::NonEditableText,
					40.f,
					EAutoEditableBox::Never);

				// Show dim string next to the current handle
				Handle->ShowHoverFloorDimensionString(false);
			}
		}
		return true;
#endif
	}

	bool FAdjustMetaEdgeHandle::OnEndUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnEndUse())
		{
			return false;
		}

		TArray<int32> vertexIDs = { StartVertexID, EndVertexID };
		TArray<FVector> vertexPositions = { MOI->ControlPoints[0], MOI->ControlPoints[1] };

		MOI->ControlPoints[0] = OriginalEdgeStart;
		MOI->ControlPoints[1] = OriginalEdgeEnd;

		Controller->ModumateCommand(
			FModumateCommand(Commands::kMoveMetaVertices)
			.Param(Parameters::kObjectIDs, vertexIDs)
			.Param(Parameters::kControlPoints, vertexPositions)
		);

		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

		MOI->ShowAdjustmentHandles(Controller.Get(), true);

		return true;
	}

	bool FAdjustMetaEdgeHandle::OnAbortUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnAbortUse())
		{
			return false;
		}

		MOI->ControlPoints[0] = OriginalEdgeStart;
		MOI->ControlPoints[1] = OriginalEdgeEnd;

		UpdateTargetGeometry();
		MOI->ShowAdjustmentHandles(Controller.Get(), true);

		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

		return true;
	}

	FVector FAdjustMetaEdgeHandle::GetAttachmentPoint()
	{
		return 0.5f * (MOI->ControlPoints[0] + MOI->ControlPoints[1]);
	}

	bool FAdjustMetaEdgeHandle::HandleInputNumber(float number)
	{
		return false;

#if 0
		// If MOI is a floor and a handle that controls 2 CPs
		if (CP.Num() == 2)
		{
			FVector fromLocation = OriginalP[0];
			FVector targetLocation = OriginalP[1];
			FVector curHandleLocation = (fromLocation + targetLocation) / 2.f;
			FVector handleDirection = UKismetMathLibrary::RotateAngleAxis(UKismetMathLibrary::GetDirectionUnitVector(targetLocation, fromLocation), 90.0f, FVector(0.0f, 0.0f, 1.0f));
			FVector pendingHandleDirection = (HandleCurrentPoint - HandleOriginalPoint).GetSafeNormal();
			float handleUserScale = (pendingHandleDirection.Equals(handleDirection, 0.01f)) ? 1.f : -1.f;
			// Calculate where the handle will be if it travels the input distance
			FVector inputHandleLocation = curHandleLocation + (handleDirection * number * handleUserScale);
			FVector currentEdgeDirection = UKismetMathLibrary::GetDirectionUnitVector(OriginalP[0], OriginalP[1]);
			FVector closestPoint1, closestPoint2;

			// Intersection test for first CP
			int32 startID0 = UModumateFunctionLibrary::LoopArrayGetPreviousIndex(CP[0], MOI->ControlPoints.Num());
			int32 endID0 = CP[0];
			FVector lineDirection0 = UKismetMathLibrary::GetDirectionUnitVector(MOI->ControlPoints[startID0], MOI->ControlPoints[endID0]);
			bool b1 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, inputHandleLocation, currentEdgeDirection, OriginalP[0], lineDirection0);
			FVector newCP0 = FVector(closestPoint1.X, closestPoint1.Y, MOI->ControlPoints[CP[0]].Z);

			// Intersection test for second CP
			int32 startID1 = UModumateFunctionLibrary::LoopArrayGetNextIndex(CP[1], MOI->ControlPoints.Num());
			int32 endID1 = CP[1];
			FVector lineDirection1 = UKismetMathLibrary::GetDirectionUnitVector(MOI->ControlPoints[startID1], MOI->ControlPoints[endID1]);
			bool b2 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, inputHandleLocation, currentEdgeDirection, OriginalP[1], lineDirection1);
			FVector newCP1 = FVector(closestPoint1.X, closestPoint1.Y, MOI->ControlPoints[CP[1]].Z);

			// Check if CPs are intersect. If it is, don't update geometry
			TArray<FVector> proxyCPs = MOI->ControlPoints;
			proxyCPs[CP[0]] = newCP0;
			proxyCPs[CP[1]] = newCP1;
			TArray<int32> intersected, conflicted, triangles;
			UModumateFunctionLibrary::CalculatePolygonTriangleWithError(proxyCPs, triangles, conflicted, intersected);
			if (conflicted.Num() == 0 && intersected.Num() == 0 && triangles.Num() > 2)
			{
				// Set MOI control points to new CP
				MOI->ControlPoints[CP[0]] = newCP0;
				MOI->ControlPoints[CP[1]] = newCP1;

				// Setup for ModumateCommand
				TArray<FVector> newPoints = MOI->ControlPoints;
				for (size_t i = 0; i < CP.Num(); ++i)
				{
					MOI->ControlPoints[CP[i]] = OriginalP[i];
				}

				Controller->ModumateCommand(
					FModumateCommand(Modumate::Commands::kSetGeometry)
					.Param("id", MOI->ID)
					.Param("points", newPoints)
				);
				Controller->EMPlayerState->SetSketchPlaneHeight(0);
				MOI->ShowAdjustmentHandles(Controller.Get(), true);
				OnEndUse();
				return true;
			}
			else
			{
				// The result triangulation will create invalid geometry
				return false;
			}
		}
		return false; // false by default if not handling numbers
#endif
	}
}
