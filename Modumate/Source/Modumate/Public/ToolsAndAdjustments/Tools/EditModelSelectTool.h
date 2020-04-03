// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditModelToolBase.h"
#include "ModumateSerialization.h"

namespace Modumate
{
	class MODUMATE_API FSelectedObjectToolMixin
	{
	protected:

		AEditModelPlayerController_CPP *SOTMController;

		TMap<FModumateObjectInstance*, FMOIDataRecordV1> OriginalObjectData;

		void AcquireSelectedObjects();
		void RestoreSelectedObjects();
		void ReleaseSelectedObjects();

		FSelectedObjectToolMixin(AEditModelPlayerController_CPP *pc) : SOTMController(pc) {}
		virtual ~FSelectedObjectToolMixin() {}

		static const FName StateRequestTag;
	};

	class MODUMATE_API FSelectTool : public FEditModelToolBase
	{
	public:
		FSelectTool(AEditModelPlayerController_CPP *pc);
		virtual ~FSelectTool();
		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool HandleMouseUp() override;
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool ShowSnapCursorAffordances() override { return false; }
		virtual bool HandleSpacebar() override;

		bool ProcessDragSelect();

		float DoubleClickTime = 0.2f;
		float MinDragDist = 1.0f;

	protected:
		TMap<const FModumateObjectInstance*, float> LastObjectSelectionAttemptTimes;
		FModumateObjectInstance *InitialClickedObject;
		FVector2D InitialClickLocation;
		bool Dragging;
	};
}