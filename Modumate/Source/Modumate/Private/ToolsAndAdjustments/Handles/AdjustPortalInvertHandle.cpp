#include "ToolsAndAdjustments/Handles/AdjustPortalInvertHandle.h"

#include "Objects/ModumateObjectInstance.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

bool AAdjustPortalInvertHandle::BeginUse()
{
	if (!ensure(TargetMOI))
	{
		return false;
	}

	if (TargetMOI)
	{
		auto delta = MakeShared<FMOIDelta>();
		TargetMOI->GetInvertedState(delta->AddMutationState(TargetMOI));
		GameState->Document->ApplyDeltas({ delta }, GetWorld());
	}

	return false;
}

FVector AAdjustPortalInvertHandle::GetHandlePosition() const
{
	FVector position(ForceInitToZero);

	const AModumateObjectInstance* parentMOI = TargetMOI ? TargetMOI->GetParentObject() : nullptr;
	if (parentMOI)
	{
		int32 numCorners = parentMOI->GetNumCorners();
		FBox bbox(ForceInitToZero);
		for (int32 c = 0; c < numCorners; ++c)
		{
			bbox += parentMOI->GetCorner(c);
		}
		position = bbox.GetCenter();
		position.Z = 0.75f * bbox.Min.Z + 0.25f * bbox.Max.Z;
	}

	return position;
}
