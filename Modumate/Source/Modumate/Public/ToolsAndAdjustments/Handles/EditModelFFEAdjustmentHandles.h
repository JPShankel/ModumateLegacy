// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"


namespace Modumate
{
	class MODUMATE_API FAdjustFFEPointHandle : public FEditModelAdjustmentHandleBase
	{
	private:
		int32 HandleID;
		FVector AssemblyNormal;
		FVector AssemblyTangent;
		FVector LocalHandlePos;

		FVector AnchorLoc;
		FVector OriginalObjectLoc;
		FQuat OriginalObjectRot;
		FVector OriginalHandleLoc;
		
		TArray<FVector> LastValidPendingCPLocations;
		FVector MeshExtent;

		bool UpdateTarget();

	public:

		FAdjustFFEPointHandle(FModumateObjectInstance *moi, int32 cp);

		virtual bool OnBeginUse() override;
		virtual bool OnUpdateUse() override;
		virtual bool OnEndUse() override;
		virtual bool OnAbortUse() override;
		virtual FVector GetAttachmentPoint() override;
		virtual bool HandleInputNumber(float number) override;
	};

	class MODUMATE_API FAdjustFFERotateHandle : public FEditModelAdjustmentHandleBase
	{
	private:
		float Sign;
		FVector AssemblyNormal;
		FVector AssemblyTangent;
		FVector LocalHandlePos;

		TArray<FVector> OriginalControlPoints;
		FPlane OriginalPlane;
		float OriginalExtrusion;
		float LastValidExtrusion;
		FVector AnchorDirLocation; // a proxy location that defines the starting direction
		FVector AnchorLoc; // pivot of the rotation handle
		FQuat OriginalRotation;
		bool bClockwise = true;

	public:

		FAdjustFFERotateHandle(FModumateObjectInstance *moi, float sign = 1.0f);

		virtual bool OnBeginUse() override;
		virtual bool OnUpdateUse() override;
		virtual bool OnEndUse() override;
		virtual bool OnAbortUse() override;
		virtual FVector GetAttachmentPoint() override;
		virtual bool GetOverrideHandleRotation(FQuat &OutRotation) override;
		virtual bool HandleInputNumber(float number) override;
		FQuat GetAnchorQuatFromCursor();
	};

	class MODUMATE_API FAdjustFFEInvertHandle : public FEditModelAdjustmentHandleBase
	{
	public:

		FAdjustFFEInvertHandle(FModumateObjectInstance *moi) :
			FEditModelAdjustmentHandleBase(moi)
		{}

		virtual bool OnBeginUse() override;
		virtual FVector GetAttachmentPoint() override;
	};
}