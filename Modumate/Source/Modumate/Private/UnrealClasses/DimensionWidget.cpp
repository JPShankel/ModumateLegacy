#include "UnrealClasses/DimensionWidget.h"

#include "Components/EditableTextBox.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"
#include "Graph/ModumateGraph3DTypes.h"

#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateUnits.h"

#define LOCTEXT_NAMESPACE "UDimensionWidget"

UDimensionWidget::UDimensionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UDimensionWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!Measurement)
	{
		return false;
	}

	Measurement->OnTextCommitted.AddDynamic(this, &UDimensionWidget::OnMeasurementTextCommitted);

	return true;
}

void UDimensionWidget::SetTarget(int32 InTargetEdgeID, int32 InTargetObjID, bool bIsEditable)
{
	TargetEdgeID = InTargetEdgeID;
	TargetObjID = InTargetObjID;

	UWorld *world = GetWorld();
	GameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
	if (!ensure(GameState))
	{
		return;
	}
	Graph = &GameState->Document.GetVolumeGraph();

	Measurement->SetIsReadOnly(!bIsEditable);
	!bIsEditable ? Measurement->WidgetStyle.SetFont(ReadOnlyFont) : Measurement->WidgetStyle.SetFont(EditableFont);

	UpdateText();
}

void UDimensionWidget::UpdateTransform()
{
	auto targetEdge = Graph->FindEdge(FMath::Abs(TargetEdgeID));

	auto controller = GetOwningPlayer();

	FVector2D targetScreenPosition;
	if (targetEdge && controller && 
		controller->ProjectWorldLocationToScreen(targetEdge->CachedMidpoint, targetScreenPosition))
	{
		auto startVertex = Graph->FindVertex(targetEdge->StartVertexID);
		auto endVertex = Graph->FindVertex(targetEdge->EndVertexID);

		// Calculate angle/offset
		// if the target object is a face, determine the offset direction from the cached edge normals 
		// so that the measurement doesn't overlap with the handles
		FVector2D offsetDirection;
		FVector2D projStart, projEnd, edgeDirection;
		controller->ProjectWorldLocationToScreen(startVertex->Position, projStart);
		controller->ProjectWorldLocationToScreen(endVertex->Position, projEnd);

		edgeDirection = projEnd - projStart;
		edgeDirection.Normalize();

		auto targetFace = Graph->FindFace(TargetObjID);
		bool bOutSameDirection;
		int32 edgeIdx = targetFace ? targetFace->FindEdgeIndex(TargetEdgeID, bOutSameDirection) : INDEX_NONE;
		if (targetFace && edgeIdx != INDEX_NONE)
		{
			FVector offset = targetFace->CachedEdgeNormals[edgeIdx];
			FVector2D screenPositionOffset;
			controller->ProjectWorldLocationToScreen(targetEdge->CachedMidpoint + offset, screenPositionOffset);
			offsetDirection = (targetScreenPosition - screenPositionOffset);
			offsetDirection.Normalize();
		}
		else
		{
			// rotate by 90 degrees in 2D
			offsetDirection = FVector2D(edgeDirection.Y, -edgeDirection.X);
		}
		FVector2D widgetSize = GetCachedGeometry().GetAbsoluteSize();

		SetPositionInViewport(targetScreenPosition - ((PixelOffset + widgetSize.Y / 2.0f) * offsetDirection));
		float angle = FMath::RadiansToDegrees(FMath::Atan2(edgeDirection.Y, edgeDirection.X));
		angle = FRotator::ClampAxis(angle);

		if (angle > 90.0f && angle <= 270.0f)
		{
			angle -= 180.0f;
		}

		Measurement->SetRenderTransformAngle(angle);

		// Update text is called here to make sure that the text matches the current length
		// even if something else changed the length (ex. Undo)
		UpdateText();
	}
}

void UDimensionWidget::InitializeNativeClassData()
{
	Super::InitializeNativeClassData();
}

void UDimensionWidget::SanitizeInput(float InLength, FText &OutText)
{
	// TODO: this is assuming the input is in cm, because that's how the data is stored in the volume graph
	InLength /= Modumate::InchesToCentimeters;

	// TODO: potentially this is a project setting
	int32 tolerance = 64;

	int32 feet = InLength / 12;

	InLength -= (feet * 12);

	int32 inches = InLength;

	InLength -= inches;

	InLength *= tolerance;

	// rounding here allows for rounding based on the tolerance
	// (ex. rounding to the nearest 1/64")
	int32 numerator = FMath::RoundHalfToZero(InLength);
	int32 denominator = tolerance;
	while (numerator % 2 == 0 && numerator != 0)
	{
		numerator /= 2;
		denominator /= 2;
	}
	// carry
	if (denominator == 1)
	{
		inches++;
		numerator = 0;
	}
	if (inches == 12)
	{
		feet++;
		inches -= 12;
	}

	FText feetText = feet != 0 ? FText::Format(LOCTEXT("feet", "{0}'-"), feet) : FText();
	FText inchesText;

	if (numerator != 0)
	{
		inchesText = FText::Format(LOCTEXT("inches_frac", "{0} {1}/{2}\""), inches, numerator, denominator);
	}
	else
	{
		inchesText = FText::Format(LOCTEXT("inches", "{0}\""), inches);
	}

	// if there are both feet and inches, separate with a dash, otherwise use the one that exists
	OutText = FText::Format(LOCTEXT("feet_and_inches", "{0}{1}"), feetText, inchesText);
}

void UDimensionWidget::OnMeasurementTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	bool bVerticesMoved = false;
	if (CommitMethod == ETextCommit::OnEnter)
	{
		float lengthValue;
		// get the desired length from the text (assuming the text is entered in feet)
		// shrinking the edge be zero length is not allowed
		if (UModumateDimensionStatics::TryParseInputNumber(Text.ToString(), lengthValue) && lengthValue > Graph->Epsilon)
		{
			lengthValue *= Modumate::InchesPerFoot * Modumate::InchesToCentimeters;

			// construct deltas

			TArray<int32> vertexIDs;

			// gather vertices that will be translated (all vertices contained within the selected object)
			if (auto vertex = Graph->FindVertex(TargetObjID))
			{
				vertexIDs = { TargetObjID };
			}
			else if (auto edge = Graph->FindEdge(TargetObjID))
			{
				vertexIDs = { edge->StartVertexID, edge->EndVertexID };
			}
			else if (auto face = Graph->FindFace(TargetObjID))
			{
				vertexIDs = face->VertexIDs;
			}

			// the selected object will be translated towards the vertex that it does not contain
			auto edge = Graph->FindEdge(TargetEdgeID);
			auto startVertex = Graph->FindVertex(edge->StartVertexID);
			auto endVertex = Graph->FindVertex(edge->EndVertexID);
			FVector offsetDir = vertexIDs.Find(startVertex->ID) != INDEX_NONE ?
				edge->CachedDir : edge->CachedDir * -1.0f;

			// lengthValue is the desired resulting length of the edge after the translation,
			// so subtract from the current value to translate by the difference
			float edgeLength = (endVertex->Position - startVertex->Position).Size();
			float offsetMagnitude = edgeLength - lengthValue;
			FVector offset = offsetDir * offsetMagnitude;

			TArray<FVector> positions;
			for (int32 vertexID : vertexIDs)
			{
				auto vertex = Graph->FindVertex(vertexID);
				positions.Add(vertex->Position + offset);
			}

			auto &doc = GameState->Document;
			bVerticesMoved = doc.MoveMetaVertices(GetWorld(), vertexIDs, positions);

		}
	}

	if (bVerticesMoved)
	{
		UpdateText();
	}
	else
	{
		Measurement->SetText(LastCommittedText);
	}

}

void UDimensionWidget::UpdateText()
{
	// check current edge length against value saved in text
	auto targetEdge = Graph->FindEdge(FMath::Abs(TargetEdgeID));
	auto startVertex = Graph->FindVertex(targetEdge->StartVertexID);
	auto endVertex = Graph->FindVertex(targetEdge->EndVertexID);

	float currentLength = (endVertex->Position - startVertex->Position).Size();

	if (currentLength != LastLength)
	{
		FText newText;
		SanitizeInput(currentLength, newText);
		Measurement->SetText(newText);
		LastCommittedText = newText;
		LastLength = currentLength;
	}
}

#undef LOCTEXT_NAMESPACE
