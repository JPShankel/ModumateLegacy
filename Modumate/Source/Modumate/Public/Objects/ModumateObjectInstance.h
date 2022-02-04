// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/ModumateSerialization.h"
#include "GameFramework/Actor.h"
#include "ModumateCore/ModumateDimensionString.h"

#include "Objects/MOIDelta.h"
#include "Objects/MOIState.h"
#include "Objects/MOIStructureData.h"
#include "Quantities/QuantitiesCollection.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "ModumateObjectInstance.generated.h"

class AActor;
class AAdjustmentHandleActor;
class AEditModelPlayerController;
class UHUDDrawWidget;
class UModumateDocument;
class FQuantitiesCollection;
class FDraftingComposite;
class IMiterNode;
class ILayeredObject;
class ISceneCaptureObject;
class FDrawingDesignerLine;


UENUM()
enum class EWebMOIPropertyType : uint8
{
	none=0,
	text,
	boolean,
	color,
	moiId
};

USTRUCT()
struct MODUMATE_API FWebMOIProperty
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	EWebMOIPropertyType Type = EWebMOIPropertyType::text;

	UPROPERTY()
	FString Value;

	UPROPERTY()
	TArray<FString> ValueArray;

	UPROPERTY()
	FString DisplayName;
};

USTRUCT()
struct MODUMATE_API FWebMOI
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ID = 0;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	TArray<int32> Children;

	UPROPERTY()
	int32 Parent = 0;

	UPROPERTY()
	EObjectType Type = EObjectType::OTNone;

	UPROPERTY()
	bool isVisible = true;

	UPROPERTY()
	TMap<FString,FWebMOIProperty> Properties;
};

UCLASS()
class MODUMATE_API AModumateObjectInstance : public AActor
{
	GENERATED_BODY()

public:
	AModumateObjectInstance();

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

protected:
	// TODO: remove when MOIs are actors or DynamicMeshActor is a component that manages procedural meshes.
	TWeakObjectPtr<ADynamicMeshActor> DynamicMeshActor;

public:
	// Object transform getters/setters.
	//   NOTE: GetLocation and GetRotation are intended to be interpreted by the object;
	//   they may either be world-relative, or parent-relative, or in any arbitrary coordinate space.
	//   WorldTransform, however, is always intended to be in world coordinate space, if that makes sense for the object.
	//   Setting any of these values does not guarantee that getting it will return the same value;
	//   objects will do their best to interpret an intended transform value, based on any constraints it might have.
	virtual FVector GetLocation() const;
	virtual FQuat GetRotation() const;
	virtual FTransform GetWorldTransform() const;
	virtual FVector GetNormal() const { return FVector::ZeroVector; }
	virtual FVector GetCorner(int32 index) const;
	virtual int32 GetNumCorners() const;

	virtual bool GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled);

	virtual void SetupAdjustmentHandles(AEditModelPlayerController* Controller) { }
	virtual void ShowAdjustmentHandles(AEditModelPlayerController* Controller, bool bShow);
	virtual bool OnHovered(AEditModelPlayerController* controller, bool bNewHovered);
	virtual bool OnSelected(bool bIsSelected);

	// Can be called when a new assembly is assigned or when an the existing assembly changes structure
	virtual void OnAssemblyChanged();

	virtual AActor* RestoreActor();
	virtual AActor* CreateActor(const FVector& loc, const FQuat& rot);
	virtual void PostCreateObject(bool bNewObject);
	virtual void PreDestroy();

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas);

	virtual void SetupDynamicGeometry() { }
	virtual void UpdateDynamicGeometry() { }
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping = false, bool bForSelection = false) const { }
	virtual void ToggleAndUpdateCapGeometry(bool bEnableCap) { }

	virtual bool IsSelectableByUser() const { return true; }
	virtual bool ShowStructureOnSelection() const { return true; }
	virtual bool UseStructureDataForCollision() const { return false; }

	virtual bool GetInvertedState(FMOIStateData& OutState) const { return false; }
	virtual bool GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const { return false; }
	virtual bool GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const { return false; }
	virtual bool GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const { return false; };
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) {}

	virtual TArray<FModelDimensionString> GetDimensionStrings() const { return TArray<FModelDimensionString>(); }

	virtual const ILayeredObject* GetLayeredInterface() const { return nullptr; }
	virtual const IMiterNode* GetMiterInterface() const { return nullptr; }
	virtual ISceneCaptureObject* GetSceneCaptureInterface() { return nullptr; }

	// Drafting
	virtual void AddDraftingLines(UHUDDrawWidget* HUDDrawWidget) { };
	virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
		TArray<TArray<FVector>>& OutPerimeters) const { }
	virtual void GetDrawingDesignerItems(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines, float MinLength = 0.0f) const
	{ }

	virtual void SetIsDynamic(bool bIsDynamic) { }
	virtual bool GetIsDynamic() const { return false; }

	virtual bool GetBoundingLines(TArray<FDrawingDesignerLine>& outBounding) const;

	// Quantity estimates
	virtual bool ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const { return true; }

	bool GetInstanceDataStruct(UScriptStruct*& OutStructDef, void*& OutStructPtr);
	bool GetInstanceDataStruct(UScriptStruct*& OutStructDef, const void*& OutStructPtr) const;

	bool ToWebMOI(FString& OutJson) const;
	bool FromWebMOI(const FString& InJson);

protected:

	friend struct FMOIDelta;

	// This map is filled in by MOI subclasses to provide display name and type information for instance data properties for the web
	TMap<FString, FWebMOIProperty> WebProperties;

	// SUPER TEMPORARY TODO: we don't have a way to expose derived/non-definitional implementation-specific data, so this will be here until MOIs are UObjects.
	friend class UModumateRoofStatics;
	friend class UModumateDocument;

	TWeakObjectPtr<AActor> MeshActor = nullptr;

	UPROPERTY()
	UModumateDocument* Document = nullptr;

	FMOIStateData StateData;

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
	bool bPartiallyDestroyed = false;

	// Sets of named requests to hide or disable collision on this MOI, which allows
	// multiple systems to independently and safely request modifying the
	// default state of this object without them knowing about each other.
	bool bVisible = false;
	TSet<FName> HideRequests;

	bool bCollisionEnabled = false;
	TSet<FName> CollisionDisabledRequests;

	EObjectDirtyFlags DirtyFlags = EObjectDirtyFlags::None;

	virtual void PostLoadInstanceData() {}

	void AddCachedChildID(int32 ChildID);
	void RemoveCachedChildID(int32 ChildID);

	void SetupMOIComponent();
	void UpdateAssemblyFromKey();

	void DestroyActor(bool bFullDelete);

	TArray<TWeakObjectPtr<AAdjustmentHandleActor>> AdjustmentHandles;
	TArray<FStructurePoint> CachedStructurePoints;
	TArray<FStructureLine> CachedStructureLines;

	FQuantitiesCollection CachedQuantities;
	virtual void UpdateQuantities() { };

public:
	FMOIStateData& GetStateData();
	const FMOIStateData& GetStateData() const;
	bool SetStateData(const FMOIStateData& NewStateData);

	bool UpdateStateDataFromObject();
	bool UpdateInstanceData();

	bool BeginPreviewOperation();
	bool EndPreviewOperation();
	bool IsInPreviewMode() const;

	int32 GetParentID() const;
	void SetParentID(int32 NewParentID);

	// Actor management
	bool HasActor(const AActor *actor) const { return MeshActor == actor; }
	AActor* GetActor() { return MeshActor.Get(); }
	const AActor* GetActor() const { return MeshActor.Get(); }
	UModumateDocument* GetDocument() { return Document; }
	const UModumateDocument* GetDocument() const { return Document; }

	void DestroyMOI(bool bFullDelete);
	void RestoreMOI();

	bool IsDestroyed() const { return bDestroyed; }

	AModumateObjectInstance *GetParentObject();
	const AModumateObjectInstance *GetParentObject() const;
	TArray<AModumateObjectInstance *> GetChildObjects();
	TArray<const AModumateObjectInstance *> GetChildObjects() const;
	const TArray<int32> &GetChildIDs() const { return CachedChildIDs; }
	bool HasChildID(int32 ChildID) const;

	void GetConnectedIDs(TArray<int32> &connectedIDs) const;
	void GetConnectedMOIs(TArray<AModumateObjectInstance *> &connectedMOIs) const;

	TArray<AModumateObjectInstance *> GetAllDescendents();
	TArray<const AModumateObjectInstance *> GetAllDescendents() const;

	// Geometry
	void MarkDirty(EObjectDirtyFlags NewDirtyFlags);
	bool IsDirty(EObjectDirtyFlags CheckDirtyFlags) const;
	bool RouteCleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas);

	void UpdateGeometry();
	void RouteGetStructuralPointsAndLines(TArray<FStructurePoint>& OutPoints, TArray<FStructureLine>& OutLines,
		bool bForSnapping = false, bool bForSelection = false, const FPlane& CullingPlane = FPlane(ForceInitToZero));

	const FBIMAssemblySpec &GetAssembly() const;
	void SetAssemblyLayersReversed(bool bNewLayersReversed);

	// TODO: generalize or delete these functions, especially if they are
	// only relevant for objects that will only be hosted by meta planes
	float CalculateThickness() const;

	bool IsHovered() const { return bHovered; }
	bool IsSelected() const { return bSelected; }

	// Manipulation
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

	// Interface for different systems requesting state changes without directly setting them.
	void RequestHidden(const FName &Requester, bool bRequestHidden);
	void RequestCollisionDisabled(const FName &Requester, bool bRequestCollisionDisabled);
	bool IsRequestedHidden() const { return HideRequests.Num() > 0; }
	bool IsVisible() const { return bVisible; }
	bool IsCollisionRequestedDisabled() const { return CollisionDisabledRequests.Num() > 0; }
	bool IsCollisionEnabled() const { return bCollisionEnabled; }

	// Function exposed to explicitly request objects to re-evaluate the visibility and collision,
	// in cases where it's affected by global state rather than just the above per-object requests.
	bool TryUpdateVisuals();

	// Class Data
	EObjectType GetObjectType() const;

	// Instance Data
	int32 ID = 0;
};
