// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/Box.h"

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
	static bool ComponentAsBillboard(UStaticMeshComponent* Target, FVector DesiredScreenSize);

	// Change component location to as close to camera as possible, acting like an UI element
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static bool ComponentToUIScreenPosition(UStaticMeshComponent* Target, FVector WorldLocation, FVector ScreenOffset = FVector::ZeroVector);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static bool LineBoxIntersection(const FBox2D &box, const FVector2D start, const FVector2D end);

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
	static FBoxSphereBounds GetSelectedExtents(const AEditModelPlayerController *Controller);

	UFUNCTION(BlueprintCallable, Category = "System")
	static void SetWindowTitle(const FString& ProjectName = TEXT(""), const FText& ProjectSuffix = FText::GetEmpty());

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static void SetMOIAndDescendentsHidden(const TArray<AModumateObjectInstance*>& MOIs, bool bHide = true);

	// Helper non-BP methods for below BP method
	static bool SetMeshMaterial(UMeshComponent *MeshComponent, const FArchitecturalMaterial &Material, int32 MatIndex, UMaterialInstanceDynamic** CachedMIDPtr = nullptr);
	static bool SetMeshMaterialsFromMapping(UMeshComponent *MeshComponent, const TMap<FName, FArchitecturalMaterial> &MaterialMapping, const TMap<FName, int32> *MatIndexMapping = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Modumate System")
	static void CopyToClipboard(const FString& ClipboardContents);

	UFUNCTION(BlueprintCallable, Category = "Modumate System")
	static void GetClipboardContents(FString& ClipboardContents);

	UFUNCTION(BlueprintPure, Category = "Modumate System")
	static FString GetProjectVersion();

	static void EvaluateParameterizedPatternExtents(FBIMLayerSpec& Layer);

	static bool ApplyTileMaterialToMeshFromLayer(UProceduralMeshComponent *MeshComponent, const FBIMLayerSpec &Layer,
		const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial, UMaterialInstanceDynamic** CachedMIDPtr = nullptr);

	static bool UpdateMaterialsFromAssembly(const TArray<UProceduralMeshComponent*> &ProceduralSubLayers, const FBIMAssemblySpec &Assembly,
		const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial,
		TArray<UMaterialInstanceDynamic*> *CachedMIDs = nullptr, bool bLayersReversed = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static bool ClipLine2DToRectangle(const FVector2D& lineStart, const FVector2D& lineEnd, const FBox2D& rectBox, FVector2D& returnLineStart, FVector2D& returnLineEnd, float tolerance = 0.02f);

	static bool FindViewports(UObject* WorldContextObject, class FSceneViewport*& OutSceneViewport, class UGameViewportClient*& OutGameViewport);
	static bool ModifyViewportWindow(class FSceneViewport* SceneViewport, int32 WindowWidth = 0, int32 WindowHeight = 0, bool bBringToFront = false);
};
