#include "UI/GraphDimensionActor.h"

#include "Components/EditableTextBox.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/LineActor.h"

AGraphDimensionActor::AGraphDimensionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGraphDimensionActor::SetTarget(int32 InTargetEdgeID, int32 InTargetObjID, bool bIsEditable, int32 InTargetSurfaceGraphID)
{
	TargetEdgeID = InTargetEdgeID;
	TargetObjID = InTargetObjID;

	if (InTargetSurfaceGraphID != MOD_ID_NONE)
	{
		SurfaceGraph = Document->FindSurfaceGraph(InTargetSurfaceGraphID);
	}

	if (DimensionText != nullptr)
	{
		DimensionText->SetIsEditable(bIsEditable);
	}
}

ALineActor* AGraphDimensionActor::GetLineActor()
{
	// the relevant actor for this implementation of the dimension interface is contained within the 
	// associated graph edge

	// TODO: this could be useful if something externally should set properties of the line, following the interface
	//return Cast<ALineActor>(Document->GetObjectById(TargetEdgeID)->GetActor());

	ensureAlways(false);
	return nullptr;
}

void AGraphDimensionActor::BeginPlay()
{
	Super::BeginPlay();

	DimensionText->Measurement->OnTextCommitted.AddDynamic(this, &AGraphDimensionActor::OnMeasurementTextCommitted);
}

void AGraphDimensionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DimensionText->Measurement->OnTextCommitted.RemoveDynamic(this, &AGraphDimensionActor::OnMeasurementTextCommitted);

	Super::EndPlay(EndPlayReason);
}

void AGraphDimensionActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector startPosition, endPosition, midpoint, offset;
	offset = FVector::ZeroVector;
	if (!SurfaceGraph.IsValid())
	{
		auto& volumeGraph = Document->GetVolumeGraph();
		auto targetEdge = volumeGraph.FindEdge(FMath::Abs(TargetEdgeID));
		if (targetEdge == nullptr)
		{
			return;
		}

		auto startVertex = volumeGraph.FindVertex(targetEdge->StartVertexID);
		auto endVertex = volumeGraph.FindVertex(targetEdge->EndVertexID);
		if (startVertex == nullptr || endVertex == nullptr)
		{
			return;
		}

		startPosition = startVertex->Position;
		endPosition = endVertex->Position;
		midpoint = targetEdge->CachedMidpoint;

		auto targetFace = volumeGraph.FindFace(TargetObjID);
		bool bOutSameDirection = true;
		int32 edgeIdx = targetFace ? targetFace->FindEdgeIndex(TargetEdgeID, bOutSameDirection) : INDEX_NONE;
		if (edgeIdx != INDEX_NONE)
		{
			offset = targetFace->CachedEdgeNormals[edgeIdx];
		}

		CurrentDirection = targetEdge->CachedDir;
	}
	else
	{
		auto targetEdge = SurfaceGraph->FindEdge(FMath::Abs(TargetEdgeID));
		if (targetEdge == nullptr)
		{
			return;
		}

		auto startVertex = SurfaceGraph->FindVertex(targetEdge->StartVertexID);
		auto endVertex = SurfaceGraph->FindVertex(targetEdge->EndVertexID);
		if (startVertex == nullptr || endVertex == nullptr)
		{
			return;
		}

		auto surfaceGraphObj = Document->GetObjectById(SurfaceGraph->GetID());
		auto surfaceGraphParent = surfaceGraphObj != nullptr ? Document->GetObjectById(surfaceGraphObj->GetParentID()) : nullptr;
		int32 surfaceGraphFaceIndex = UModumateObjectStatics::GetParentFaceIndex(surfaceGraphObj);
		if ((surfaceGraphParent == nullptr) || (surfaceGraphFaceIndex == INDEX_NONE))
		{
			return;
		}

		TArray<FVector> facePoints;
		FVector faceNormal;
		if (!ensure(UModumateObjectStatics::GetGeometryFromFaceIndex(surfaceGraphParent, surfaceGraphFaceIndex, facePoints, faceNormal, AxisX, AxisY)))
		{
			return;
		}
		Origin = facePoints[0];

		startPosition = UModumateGeometryStatics::Deproject2DPoint(startVertex->Position, AxisX, AxisY, Origin);
		endPosition = UModumateGeometryStatics::Deproject2DPoint(endVertex->Position, AxisX, AxisY, Origin);
		midpoint = (endPosition + startPosition) * 0.5f;

		auto targetFace = SurfaceGraph ? SurfaceGraph->FindPolygon(TargetObjID) : nullptr;
		bool bOutSameDirection = true;
		int32 edgeIdx = targetFace ? targetFace->FindPerimeterEdgeIndex(TargetEdgeID, bOutSameDirection) : INDEX_NONE;

		FVector2D dir = endVertex->Position - startVertex->Position;
		dir.Normalize();
		
		FVector2D offset2D;
		if (edgeIdx != INDEX_NONE)
		{
			float sign = bOutSameDirection ? 1.0f : -1.0f;
			offset2D = targetFace->CachedPerimeterEdgeNormals[edgeIdx] * sign;
			offset = AxisX * offset2D.X + AxisY * offset2D.Y;
			offset.Normalize();
		}

		CurrentDirection = endPosition - startPosition;
		CurrentDirection.Normalize();
	}

	CurrentLength = (endPosition - startPosition).Size();

	auto controller = DimensionText->GetOwningPlayer();

	FVector2D targetScreenPosition;
	if (controller && controller->ProjectWorldLocationToScreen(midpoint, targetScreenPosition))
	{
		// Calculate angle/offset
		// if the target object is a face, determine the offset direction from the cached edge normals 
		// so that the measurement doesn't overlap with the handles
		FVector2D projStart, projEnd, edgeDirection, offsetDirection;
		controller->ProjectWorldLocationToScreen(startPosition, projStart);
		controller->ProjectWorldLocationToScreen(endPosition, projEnd);

		edgeDirection = projEnd - projStart;
		edgeDirection.Normalize();

		if (!offset.IsNearlyZero())
		{
			FVector2D screenPositionOffset;
			controller->ProjectWorldLocationToScreen(midpoint + offset, screenPositionOffset);
			offsetDirection = (targetScreenPosition - screenPositionOffset);
			offsetDirection.Normalize();
		}
		else
		{
			// rotate by 90 degrees in 2D
			offsetDirection = FVector2D(edgeDirection.Y, -edgeDirection.X);
		}

		if (DimensionText != nullptr)
		{
			float length = (endPosition - startPosition).Size();
			DimensionText->UpdateLengthTransform(targetScreenPosition, edgeDirection, offsetDirection, length);
		}
	}
}

void AGraphDimensionActor::OnMeasurementTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		auto& volumeGraph = Document->GetVolumeGraph();

		// get the desired length from the text (assuming the text is entered in feet)
		// shrinking the edge be zero length is not allowed
		float lengthValue = UModumateDimensionStatics::StringToFormattedDimension(Text.ToString()).Centimeters;
		float epsilon = SurfaceGraph.IsValid() ? SurfaceGraph->Epsilon : volumeGraph.Epsilon;
		if (lengthValue > epsilon)
		{
			// construct deltas
			TSet<int32> vertexIDs;

			FModumateObjectDeltaStatics::GetTransformableIDs({ TargetObjID }, Document, vertexIDs);

			// the selected object will be translated towards the vertex that it does not contain
			int32 startID = MOD_ID_NONE;
			if (SurfaceGraph.IsValid())
			{
				auto edge = SurfaceGraph->FindEdge(TargetEdgeID);
				startID = edge->StartVertexID;
			}
			else
			{
				auto edge = volumeGraph.FindEdge(TargetEdgeID);
				startID = edge->StartVertexID;
			}

			FVector offsetDir = vertexIDs.Contains(startID) ?
				CurrentDirection : CurrentDirection * -1.0f;

			// lengthValue is the desired resulting length of the edge after the translation,
			// so subtract from the current value to translate by the difference
			float offsetMagnitude = CurrentLength - lengthValue;
			FVector offset = offsetDir * offsetMagnitude;

			TMap<int32, FTransform> objectInfo;
			for (int32 vertexID : vertexIDs)
			{
				if (SurfaceGraph)
				{
					auto vertex = SurfaceGraph->FindVertex(vertexID);

					FVector position = UModumateGeometryStatics::Deproject2DPoint(vertex->Position, AxisX, AxisY, Origin);
					objectInfo.Add(vertexID, FTransform(position + offset));
				}
				else
				{
					auto vertex = volumeGraph.FindVertex(vertexID);
					objectInfo.Add(vertexID, FTransform(vertex->Position + offset));
				}
			}
			FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, Document, GetWorld(), false);
		}
	}

	DimensionText->ResetText();
}
