// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/Box.h"
#include "BIMKernel/BIMKey.h"
#include "ModumateCore/ModumateTypes.h"
#include "ModumateCore/ModumateDimensionString.h"
#include "UObject/Object.h"

#include "ModumateFunctionLibrary.generated.h"


/**
*
*/

class ADynamicMeshActor;

UCLASS()
class MODUMATE_API UModumateFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static bool IsWithEditor();

	// Rotate a list of IDs to start at the minimum, preserving the order of the rest of the elements. Returns true if the ID list can be (or already is) normalized.
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static bool NormalizeIDs(TArray<int32>& IDs);

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static bool AreNormalizedIDListsEqual(const TArray<int32>& IDList1, const TArray<int32>& IDList2);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static bool IsVectorInArray(const TArray<FVector>& Array, const FVector& TargetVector, float Tolerance);

	// Given a component reference (size radius should be 100.0), calculate how much new scale needs to be in order to maintain constant screen size
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static FVector GetWorldComponentToScreenSizeScale(UStaticMeshComponent* Target, FVector DesiredScreenSize);

	// Change component transform as if it is like a material billboard
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static void ComponentAsBillboard(UStaticMeshComponent* Target, FVector DesiredScreenSize);

	// Change component location to as close to camera as possible, acting like an UI element
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static void ComponentToUIScreenPosition(UStaticMeshComponent* Target, FVector WorldLocation, FVector ScreenOffset = FVector::ZeroVector);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static bool LineBoxIntersection(const FBox2D &box, const FVector2D &start, const FVector2D &end);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static void ScreenLineExtend(FVector2D& ReturnStart, FVector2D& ReturnEnd, FVector2D Start, FVector2D End, float StartExtendDistance, float EndExtendDistance);

	// Code from UE4 forum for projecting 3D coordinate to 2D position. Use for getting 3D position that's behind Player's camera
	// https://forums.unrealengine.com/development-discussion/blueprint-visual-scripting/74693-project-world-to-screen-location-stops-returning-a-value
	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static bool ProjectWorldToScreenBidirectional(APlayerController const* Player, const FVector& WorldPosition, FVector2D& ScreenPosition, bool& bTargetBehindCamera, bool bPlayerViewportRelative = false);

	// This will limit position of start and end to always be in front of the camera, good for using before screen projection. Return true if both points are in front of camera
	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static bool ProjectLineToCameraPlane(APlayerController const* player, const FVector point1, const FVector point2, FVector& newPoint1, FVector& newPoint2);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static float GetViewportDPIScale();

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static void AddNewDimensionString(const AEditModelPlayerController_CPP *controller,
		const FVector &p1,
		const FVector &p2,
		const FVector &offsetDir, // Direction the text of the dim string will be offset
		const FName &groupID, // The group this dim string will be part of. Used when user press tab to switch between dimension
		const FName &uniqueID, // Useful for identification, ex: "delta", "total"
		const int32 groupIndex, // Order inside the dim string group
		const AActor* owner, // Useful for contextual purpose. ex: Text box can can know what the current object type is
		EDimStringStyle style = EDimStringStyle::Fixed,
		EEnterableField functionality = EEnterableField::NonEditableText,
		const float offset = 40.f,
		EAutoEditableBox autoTextBox = EAutoEditableBox::UponUserInput, // Auto turn functionality from nonEditable to Editable
		const bool alwaysVisible = false, // This dim string will always be visible
		const FLinearColor &color = FLinearColor::White);

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static void AddNewDegreeString(const AEditModelPlayerController_CPP *controller,
		const FVector &location, // The world location showing the degree string
		const FVector &start, // Form a line between "start" position and "location", use that line as anchor line
		const FVector &end, // Form a line between "end" position and "location" use the angle between anchor line and this line to calculate degree
		const bool clockwise,
		const FName &groupID,
		const FName &uniqueID,
		const int32 groupIndex,
		const AActor* owner,
		EDimStringStyle style = EDimStringStyle::DegreeString,
		EEnterableField functionality = EEnterableField::NonEditableText,
		EAutoEditableBox autoTextBox = EAutoEditableBox::UponUserInput,
		const bool alwaysVisible = false,
		const FLinearColor &color = FLinearColor::White);

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static FBoxSphereBounds GetSelectedExtents(const AEditModelPlayerController_CPP *Controller);

	UFUNCTION(BlueprintCallable, Category = "System")
	static void SetWindowTitle(const FString& ProjectName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static void DocAddHideMoiActors(const TArray<AActor*> Actors);

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static void DocUnHideAllMoiActors(const AActor* Owner);

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static FBIMKey GetShopItemFromActor(AActor* TargetActor, bool& bSuccess);

	static bool GetCabinetToeKickDimensions(const FBIMAssemblySpec &obAsm, FVector2D &outToeKickDims);

	// Helper non-BP methods for below BP method
	static bool SetMeshMaterial(UMeshComponent *MeshComponent, const FArchitecturalMaterial &Material, int32 MatIndex, UMaterialInstanceDynamic** CachedMIDPtr = nullptr);
	static bool SetMeshMaterialsFromMapping(UMeshComponent *MeshComponent, const TMap<FName, FArchitecturalMaterial> &MaterialMapping, const TMap<FName, int32> *MatIndexMapping = nullptr);
	static bool SetMeshMaterialsFromAssemblyLayer(UMeshComponent *MeshComponent, const FBIMLayerSpec &AssemblyLayer, const TMap<FName, int32> *MatIndexMapping = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Modumate Color")
	static FColor GetColorFromHex(FString Hex);

	UFUNCTION(BlueprintCallable, Category = "Modumate Color")
	static FString GetHexFromColor(FColor Color);

	UFUNCTION(BlueprintCallable, Category = "Modumate System")
	static void CopyToClipboard(const FString& ClipboardContents);

	UFUNCTION(BlueprintCallable, Category = "Modumate System")
	static void GetClipboardContents(FString& ClipboardContents);

	UFUNCTION(BlueprintPure, Category = "Modumate System")
	static FString GetProjectVersion();

	static void PopulatePatternModuleVariables(TMap<FString, float> &patternExprVars, const FVector &moduleDims, int32 moduleIdx);

	static bool ApplyTileMaterialToMeshFromLayer(UProceduralMeshComponent *MeshComponent, const FBIMLayerSpec &Layer,
		const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial, UMaterialInstanceDynamic** CachedMIDPtr = nullptr);

	static bool UpdateMaterialsFromAssembly(const TArray<UProceduralMeshComponent*> &ProceduralSubLayers, const FBIMAssemblySpec &Assembly,
		const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial,
		TArray<UMaterialInstanceDynamic*> *CachedMIDs = nullptr, bool bLayersReversed = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static bool ClipLine2DToRectangle(const FVector2D& lineStart, const FVector2D& lineEnd, const FBox2D& rectBox, FVector2D& returnLineStart, FVector2D& returnLineEnd, float tolerance = 0.02f);
};
