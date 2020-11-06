// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/ModumateDimensionString.h"
#include "ModumateCore/ModumateTypes.h"
#include "Objects/MOIDelta.h"
#include "Objects/MOIState.h"
#include "Objects/MOIStructureData.h"
#include "UObject/Object.h"
#include "UnrealClasses/DynamicMeshActor.h"


class AActor;
class AAdjustmentHandleActor;
class AEditModelPlayerController_CPP;
class UHUDDrawWidget;
class FModumateDocument;

namespace Modumate {
	class FDraftingComposite;
}

class IMiterNode;
class ILayeredObject;
class ISceneCaptureObject;

class FModumateObjectInstance;

class MODUMATE_API IModumateObjectInstanceImpl
{
public:
	virtual ~IModumateObjectInstanceImpl() {};

	virtual void SetMaterial(UMaterialInterface *m) = 0;
	virtual UMaterialInterface *GetMaterial() = 0;

	virtual FVector GetLocation() const = 0;
	virtual FQuat GetRotation() const = 0;
	virtual FTransform GetWorldTransform() const = 0;
	virtual FVector GetNormal() const = 0;
	virtual FVector GetCorner(int32 index) const = 0;
	virtual int32 GetNumCorners() const = 0;

	virtual void GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr) = 0;

	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) = 0;

	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *Controller) = 0;
	virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow) = 0;
	virtual void OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered) = 0;
	virtual void OnSelected(bool bIsSelected) = 0;

	virtual void OnAssemblyChanged() = 0;

	virtual AActor *RestoreActor() = 0;
	virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) = 0;
	virtual void PostCreateObject(bool bNewObject) = 0;
	virtual void Destroy() = 0;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) = 0;

	virtual void SetupDynamicGeometry() = 0;
	virtual void UpdateDynamicGeometry() = 0;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const = 0;

	virtual bool IsSelectableByUser() const = 0;
	virtual bool ShowStructureOnSelection() const = 0;
	virtual bool UseStructureDataForCollision() const = 0;

	virtual bool GetInvertedState(FMOIStateData& OutState) const = 0;
	virtual bool GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const = 0;

	virtual TArray<FModelDimensionString> GetDimensionStrings() const = 0;

	virtual const ILayeredObject* GetLayeredInterface() const = 0;
	virtual const IMiterNode* GetMiterInterface() const = 0;
	virtual ISceneCaptureObject* GetSceneCaptureInterface() = 0;

	virtual void AddDraftingLines(UHUDDrawWidget *HUDDrawWidget) = 0;
	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const = 0;

	virtual void SetIsDynamic(bool bIsDynamic) = 0;
	virtual bool GetIsDynamic() const = 0;
};

class MODUMATE_API FModumateObjectInstanceImplBase : public IModumateObjectInstanceImpl
{
protected:
	FModumateObjectInstance* MOI;

	// TODO: remove when MOIs are actors or DynamicMeshActor is a component that manages procedural meshes.
	TWeakObjectPtr<ADynamicMeshActor> DynamicMeshActor;

	TWeakObjectPtr<UWorld> World;

	FModumateObjectInstanceImplBase(FModumateObjectInstance* InMOI)
		: MOI(InMOI)
	{}

public:
	virtual void SetMaterial(UMaterialInterface *m) override { }
	virtual UMaterialInterface *GetMaterial() override { return nullptr; }

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;
	virtual FTransform GetWorldTransform() const override;
	virtual FVector GetNormal() const override { return FVector::ZeroVector; }
	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;

	virtual void GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr) override;

	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;

	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *Controller) override { }
	virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow) override;
	virtual void OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered) override { }
	virtual void OnSelected(bool bIsSelected) override;

	virtual void OnAssemblyChanged() override;

	virtual AActor *RestoreActor() override;
	virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
	virtual void PostCreateObject(bool bNewObject) override;
	virtual void Destroy() override;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	virtual void SetupDynamicGeometry() override { }
	virtual void UpdateDynamicGeometry() override { }
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override { }

	virtual bool IsSelectableByUser() const override { return true; }
	virtual bool ShowStructureOnSelection() const override { return true; }
	virtual bool UseStructureDataForCollision() const override { return false; }

	virtual bool GetInvertedState(FMOIStateData& OutState) const override { return false; }
	virtual bool GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const override { return false; };

	virtual TArray<FModelDimensionString> GetDimensionStrings() const override { return TArray<FModelDimensionString>(); }

	virtual const ILayeredObject* GetLayeredInterface() const override { return nullptr; }
	virtual const IMiterNode* GetMiterInterface() const override { return nullptr; }
	virtual ISceneCaptureObject* GetSceneCaptureInterface() override { return nullptr; }

	virtual void AddDraftingLines(UHUDDrawWidget *HUDDrawWidget) override { };
	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const override { };

	virtual void SetIsDynamic(bool bIsDynamic) override { }
	virtual bool GetIsDynamic() const override { return false; }
};


class MODUMATE_API FModumateObjectInstance
{
private:

	friend class FMOIDelta;

	// SUPER TEMPORARY TODO: we don't have a way to expose derived/non-definitional implementation-specific data, so this will be here until MOIs are UObjects.
	friend class UModumateRoofStatics;

	TWeakObjectPtr<AActor> MeshActor = nullptr;
	TWeakObjectPtr<UWorld> World = nullptr;
	IModumateObjectInstanceImpl *Implementation = nullptr;
	FModumateDocument *Document = nullptr;

	FMOIStateData_DEPRECATED CurrentState_DEPRECATED, PreviewState_DEPRECATED;
	FMOIStateData StateData_V2;

	// Carry a bespoke copy of your assembly
	// TODO: this is motivated by object-level overrides and anticipated serialization needs
	FBIMAssemblySpec CachedAssembly;

	// TODO: this flag is helpful to keep the above ObjectAssembly layer orders correct, especially because the assembly
	// itself isn't stored in the FMOIStateData. So if CurrentState / PreviewState get modified (or swapped), this flag lets us maintain the layer order.
	bool bAssemblyLayersReversed = false;

	TArray<int32> CachedChildIDs;
	bool bDestroyed = false;
	bool bHovered = false;
	bool bSelected = false;
	bool bPreviewOperationMode = false;

	// Sets of named requests to hide or disable collision on this MOI, which allows
	// multiple systems to independently and safely request modifying the
	// default state of this object without them knowing about each other.
	bool bVisible = false;
	TSet<FName> HideRequests;

	bool bCollisionEnabled = false;
	TSet<FName> CollisionDisabledRequests;

	EObjectDirtyFlags DirtyFlags = EObjectDirtyFlags::None;

	FMOIStateData_DEPRECATED& GetDataState_DEPRECATED();

	bool GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr);
	bool GetTypedInstanceData(UScriptStruct*& OutStructDef, const void*& OutStructPtr) const;

	void AddCachedChildID(int32 ChildID);
	void RemoveCachedChildID(int32 ChildID);

	void SetupMOIComponent();
	void UpdateAssemblyFromKey();

	void DestroyActor(bool bFullDelete);

	TArray<TWeakObjectPtr<AAdjustmentHandleActor>> AdjustmentHandles;

public:
	FModumateObjectInstance(UWorld *world, FModumateDocument *doc, const FMOIStateData& InStateData);
	~FModumateObjectInstance();

	FMOIStateData& GetStateData();
	const FMOIStateData& GetStateData() const;
	bool SetStateData(const FMOIStateData& NewStateData);

	bool UpdateStateDataFromObject();
	bool UpdateInstanceData();

	const FMOIStateData_DEPRECATED &GetDataState_DEPRECATED() const;

	const TArray<int32> &GetControlPointIndices() const;

	bool BeginPreviewOperation_DEPRECATED();
	bool EndPreviewOperation_DEPRECATED();
	bool GetIsInPreviewMode() const;

	bool SetDataState_DEPRECATED(const FMOIStateData_DEPRECATED &DataState);

	int32 GetParentID() const;
	void SetParentID(int32 NewParentID);

	UWorld *GetWorld() const { return World.Get(); }
	// Actor management
	bool HasActor(const AActor *actor) const { return MeshActor == actor; }
	AActor* GetActor() { return MeshActor.Get(); }
	const AActor* GetActor() const { return MeshActor.Get(); }
	FModumateDocument* GetDocument() { return Document; }
	const FModumateDocument* GetDocument() const { return Document; }

	void PostCreateObject(bool bNewObject);
	void Destroy(bool bFullDelete);
	void Restore();

	bool IsDestroyed() const { return bDestroyed; }

	FModumateObjectInstance *GetParentObject();
	const FModumateObjectInstance *GetParentObject() const;
	TArray<FModumateObjectInstance *> GetChildObjects();
	TArray<const FModumateObjectInstance *> GetChildObjects() const;
	const TArray<int32> &GetChildIDs() const { return CachedChildIDs; }
	bool HasChildID(int32 ChildID) const;

	bool AddChild_DEPRECATED(FModumateObjectInstance *child, bool bUpdateChildHierarchy = true);
	bool RemoveChild_DEPRECATED(FModumateObjectInstance *child, bool bUpdateChildHierarchy = true);

	void GetConnectedIDs(TArray<int32> &connectedIDs) const;
	void GetConnectedMOIs(TArray<FModumateObjectInstance *> &connectedMOIs) const;

	// Property getters/setters that, for now, only pass through directly to ObjectProperties.
	// TODO: use these for instance-level overrides, where they pass through to the assembly.
	const FBIMPropertySheet &GetProperties() const;
	FBIMPropertySheet& GetPropertiesNonConst();
	void SetAllProperties(const FBIMPropertySheet &NewProperties);

	template<class T>
	bool TryGetProperty(EBIMValueScope Scope, const FBIMNameType &Name, T &OutT) const
	{
		return GetProperties().TryGetProperty(Scope, Name, OutT);
	}

	template<class T>
	T GetProperty(EBIMValueScope Scope, const FBIMNameType& Name) const
	{
		return GetProperties().GetProperty<T>(Scope, Name);
	}

	template<class T>
	void SetProperty(EBIMValueScope Scope, const FBIMNameType& Name, const T& Value) 
	{
		return GetPropertiesNonConst().SetProperty<T>(Scope, Name, Value);
	}

	TArray<FModumateObjectInstance *> GetAllDescendents();
	TArray<const FModumateObjectInstance *> GetAllDescendents() const;

	// Geometry
	void MarkDirty(EObjectDirtyFlags NewDirtyFlags);
	bool IsDirty(EObjectDirtyFlags CheckDirtyFlags) const;
	bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas);

	void SetupGeometry();
	void UpdateGeometry();
	void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const;

	// Drafting
	void AddDraftingLines(UHUDDrawWidget *HUDDrawWidget);
	void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const;

	const FBIMAssemblySpec &GetAssembly() const;
	void SetAssemblyLayersReversed(bool bNewLayersReversed);

	// TODO: generalize or delete these functions, especially if they are
	// only relevant for objects that will only be hosted by meta planes
	float CalculateThickness() const;

	const ILayeredObject* GetLayeredInterface() const;
	const IMiterNode* GetMiterInterface() const;
	ISceneCaptureObject* GetSceneCaptureInterface();

	// Object transform getters/setters.
	//   NOTE: ObjectLocation and ObjectRotation are intended to be interpreted by the object;
	//   they may either be world-relative, or parent-relative, or in any arbitrary coordinate space.
	//   WorldTransform, however, is always intended to be in world coordinate space, if that makes sense for the object.
	//   Setting any of these values does not guarantee that getting it will return the same value;
	//   objects will do their best to interpret an intended transform value, based on any constraints it might have.
	FVector GetObjectLocation() const;
	FQuat GetObjectRotation() const;
	FTransform GetWorldTransform() const;
	FVector GetNormal() const;
	FVector GetCorner(int32 index) const;
	int32 GetNumCorners() const;

	void MouseHoverActor(AEditModelPlayerController_CPP *controller, bool EnableHover);
	bool IsHovered() const { return bHovered; }

	void OnSelected(bool bNewSelected);
	bool IsSelected() const { return bSelected; }

	bool IsSelectableByUser() const;
	bool ShowStructureOnSelection() const;
	bool UseStructureDataForCollision() const;

	bool GetInvertedState(FMOIStateData& OutState) const;
	bool GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const;

	// Manipulation
	void ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow);
	void ClearAdjustmentHandles();
	const TArray<TWeakObjectPtr<AAdjustmentHandleActor>> &GetAdjustmentHandles() const;
	bool HasAdjustmentHandles() const;

	AAdjustmentHandleActor *MakeHandle(TSubclassOf<AAdjustmentHandleActor> HandleClass);

	template< typename THandleClass >
	THandleClass* MakeHandle()
	{
		return CastChecked<THandleClass>(MakeHandle(THandleClass::StaticClass()), ECastCheckedType::NullAllowed);
	}

	// Whether it can be split - called by both the Document and the SplitTool
	bool CanBeSplit() const;

	// Serialization
	FMOIDataRecord_DEPRECATED AsDataRecord_DEPRECATED() const;

	UMaterialInterface *GetMaterial();
	void SetMaterial(UMaterialInterface *mat);

	// Interface for different systems requesting state changes without directly setting them.
	void RequestHidden(const FName &Requester, bool bRequestHidden);
	void RequestCollisionDisabled(const FName &Requester, bool bRequestCollisionDisabled);
	bool IsRequestedHidden() const { return HideRequests.Num() > 0; }
	bool IsVisible() const { return bVisible; }
	bool IsCollisionRequestedDisabled() const { return CollisionDisabledRequests.Num() > 0; }
	bool IsCollisionEnabled() const { return bCollisionEnabled; }

	// Function exposed to explicitly request objects to re-evaluate the visibility and collision,
	// in cases where it's affected by global state rather than just the above per-object requests.
	void UpdateVisibilityAndCollision();

	// Class Data
	EObjectType GetObjectType() const;

	// Instance Data
	int32 ID = 0;
};
