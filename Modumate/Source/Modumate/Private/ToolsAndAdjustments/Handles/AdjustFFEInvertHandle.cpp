#include "ToolsAndAdjustments/Handles/AdjustFFEInvertHandle.h"

#include "DocumentManagement/ModumateObjectInstance.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/AdjustmentHandleAssetData.h"

bool AAdjustFFEInvertHandle::BeginUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustFFEInvertHandle::BeginUse"));

	if (!Super::BeginUse())
	{
		return false;
	}

	TargetMOI->InvertObject();

	EndUse();
	return false;
}

FVector AAdjustFFEInvertHandle::GetHandlePosition() const
{
	FVector actorOrigin; FVector actorExtent;
	TargetMOI->GetActor()->GetActorBounds(false, actorOrigin, actorExtent);
	return actorOrigin + FVector(0.f, 0.f, actorExtent.Z);
}

FVector AAdjustFFEInvertHandle::GetHandleDirection() const
{
	// Return the world axis along which the FF&E would be inverted
	return TargetMOI->GetActor()->GetActorQuat().RotateVector(FVector(0.0f, 1.0f, 0.0f));
}

bool AAdjustFFEInvertHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->InvertStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);
	return true;
}
