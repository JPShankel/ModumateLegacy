// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "EditModelToolBase.h"


class APortalFrameActor_CPP;
class AEditModelPlayerController_CPP;
class ACompoundMeshActor;

namespace Modumate
{
	class FModumateDocument;

	class MODUMATE_API FPortalToolBase : public IModumateEditorTool
	{
	protected:
		AEditModelPlayerController_CPP * Controller;
		ACompoundMeshActor * CursorActor;

		Modumate::FModumateDocument *Document;
		int32 HostID;
		bool bHasBoringError;
		bool bUseFixedOffset;
		FVector WorldPos;
		FVector2D RelativePos;
		FQuat WorldRot, RelativeRot;
		FShoppingItem Assembly;

		bool Active,Inverted;
		bool bValidPortalConfig;

		void SetupCursor();


	public:
		FPortalToolBase(AEditModelPlayerController_CPP *pc);

		virtual ~FPortalToolBase();

		virtual bool Activate() override;
		virtual bool Deactivate() override;
		virtual bool IsInUse() const override { return false; }
		virtual bool IsActive() const override { return Active; }
		virtual bool FrameUpdate() override;
		virtual bool EnterNextStage() override;
		virtual bool EndUse() override;
		virtual bool ScrollToolOption(int32 dir) override;
		virtual bool HandleInputNumber(double n) override;
		virtual bool AbortUse() override;
		virtual bool HandleSpacebar() override;
		virtual bool HandleControlKey(bool pressed) override;
		virtual bool HandleMouseUp() override { return true; }
		virtual bool ShowSnapCursorAffordances() override { return true; }
		virtual const FShoppingItem &GetAssembly() const override { return Assembly; }
		virtual void SetAssembly(const FShoppingItem &key) override;
		virtual void SetAxisConstraint(EAxisConstraint InAxisConstraint) override { };
		virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) override { };
	};


	class MODUMATE_API FDoorTool : public FPortalToolBase
	{
	public:
		virtual ~FDoorTool();
		FDoorTool(AEditModelPlayerController_CPP *pc);
		virtual bool BeginUse() override;
	};

	class MODUMATE_API FWindowTool : public FPortalToolBase
	{
	public:
		virtual ~FWindowTool();
		FWindowTool(AEditModelPlayerController_CPP *pc);
		virtual bool BeginUse() override;
	};


}




