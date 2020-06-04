#include "UnrealClasses/DimensionWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateUnits.h"

#define LOCTEXT_NAMESPACE "UDimensionWidget"

UDimensionWidget::UDimensionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, OffsetFromEdge(3.0f)
{

}

void UDimensionWidget::SetTarget(int32 InTargetEdgeID, int32 InTargetObjID)
{
	TargetEdgeID = InTargetEdgeID;
	TargetObjID = InTargetObjID;

	UWorld *world = GetWorld();
	GameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
	if (!ensure(GameState))
	{
		return;
	}
}

void UDimensionWidget::UpdateTransform()
{
	auto volumeGraph = GameState->Document.GetVolumeGraph();
	auto targetEdge = volumeGraph.FindEdge(FMath::Abs(TargetEdgeID));

	auto controller = GetOwningPlayer();

	FVector2D targetScreenPosition;
	if (targetEdge && controller && controller->ProjectWorldLocationToScreen(targetEdge->CachedMidpoint, targetScreenPosition))
	{
		auto startVertex = volumeGraph.FindVertex(targetEdge->StartVertexID);
		auto endVertex = volumeGraph.FindVertex(targetEdge->EndVertexID);

		float length = (endVertex->Position - startVertex->Position).Size();

		FText lengthText;
		SanitizeInput(length, lengthText);
		Measurement->SetText(lengthText);

		// Calculate angle/offset
		// if the target object is a face, determine the offset direction from the cached edge normals 
		// so that the measurement doesn't overlap with the handles
		FVector2D offsetDirection;
		FVector2D projStart, projEnd, edgeDirection;
		controller->ProjectWorldLocationToScreen(startVertex->Position, projStart);
		controller->ProjectWorldLocationToScreen(endVertex->Position, projEnd);
		edgeDirection = projEnd - projStart;
		edgeDirection.Normalize();

		auto targetFace = volumeGraph.FindFace(TargetObjID);
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

		SetPositionInViewport(targetScreenPosition - ((OffsetFromEdge + widgetSize.Y / 2.0f) * offsetDirection));
		float angle = FMath::RadiansToDegrees(FMath::Atan2(edgeDirection.Y, edgeDirection.X));
		angle = FRotator::ClampAxis(angle);

		if (angle > 90.0f && angle <= 270.0f)
		{
			angle -= 180.0f;
		}

		RootBorder->SetRenderTransformAngle(angle);
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

#undef LOCTEXT_NAMESPACE
