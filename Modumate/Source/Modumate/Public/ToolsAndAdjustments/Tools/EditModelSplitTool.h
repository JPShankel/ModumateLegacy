// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "EditModelToolbase.h"
#include "EditModelSelectTool.h"

#include "EditModelSplitTool.generated.h"

class AEditModelPlayerController_CPP;
struct FSnappedCursor;

namespace Modumate {
	class FModumateObjectInstance;
}

UCLASS()
class USplitObjectTool : public UEditModelToolBase, public FSelectedObjectToolMixin
{
	GENERATED_BODY()

private:
	Modumate::FModumateObjectInstance * LastValidTarget;
	FVector LastValidSplitStart, LastValidSplitEnd;
	int32 LastValidStartCP1, LastValidStartCP2, LastValidEndCP1, LastValidEndCP2;
	int32 ChosenCornerCP;

	enum ESplitStage
	{
		Hovering = 0,
		SplittingFromPoint
	};

	ESplitStage Stage;

public:
	USplitObjectTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_SPLIT; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool FrameUpdate() override;

	bool PerformSplit() const;

	bool HasValidEdgeSplitTarget() const;
	bool HasValidCornerHoverTarget() const;
	bool HasValidCornerSplitTarget() const;

	static bool FindSplitTarget(const Modumate::FModumateObjectInstance *moi, const FSnappedCursor &snapCursor, ESplitStage splitStage,
		FVector &refSplitStart, FVector &outSplitEnd, FVector &outSplitStart2, FVector &outSplitEnd2,
		int32 &refStartCP1, int32 &refStartCP2, int32 &outEndCP1, int32 &outEndCP2);
};
