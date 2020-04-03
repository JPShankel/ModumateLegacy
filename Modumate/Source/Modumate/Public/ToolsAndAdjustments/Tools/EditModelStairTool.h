// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "EditModelToolBase.h"
#include "ModumateSnappedCursor.h"
#include "ModumateObjectAssembly.h"

class AEditModelGameMode_CPP;
class AEditModelGameState_CPP;
class AEditModelPlayerState_CPP;
class ADynamicMeshActor;
class ALineActor3D_CPP;

namespace Modumate
{
	class MODUMATE_API FStairTool : public FEditModelToolBase
	{
	public:
		FStairTool(AEditModelPlayerController_CPP *InController);
		virtual ~FStairTool();

		virtual bool Activate() override;
		virtual bool HandleInputNumber(double n) override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool EnterNextStage() override;
		virtual void SetAssembly(const FShoppingItem &key) override;
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;

	protected:
		enum EState
		{
			Starting,
			RunPending,
			RisePending,
			WidthPending
		};

		bool UpdatePreviewStairs();
		bool MakeStairs(int32 &RefParentPlaneID, int32 &OutStairsID);
		bool ValidatePlaneTarget(const FModumateObjectInstance *PlaneTarget);
		void MakePendingSegment(TWeakObjectPtr<ALineActor3D_CPP> &TargetSegment, const FVector &StartingPoint, const FColor &SegmentColor);
		void ResetState();

		EState CurrentState;
		bool bWasShowingSnapCursor;
		EMouseMode OriginalMouseMode;
		bool bWantedVerticalSnap;
		int32 LastValidTargetID;
		FModumateObjectAssembly ObjAssembly;
		TWeakObjectPtr<ALineActor3D_CPP> RunSegment, RiseSegment, WidthSegment;
		TWeakObjectPtr<ADynamicMeshActor> PendingObjMesh;
		TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;
		TWeakObjectPtr<AEditModelGameState_CPP> GameState;
		FVector RunStartPos, RiseStartPos, RiseEndPos, WidthEndPos;
		FVector RunDir, WidthDir;
		float DesiredTreadDepth, CurrentTreadDepth, CurrentRiserHeight, CurrentWidth;
		int32 DesiredTreadNum, CurrentTreadNum, CurrentRiserNum;
		bool bWantStartRiser, bWantEndRiser, bFixTreadDepth;

		TArray<TArray<FVector>> CachedTreadPolys;
		TArray<TArray<FVector>> CachedRiserPolys;
		TArray<FVector> CachedRiserNormals;

		FColor TargetPlaneDotColor = FColor::Orange;
		float TargetPlaneDotThickness = 4.0f;
		float TargetPlaneDotInterval = 8.0f;
		FColor RunSegmentColor, RiseSegmentColor, WidthSegmentColor;
	};
}
