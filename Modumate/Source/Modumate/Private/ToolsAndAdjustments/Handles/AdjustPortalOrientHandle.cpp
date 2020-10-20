#include "ToolsAndAdjustments/Handles/AdjustPortalOrientHandle.h"

#include "Objects/ModumateObjectInstance.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/AdjustmentHandleWidget.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Objects/Portal.h"

bool AAdjustPortalOrientHandle::BeginUse()
{
	if (!ensure(TargetMOI) || !ensure(TargetMOI->GetParentObject()) )
	{
		return false;
	}

	FModumateObjectInstance* parent = TargetMOI->GetParentObject();
	FModumateDocument* document = parent->GetDocument();

	const int32 numCorners = parent->GetNumCorners();
	if (!ensureAlwaysMsgf(numCorners == 4, TEXT("Portal parent does not have 4 vertices")))
	{
		return false;
	}

	FMOIPortalData portalData;
	auto newDelta = MakeShared<FMOIDelta>();

	FMOIStateData& portalState = newDelta->AddMutationState(TargetMOI);
	portalState.CustomData.LoadStructData(portalData);
	portalData.Orientation = portalData.Orientation + (CounterClockwise ? +1 : -1);
	portalState.CustomData.SaveStructData(portalData);
	document->ApplyDeltas({ newDelta }, GetWorld());

	return false;
}

FVector AAdjustPortalOrientHandle::GetHandleDirection() const
{
	auto parent = TargetMOI->GetParentObject();
	bool bFacing = (Controller->PlayerCameraManager->GetCameraRotation().Quaternion().GetAxisX() | TargetMOI->GetNormal()) <= 0;
	bool bCanonical = bFacing ^ CounterClockwise;
	if (parent)
	{
		if (bCanonical)
		{
			Widget->SetRenderScale(FVector2D(1.0f, 1.0f));
		}
		else
		{
			Widget->SetRenderScale(FVector2D(-1.0f, 1.0f));
		}
		return (bFacing ? +1 : -1) * parent->GetObjectRotation().GetAxisX();
	}
	return FVector::RightVector;
}

FVector AAdjustPortalOrientHandle::GetHandlePosition() const
{
	if (!ensure(Controller && TargetMOI))
	{
		return FVector::ZeroVector;
	}

	auto parent = TargetMOI->GetParentObject();
	if (parent)
	{
		FVector objectPos = parent->GetObjectLocation();
		FQuat objectRotation = parent->GetObjectRotation();
		FVector attachDirectionY = -objectRotation.GetAxisY();
		FVector attachDirectionX = objectRotation.GetAxisX();
		FVector attachDirection = ((CounterClockwise ? +1 : -1 ) * attachDirectionX - attachDirectionY).GetSafeNormal();
		static constexpr float desiredWorldDist = 20.0;

		FVector attachPosWorld;
		FVector2D attachPosScreen;
		if (Controller->GetScreenScaledDelta(objectPos, attachDirection, desiredWorldDist, 1000.0f, attachPosWorld, attachPosScreen))
		{
			return attachPosWorld;
		}
	}

	return FVector::ZeroVector;
}

bool AAdjustPortalOrientHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->RotateCWStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);

	return true;
}
