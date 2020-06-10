#include "UI/PendingSegmentActor.h"

#include "Components/EditableTextBox.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/LineActor.h"

APendingSegmentActor::APendingSegmentActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

ALineActor* APendingSegmentActor::GetLineActor()
{
	return PendingSegment;
}

void APendingSegmentActor::BeginPlay()
{
	Super::BeginPlay();

	CreateWidget();

	// TODO: this can be removed if input can be handled through here
	DimensionText->SetIsEditable(false);

	PendingSegment = GetWorld()->SpawnActor<ALineActor>();
}

void APendingSegmentActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseWidget();

	if (PendingSegment != nullptr)
	{
		PendingSegment->Destroy();
	}

	Super::EndPlay(EndPlayReason);
}

void APendingSegmentActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector2D projStart, projEnd, edgeDirection, offsetDirection;

	auto controller = DimensionText->GetOwningPlayer();
	FVector lineCenter = (PendingSegment->Point2 + PendingSegment->Point1) / 2.0f;

	FVector2D targetScreenPosition;
	if (controller && DimensionText && controller->ProjectWorldLocationToScreen(lineCenter, targetScreenPosition))
	{
		controller->ProjectWorldLocationToScreen(PendingSegment->Point1, projStart);
		controller->ProjectWorldLocationToScreen(PendingSegment->Point2, projEnd);

		edgeDirection = projEnd - projStart;
		edgeDirection.Normalize();

		offsetDirection = FVector2D(edgeDirection.Y, -edgeDirection.X);

		float length = (PendingSegment->Point2 - PendingSegment->Point1).Size();
		DimensionText->UpdateTransform(targetScreenPosition, edgeDirection, offsetDirection, length);
	}
}
