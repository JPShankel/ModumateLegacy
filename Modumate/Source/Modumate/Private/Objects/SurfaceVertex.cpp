// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/SurfaceVertex.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/SurfaceGraph.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/VertexActor.h"

AMOISurfaceVertex::AMOISurfaceVertex()
	: FMOIVertexImplBase()
	, CachedDeprojectedLocation(ForceInitToZero)
{
}

FVector AMOISurfaceVertex::GetLocation() const
{
	return CachedDeprojectedLocation;
}

bool AMOISurfaceVertex::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	auto surfaceGraphObj = GetParentObject();
	if (!ensure(surfaceGraphObj && VertexActor.IsValid()))
	{
		return false;
	}

	switch (DirtyFlag)
	{
		case EObjectDirtyFlags::Structure:
		{
			auto surfaceGraph = GetDocument()->FindSurfaceGraph(surfaceGraphObj->ID);
			auto surfaceVertex = surfaceGraph ? surfaceGraph->FindVertex(ID) : nullptr;
			if (ensureAlways(surfaceVertex))
			{
				FTransform surfaceGraphTransform = surfaceGraphObj->GetWorldTransform();
				CachedDeprojectedLocation = UModumateGeometryStatics::Deproject2DPointTransform(surfaceVertex->Position, surfaceGraphTransform);
				FVector offsetLocation = CachedDeprojectedLocation + (surfaceGraphTransform.GetRotation().GetAxisZ() * AMOISurfaceGraph::VisualNormalOffset);
				VertexActor->SetMOILocation(offsetLocation);

				// Mark the connected edges and polygons dirty
				for (FGraphSignedID connectedEdgeID : surfaceVertex->Edges)
				{
					auto surfaceEdge = surfaceGraph->FindEdge(connectedEdgeID);
					auto edgeMOI = GetDocument()->GetObjectById(FMath::Abs(connectedEdgeID));
					if (ensure(surfaceEdge && edgeMOI))
					{
						edgeMOI->MarkDirty(DirtyFlag);

						for (int32 polyID : {surfaceEdge->LeftPolyID, surfaceEdge->RightPolyID})
						{
							if (polyID != MOD_ID_NONE)
							{
								auto surfacePoly = surfaceGraph->FindPolygon(polyID);
								auto polyMOI = GetDocument()->GetObjectById(polyID);
								if (ensure(surfacePoly && polyMOI) && surfacePoly->bInterior)
								{
									polyMOI->MarkDirty(DirtyFlag);
								}
							}
						}
					}
				}
			}

			break;
		}
		case EObjectDirtyFlags::Visuals:
			UpdateVisuals();
			break;
		default:
			break;
	}

	return true;
}

void AMOISurfaceVertex::GetTangents(TArray<FVector>& OutTangents) const
{
	for (AModumateObjectInstance* connectedMOI : CachedConnectedMOIs)
	{
		if (connectedMOI && (connectedMOI->GetObjectType() == EObjectType::OTSurfaceEdge))
		{
			FVector edgeStart = connectedMOI->GetCorner(0);
			FVector edgeEnd = connectedMOI->GetCorner(1);
			FVector edgeDir = (edgeEnd - edgeStart).GetSafeNormal();

			if (CachedDeprojectedLocation.Equals(edgeStart))
			{
				OutTangents.Add(edgeDir);
			}
			else if (CachedDeprojectedLocation.Equals(edgeEnd))
			{
				OutTangents.Add(-edgeDir);
			}
		}
	}
}
