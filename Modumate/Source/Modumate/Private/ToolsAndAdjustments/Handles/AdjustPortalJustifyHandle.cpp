#include "ToolsAndAdjustments/Handles/AdjustPortalJustifyHandle.h"

#include "Objects/ModumateObjectInstance.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/DimensionManager.h"
#include "UI/DimensionActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "Components/EditableTextBox.h"
#include "Objects/Portal.h"

bool AAdjustPortalJustifyHandle::BeginUse()
{
	if (!ensure(TargetMOI) || !Super::BeginUse())
	{
		return false;
	}

	ADimensionActor* dimensionActor = GameInstance->DimensionManager->AddDimensionActor(ADimensionActor::StaticClass());
	PendingSegmentID = dimensionActor->ID;

	UDimensionWidget* dimensionWidget = dimensionActor->DimensionText;
	dimensionWidget->Measurement->SetIsReadOnly(false);
	dimensionWidget->Measurement->OnTextCommitted.AddDynamic(this, &AAdjustmentHandleActor::OnTextCommitted);

	GameInstance->DimensionManager->SetActiveActorID(PendingSegmentID);

	InitialLocation = Controller->EMPlayerState->SnappedCursor.ScreenPosition;
	InitialState = TargetMOI->GetStateData();
	FMOIPortalData portalData;
	InitialState.CustomData.LoadStructData(portalData);
	InitialJustifyValue = portalData.Justification;
	return true;
}

bool AAdjustPortalJustifyHandle::UpdateUse()
{
	static constexpr float dragScaleFactor = 0.1f;
	Super::UpdateUse();
	if (IsInUse() && TargetMOI)
	{
		FVector2D currentLocation = Controller->EMPlayerState->SnappedCursor.ScreenPosition;
		FVector2D delta = currentLocation - InitialLocation;
		float distance = (delta.Y - delta.X) * dragScaleFactor;
		if (distance != CurrentDistance)
		{
			CurrentDistance = distance;
			ApplyJustification(true);
		}
		ADimensionActor* dimensionActor = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID);
		UDimensionWidget* textBox = dimensionActor->DimensionText;
		currentLocation += FVector2D((delta.X > 0 ? -1 : 1) * 30.0f, -25.0f);  // Offset text box from cursor.
		textBox->UpdateLengthTransform(currentLocation, FVector2D(1.0f, 0.0f), FVector2D::ZeroVector,
			FMath::Abs(CurrentDistance));
	}

	return true;
}

void AAdjustPortalJustifyHandle::EndUse()
{
	Super::EndUse();
	ApplyJustification(false);
}

void AAdjustPortalJustifyHandle::AbortUse()
{
	Super::AbortUse();
	GameState->Document.ClearPreviewDeltas(GetWorld());
}

FVector AAdjustPortalJustifyHandle::GetHandlePosition() const
{
	const FModumateObjectInstance* parentMOI = TargetMOI ? TargetMOI->GetParentObject() : nullptr;
	return parentMOI ? parentMOI->GetLocation() : FVector::ZeroVector;
}

bool AAdjustPortalJustifyHandle::HandleInputNumber(float number)
{
	float sign = FMath::Sign(CurrentDistance);
	CurrentDistance = sign * number;
	EndUse();
	return true;
}

FVector AAdjustPortalJustifyHandle::GetHandleDirection() const
{
	return ensure(TargetMOI) ? TargetMOI->GetNormal(): FVector::ZeroVector;
}

bool AAdjustPortalJustifyHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->PortalJustifyStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);

	return true;
}

void AAdjustPortalJustifyHandle::ApplyJustification(bool preview)
{
	FMOIPortalData portalData;
	GameState->Document.ClearPreviewDeltas(GetWorld());
	auto newDelta = MakeShared<FMOIDelta>();
	FMOIStateData& previewState = newDelta->AddMutationState(TargetMOI);
	previewState.CustomData.LoadStructData(portalData);
	portalData.Justification += CurrentDistance;
	previewState.CustomData.SaveStructData(portalData);
	if (preview)
	{
		GameState->Document.ApplyPreviewDeltas({ newDelta }, GetWorld());
	}
	else
	{
		GameState->Document.ApplyDeltas({ newDelta }, GetWorld());
	}

}
