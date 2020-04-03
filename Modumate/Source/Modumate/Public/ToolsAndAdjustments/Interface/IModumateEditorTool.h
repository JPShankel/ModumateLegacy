// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "ModumateObjectEnums.h"

struct FShoppingItem;

class AEditModelPlayerController_CPP;

namespace Modumate {

	class FModumateObjectInstance;

	class MODUMATE_API IModumateEditorTool
	{
	public:
		virtual ~IModumateEditorTool() {};

		virtual bool Activate() = 0;
		virtual bool Deactivate() = 0;
		virtual bool IsInUse() const = 0;
		virtual bool IsActive() const = 0;
		virtual bool BeginUse() = 0;
		virtual bool EnterNextStage() = 0;
		virtual bool FrameUpdate() = 0;
		virtual bool EndUse() = 0;
		virtual bool AbortUse() = 0;
		virtual bool ScrollToolOption(int32 dir) = 0;
		virtual bool HandleInputNumber(double n) = 0;
		virtual bool HandleSpacebar() = 0;
		virtual bool HandleControlKey(bool pressed) = 0;
		virtual bool HandleMouseUp() = 0;
		virtual bool ShowSnapCursorAffordances() = 0;
		virtual const FShoppingItem &GetAssembly() const = 0;
		virtual void SetAssembly(const FShoppingItem &str) = 0;
		virtual void SetAxisConstraint(EAxisConstraint AxisConstraint) = 0;
		virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) = 0;
	};


	IModumateEditorTool *MakePlaceObjectTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeMoveObjectTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeRotateObjectTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeSelectTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeSplitObjectTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeWallTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeFloorTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeDoorTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeWindowTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeStairTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeRailTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeCabinetTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeWandTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeFinishTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeCountertopTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeTrimTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeRoofTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeMetaPlaneTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeCutPlaneTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeScopeBoxTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeJoinTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeCreateSimilarTool(AEditModelPlayerController_CPP *controller);
	IModumateEditorTool *MakeStructureLineTool(AEditModelPlayerController_CPP *controller);
}
