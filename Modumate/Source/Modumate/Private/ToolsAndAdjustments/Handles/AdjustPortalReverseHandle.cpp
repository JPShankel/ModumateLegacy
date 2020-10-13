#include "ToolsAndAdjustments/Handles/AdjustPortalReverseHandle.h"

#include "Objects/ModumateObjectInstance.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Objects/Portal.h"

bool AAdjustPortalReverseHandle::BeginUse()
{
	if (!ensure(TargetMOI))
	{
		return false;
	}

	FMOIStateData currentState = TargetMOI->GetStateData();
	FMOIPortalData portalData;

	auto delta = MakeShared<FMOIDelta>();
	currentState.CustomData.LoadStructData(portalData);
	portalData.bLateralInverted = !portalData.bLateralInverted;
	delta->AddMutationState(TargetMOI).CustomData.SaveStructData(portalData);
	GameState->Document.ApplyDeltas({ delta }, GetWorld());

	return false;
}

FVector AAdjustPortalReverseHandle::GetHandlePosition() const
{
	FVector position(ForceInitToZero);

	const FModumateObjectInstance* parentMOI = TargetMOI ? TargetMOI->GetParentObject() : nullptr;
	if (parentMOI)
	{
		int32 numCorners = parentMOI->GetNumCorners();
		FBox bbox(ForceInitToZero);
		for (int32 c = 0; c < numCorners; ++c)
		{
			bbox += parentMOI->GetCorner(c);
		}
		position = bbox.GetCenter();
		position.Z = 0.25f * bbox.Min.Z + 0.75f * bbox.Max.Z;
	}

	return position;
}

FVector AAdjustPortalReverseHandle::GetHandleDirection() const
{
	return ensure(TargetMOI) ? TargetMOI->GetNormal() ^ FVector::UpVector : FVector::ZeroVector;
}

bool AAdjustPortalReverseHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->InvertStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);

	return true;
}
