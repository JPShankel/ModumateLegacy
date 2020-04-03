// Copyright 2020 Modumate, Inc. All Rights Reserved.

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
	class MODUMATE_API FStructureLineTool : public FEditModelToolBase
	{
	public:
		FStructureLineTool(AEditModelPlayerController_CPP *InController);
		virtual ~FStructureLineTool();

		virtual bool Activate() override;
		virtual bool HandleInputNumber(double n) override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool EnterNextStage() override;
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;
		virtual void SetAssembly(const FShoppingItem &key) override;
		virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) override;

	protected:
		void SetTargetID(int32 NewTargetID);
		bool SetStructureLineHidden(int32 StructureLineID, bool bHidden);
		bool UpdatePreviewStructureLine();
		bool MakeStructureLine(int32 TargetEdgeID = MOD_ID_NONE);
		void ResetState();

		bool bHaveSetUpGeometry;
		bool bWasShowingSnapCursor;
		EMouseMode OriginalMouseMode;
		bool bWantedVerticalSnap;
		int32 LastValidTargetID;
		int32 LastTargetStructureLineID;
		FModumateObjectAssembly ObjAssembly;
		TWeakObjectPtr<ALineActor3D_CPP> PendingSegment;
		TWeakObjectPtr<ADynamicMeshActor> PendingObjMesh;
		TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;
		TWeakObjectPtr<AEditModelGameState_CPP> GameState;
		FVector LineStartPos, LineEndPos, LineDir, ObjNormal, ObjUp;

		FColor TargetEdgeDotColor = FColor::Orange;
		float TargetEdgeDotThickness = 4.0f;
		float TargetEdgeDotInterval = 8.0f;
	};
}
