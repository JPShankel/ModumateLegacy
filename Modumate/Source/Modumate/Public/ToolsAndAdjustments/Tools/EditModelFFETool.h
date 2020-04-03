// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ModumateObjectAssembly.h"
#include "EditModelToolBase.h"

class ACompoundMeshActor;

namespace Modumate
{
	class FPlaceObjectTool : public FEditModelToolBase
	{
	private:
		AStaticMeshActor * Cursor;
		ACompoundMeshActor* CursorCompoundMesh;
	public:

		FPlaceObjectTool(AEditModelPlayerController_CPP *pc);
		virtual ~FPlaceObjectTool();

		FModumateObjectAssembly CurrentFFEAssembly;

		virtual bool Activate() override;
		virtual bool ScrollToolOption(int32 dir) override;
		virtual bool Deactivate() override;

		virtual bool FrameUpdate() override;
		virtual bool BeginUse() override;
	};
}