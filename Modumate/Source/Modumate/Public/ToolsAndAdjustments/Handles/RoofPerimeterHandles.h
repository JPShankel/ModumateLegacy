// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Graph/ModumateGraph3DTypes.h"
#include "ModumateCore/ModumateRoofStatics.h"
#include "ToolsAndAdjustments/Common/EditModelAdjustmentHandleBase.h"
#include "UnrealClasses/AdjustmentHandleActor_CPP.h"

namespace Modumate
{
	class MODUMATE_API FCreateRoofFacesHandle : public FEditModelAdjustmentHandleBase
	{
	public:
		FCreateRoofFacesHandle(FModumateObjectInstance *MOI)
			: FEditModelAdjustmentHandleBase(MOI)
		{ }

		virtual bool OnBeginUse() override;
		virtual bool OnEndUse() override;
		virtual FVector GetAttachmentPoint() override;

	protected:
		TArray<FVector> EdgePoints;
		TArray<int32> EdgeIDs;
		FRoofEdgeProperties DefaultEdgeProperties;
		TArray<FRoofEdgeProperties> EdgeProperties;

		TArray<FVector> CombinedPolyVerts;
		TArray<int32> PolyVertIndices;
	};

	class MODUMATE_API FRetractRoofFacesHandle : public FEditModelAdjustmentHandleBase
	{
	public:
		FRetractRoofFacesHandle(FModumateObjectInstance *MOI)
			: FEditModelAdjustmentHandleBase(MOI)
		{ }

		virtual bool OnBeginUse() override;
		virtual bool OnEndUse() override;
		virtual FVector GetAttachmentPoint() override;

	protected:
		TSet<FTypedGraphObjID> TempGroupMembers;
		TArray<int32> TempFaceIDs;
	};

	class MODUMATE_API FEditRoofEdgeHandle : public FEditModelAdjustmentHandleBase
	{
	public:
		FEditRoofEdgeHandle(FModumateObjectInstance *MOI, FSignedID InTargetEdgeID)
			: FEditModelAdjustmentHandleBase(MOI)
			, TargetEdgeID(InTargetEdgeID)
		{ }

		virtual bool OnBeginUse() override;
		virtual bool OnUpdateUse() override;
		virtual void Tick(float DeltaTime) override;
		virtual bool OnEndUse() override;
		virtual bool OnAbortUse() override;
		virtual FVector GetAttachmentPoint() override;

		FSignedID TargetEdgeID;
		TWeakObjectPtr<URoofPerimeterPropertiesWidget> PropertiesWidget;
	};
}
