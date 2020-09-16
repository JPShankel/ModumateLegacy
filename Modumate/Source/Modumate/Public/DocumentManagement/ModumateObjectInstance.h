// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDelta.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/ModumateDimensionString.h"
#include "ModumateCore/ModumateTypes.h"
#include "Runtime/Engine/Classes/Materials/MaterialInterface.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UObject/Object.h"

/**
 *
 */

class AActor;
class AAdjustmentHandleActor;
class UIMaterialInterface;
class AEditModelPlayerController_CPP;
class ALineActor;
class UHUDDrawWidget;

struct FMOIDataRecordV1;
class FModumateDocument;

namespace Modumate {
	class FDraftingComposite;
}

class IMiterNode;
class ILayeredObject;
class ISceneCaptureObject;

class FModumateObjectInstance;

struct FStructurePoint
{
	FStructurePoint() {}
	FStructurePoint(const FVector &point, const FVector &dir, int32 cp1, int32 cp2 = -1, int32 objID = MOD_ID_NONE)
		: ObjID(objID)
		, Point(point)
		, Direction(dir)
		, CP1(cp1), CP2(cp2)
	{ }

	int32 ObjID = MOD_ID_NONE;
	FVector Point = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	int32 CP1 = INDEX_NONE;
	int32 CP2 = INDEX_NONE;
};

struct FStructureLine
{
	FStructureLine() {}
	FStructureLine(const FVector &p1, const FVector &p2, int32 cp1, int32 cp2, int32 objID = MOD_ID_NONE)
		: ObjID(objID)
		, P1(p1), P2(p2)
		, CP1(cp1), CP2(cp2)
	{ }

	int32 ObjID = MOD_ID_NONE;
	FVector P1 = FVector::ZeroVector, P2 = FVector::ZeroVector;
	int32 CP1 = 0, CP2 = 0;
};

class MODUMATE_API IModumateObjectInstanceImpl
{
public:
	virtual ~IModumateObjectInstanceImpl() {};

	virtual void SetMaterial(UMaterialInterface *m) = 0;
	virtual UMaterialInterface *GetMaterial() = 0;

	virtual void SetRotation(const FQuat &r) = 0;
	virtual FQuat GetRotation() const = 0;

	virtual void SetLocation(const FVector &p) = 0;
	virtual FVector GetLocation() const = 0;

	virtual void SetWorldTransform(const FTransform &NewTransform) = 0;
	virtual FTransform GetWorldTransform() const = 0;

	virtual FVector GetCorner(int32 index) const = 0;
	virtual int32 GetNumCorners() const = 0;

	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) = 0;

	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *Controller) = 0;
	virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow) = 0;
	virtual void OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool EnableHover) = 0;
	virtual void OnSelected(bool bNewSelected) = 0;

	virtual void OnAssemblyChanged() = 0;

	virtual AActor *RestoreActor() = 0;
	virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) = 0;
	virtual void PostCreateObject(bool bNewObject) = 0;
	virtual void Destroy() = 0;

	virtual FVector GetNormal() const = 0;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) = 0;

	virtual void SetupDynamicGeometry() = 0;
	virtual void UpdateDynamicGeometry() = 0;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const = 0;
	virtual bool GetTriInternalNormalFromEdge(int32 cp1, int32 cp2, FVector &outNormal) const = 0;

	virtual bool IsSelectableByUser() const = 0;
	virtual bool ShowStructureOnSelection() const = 0;
	virtual bool UseStructureDataForCollision() const = 0;

	virtual void TransverseObject() = 0;

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

	FModumateObjectInstanceImplBase(FModumateObjectInstance* InMOI)
		: MOI(InMOI)
	{}

public:
	virtual void SetMaterial(UMaterialInterface *m) override { }
	virtual UMaterialInterface *GetMaterial() override { return nullptr; }

	virtual void SetRotation(const FQuat &r) override;
	virtual FQuat GetRotation() const override;

	virtual void SetLocation(const FVector &p) override;
	virtual FVector GetLocation() const override;

	virtual void SetWorldTransform(const FTransform &NewTransform) override;
	virtual FTransform GetWorldTransform() const override;

	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const;

	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;

	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *Controller) override { }
	virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow) override;
	virtual void OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool EnableHover) override { }
	virtual void OnSelected(bool bNewSelected) override;

	virtual void OnAssemblyChanged() override;

	virtual AActor *RestoreActor() override;
	virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
	virtual void PostCreateObject(bool bNewObject) override;
	virtual void Destroy() override;

	virtual FVector GetNormal() const override { return FVector::ZeroVector; }

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) override;

	virtual void SetupDynamicGeometry() override { }
	virtual void UpdateDynamicGeometry() override { }
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override { }
	virtual bool GetTriInternalNormalFromEdge(int32 cp1, int32 cp2, FVector &outNormal) const override { return false; }

	virtual bool IsSelectableByUser() const override { return true; }
	virtual bool ShowStructureOnSelection() const override { return true; }
	virtual bool UseStructureDataForCollision() const override { return false; }

	virtual void TransverseObject() override { }

	virtual TArray<FModelDimensionString> GetDimensionStrings() const override { return TArray<FModelDimensionString>(); }

	virtual const ILayeredObject* GetLayeredInterface() const override { return nullptr; }
	virtual const IMiterNode* GetMiterInterface() const override { return nullptr; }
	virtual ISceneCaptureObject* GetSceneCaptureInterface() override { return nullptr; }

	virtual void AddDraftingLines(UHUDDrawWidget *HUDDrawWidget) override { };
	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const override { };

	virtual void SetIsDynamic(bool bIsDynamic) override { }
	virtual bool GetIsDynamic() const override { return false; }
};

struct MODUMATE_API FMOIStateData
{
	EMOIDeltaType StateType = EMOIDeltaType::None;
	int32 ObjectID = MOD_ID_NONE;

	// TODO: use this for instance-level overrides
	FBIMPropertySheet ObjectProperties;

	//<Thickness, Height, UNUSED>
	FVector Extents = FVector::ZeroVector;

	TArray<FVector> ControlPoints;
	TArray<int32> ControlIndices;
	bool bObjectInverted = false;

	FVector Location = FVector::ZeroVector;
	FQuat Orientation = FQuat::Identity;

	int32 ParentID = MOD_ID_NONE;

	// Store key instead of whole assembly to avoid old versions of an assembly from being re-applied
	FName ObjectAssemblyKey;

	EObjectType ObjectType = EObjectType::OTNone;

	// TODO: to be deprecated when Commands 2.0 is developed, meantime...
	bool ToParameterSet(const FString &Prefix,Modumate::FModumateFunctionParameterSet &OutParameterSet) const;
	bool FromParameterSet(const FString &Prefix,const Modumate::FModumateFunctionParameterSet &ParameterSet);

	bool operator==(const FMOIStateData& Other) const;
};

class FModumateObjectInstance;

class MODUMATE_API FMOIDelta : public Modumate::FDelta
{
public:

	FMOIDelta() {}
	FMOIDelta(const TArray<FMOIStateData> &States);
	FMOIDelta(const TArray<FModumateObjectInstance*> &Objects);

	void AddCreateDestroyStates(const TArray<FMOIStateData> &States);
	void AddMutationStates(const TArray<FModumateObjectInstance*> &Objects);

	virtual bool ApplyTo(FModumateDocument *doc, UWorld *world) const override;
	virtual TSharedPtr<FDelta> MakeInverse() const override;

	typedef TPair<FMOIStateData, FMOIStateData> FStatePair;
	TArray<FStatePair> StatePairs;
};

class MODUMATE_API FModumateObjectInstance
{
private:

	friend class FMOIDelta;

	TWeakObjectPtr<AActor> MeshActor = nullptr;
	TWeakObjectPtr<UWorld> World = nullptr;
	IModumateObjectInstanceImpl *Implementation = nullptr;
	FModumateDocument *Document = nullptr;

	FMOIStateData CurrentState, PreviewState;

	// Carry a bespoke copy of your assembly
	// TODO: this is motivated by object-level overrides and anticipated serialization needs
	FBIMAssemblySpec ObjectAssembly;

	// TODO: this flag is helpful to keep the above ObjectAssembly layer orders correct, especially because the assembly
	// itself isn't stored in the FMOIStateData. So if CurrentState / PreviewState get modified (or swapped), this flag lets us maintain the layer order.
	bool bAssemblyLayersReversed = false;

	TArray<int32> Children;
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

	FMOIStateData &GetDataState();

	void MakeImplementation();
	void MakeActor(const FVector &Loc, const FQuat &Rot);
	void SetupMOIComponent();

	TArray<TWeakObjectPtr<AAdjustmentHandleActor>> AdjustmentHandles;

public:
	FModumateObjectInstance(UWorld *world, FModumateDocument *doc, const FBIMAssemblySpec &obAsm, int32 id);
	FModumateObjectInstance(UWorld *world, FModumateDocument *doc, const FMOIDataRecord &obRec);
	~FModumateObjectInstance();

	const FMOIStateData &GetDataState() const;

	bool GetObjectInverted() const;
	void SetObjectInverted(bool bNewInverted);

	const FVector &GetExtents() const;
	void SetExtents(const FVector &NewExtents);

	void SetControlPoint(int32 Index, const FVector &Value);
	const FVector &GetControlPoint(int32 Index) const;

	void SetControlPointIndex(int32 IndexNum, int32 IndexVal);
	int32 GetControlPointIndex(int32 IndexNum) const;

	const TArray<FVector> &GetControlPoints() const;
	const TArray<int32> &GetControlPointIndices() const;

	void AddControlPoint(const FVector &ControlPoint);
	void AddControlPointIndex(int32 Index);

	void SetControlPoints(const TArray<FVector> &NewControlPoints);
	void SetControlPointIndices(const TArray<int32> &NewControlPointIndices);

	const TArray<int32> &GetChildren() const {return Children;}

	bool BeginPreviewOperation();
	bool EndPreviewOperation();
	bool GetIsInPreviewMode() const;

	bool SetDataState(const FMOIStateData &DataState);

	int32 GetParentID() const;
	void SetParentID(int32 NewID);

	UWorld *GetWorld() const { return World.Get(); }
	// Actor management
	bool HasActor(const AActor *actor) const { return MeshActor == actor; }
	AActor* GetActor() { return MeshActor.Get(); }
	const AActor* GetActor() const { return MeshActor.Get(); }
	FModumateDocument* GetDocument() { return Document; }
	const FModumateDocument* GetDocument() const { return Document; }

	void Destroy();

	bool IsDestroyed() const { return bDestroyed; }

	void SetParentObject(FModumateObjectInstance *parent, bool bForceUpdate = false);
	FModumateObjectInstance *GetParentObject();

	const FModumateObjectInstance *GetParentObject() const;
	TArray<FModumateObjectInstance *> GetChildObjects();
	TArray<const FModumateObjectInstance *> GetChildObjects() const;
	const TArray<int32> &GetChildIDs() const { return Children; }
	bool AddChild(FModumateObjectInstance *child, bool bUpdateChildHierarchy = true);
	bool RemoveChild(FModumateObjectInstance *child, bool bUpdateChildHierarchy = true);

	void GetConnectedIDs(TArray<int32> &connectedIDs) const;
	void GetConnectedMOIs(TArray<FModumateObjectInstance *> &connectedMOIs) const;

	// Property getters/setters that, for now, only pass through directly to ObjectProperties.
	// TODO: use these for instance-level overrides, where they pass through to the assembly.
	bool HasProperty(EBIMValueScope Scope, const FBIMNameType &Name) const;
	Modumate::FModumateCommandParameter GetProperty(EBIMValueScope Scope, const FBIMNameType &Name) const;
	const FBIMPropertySheet &GetProperties() const;
	void SetProperty(EBIMValueScope Scope, const FBIMNameType &Name, const Modumate::FModumateCommandParameter &Param);
	void SetAllProperties(const FBIMPropertySheet &NewProperties);

	template<class T>
	bool TryGetProperty(EBIMValueScope Scope, const FBIMNameType &Name, T &OutT) const
	{
		return GetProperties().TryGetProperty(Scope, Name, OutT);
	}

	TArray<FModumateObjectInstance *> GetAllDescendents();
	TArray<const FModumateObjectInstance *> GetAllDescendents() const;

	// Geometry
	void MarkDirty(EObjectDirtyFlags NewDirtyFlags);
	bool IsDirty(EObjectDirtyFlags CheckDirtyFlags) const;
	bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas);

	void SetupGeometry();
	void UpdateGeometry();
	void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const;
	bool GetTriInternalNormalFromEdge(int32 cp1, int32 cp2, FVector &outNormal) const;

	// Drafting
	void AddDraftingLines(UHUDDrawWidget *HUDDrawWidget);
	void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const;

	TArray<FModelDimensionString> GetDimensionStrings() const;

	// Wall interface
	// TODO: move some of these to ILayeredObject, or make them more general
	void OnAssemblyChanged();

	const FBIMAssemblySpec &GetAssembly() const;
	void SetAssembly(const FBIMAssemblySpec &NewAssembly);
	void SetAssemblyLayersReversed(bool bNewLayersReversed);

	// TODO: generalize or delete these functions, especially if they are
	// only relevant for objects that will only be hosted by meta planes
	float CalculateThickness() const;
	FVector GetWallDirection() const;
	FVector GetCorner(int32 index) const;
	int32 GetNumCorners() const;

	const ILayeredObject* GetLayeredInterface() const;
	const IMiterNode* GetMiterInterface() const;
	ISceneCaptureObject* GetSceneCaptureInterface();

	// Object delete/restore
	void DestroyActor();
	void RestoreActor();
	void PostCreateObject(bool bNewObject);

	// Object transform getters/setters.
	//   NOTE: ObjectLocation and ObjectRotation are intended to be interpreted by the object;
	//   they may either be world-relative, or parent-relative, or in any arbitrary coordinate space.
	//   WorldTransform, however, is always intended to be in world coordinate space, if that makes sense for the object.
	//   Setting any of these values does not guarantee that getting it will return the same value;
	//   objects will do their best to interpret an intended transform value, based on any constraints it might have.
	FVector GetObjectLocation() const;
	void SetObjectLocation(const FVector &p);
	FQuat GetObjectRotation() const;
	void SetObjectRotation(const FQuat &r);
	void SetWorldTransform(const FTransform &NewTransform);
	FTransform GetWorldTransform() const;

	FVector GetNormal() const;

	void MouseHoverActor(AEditModelPlayerController_CPP *controller, bool EnableHover);
	bool IsHovered() const { return bHovered; }

	void OnSelected(bool bNewSelected);
	bool IsSelected() const { return bSelected; }

	bool IsSelectableByUser() const;
	bool ShowStructureOnSelection() const;
	bool UseStructureDataForCollision() const;

	void InvertObject();
	void TransverseObject();

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
	FMOIDataRecord AsDataRecord() const;

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
