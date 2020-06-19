// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/CompoundMeshActor.h"
#include "CoreMinimal.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "DocumentManagement/ModumateDynamicObjectBase.h"

#include "ModumateObjectInstanceCabinets.generated.h"


class AEditModelPlayerController_CPP;

UCLASS()
class MODUMATE_API ASelectCabinetFrontHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	static const float FaceCenterHeightOffset;

	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;
};

namespace Modumate
{
	class FModumateObjectInstance;

	class MODUMATE_API FMOICabinetImpl : public FDynamicModumateObjectInstanceImpl
	{
	public:
		FMOICabinetImpl(FModumateObjectInstance *moi);
		virtual ~FMOICabinetImpl();

		virtual FVector GetCorner(int32 index) const override;
		virtual FVector GetNormal() const override;
		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
		virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow);
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;

		static FName CabinetGeometryMatName;

	protected:
		void UpdateToeKickDimensions();
		void UpdateCabinetPortal();

		bool AdjustmentHandlesVisible;
		TArray<TWeakObjectPtr<ASelectCabinetFrontHandle>> FrontSelectionHandles;
		TWeakObjectPtr<ACompoundMeshActor> FrontFacePortalActor;
		FVector2D ToeKickDimensions;
	};
}
