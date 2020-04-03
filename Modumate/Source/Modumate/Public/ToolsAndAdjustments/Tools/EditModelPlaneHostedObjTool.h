// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "EditModelMetaPlaneTool.h"
#include "EditModelPlayerState_CPP.h"
#include "ModumateObjectAssembly.h"

class ADynamicMeshActor;

namespace Modumate
{
	class FModumateDocument;

	class MODUMATE_API FPlaneHostedObjTool : public FMetaPlaneTool
	{
	protected:
		TWeakObjectPtr<ADynamicMeshActor> PendingObjMesh;
		bool bInverted;
		bool bRequireHoverMetaPlane;
		EObjectType ObjectType;
		FModumateObjectAssembly ObjAssembly;
		int32 LastValidTargetID;
		bool bWasShowingSnapCursor;

		virtual bool ValidatePlaneTarget(const FModumateObjectInstance *PlaneTarget);
		float GetDefaultJustificationValue();

	public:

		FPlaneHostedObjTool(AEditModelPlayerController_CPP *InController, EObjectType InObjectType, EAxisConstraint InAxisConstraint);
		virtual ~FPlaneHostedObjTool();

		virtual bool Activate() override;
		virtual bool HandleInputNumber(double n) override;
		virtual bool Deactivate() override;
		virtual bool BeginUse() override;
		virtual bool EnterNextStage() override;
		virtual bool FrameUpdate() override;
		virtual bool EndUse() override;
		virtual bool AbortUse() override;
		virtual bool HandleSpacebar() override;
		virtual void SetAssembly(const FShoppingItem &key) override;

		virtual bool MakeObject(const FVector &Location, TArray<int32> &newObjIDs) override;

		FColor AffordanceLineColor = FColor::Orange;
		float AffordanceLineThickness = 4.0f;
		float AffordanceLineInterval = 8.0f;
	};
}
