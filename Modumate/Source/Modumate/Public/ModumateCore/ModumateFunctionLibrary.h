// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/Box.h"
#include "ModumateCore/ModumateTypes.h"
#include "ModumateCore/ModumateDimensionString.h"
#include "UObject/Object.h"

#include "ModumateFunctionLibrary.generated.h"


/**
*
*/

class ADynamicMeshActor;
class UModumateCraftingWidget_CPP;

UCLASS()
class MODUMATE_API UModumateFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Convert Vector3D to 2D.
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static FVector V3dToV2d(TArray<FVector2D> & OutV2d, TArray<FVector> InV3d);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static bool IsWithEditor();

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static bool PointIsOnSegment(const FVector2D &p, const FVector2D &s1, const FVector2D &s2, float tolerance = 0.1f);

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static bool LinesIntersect2D(const FVector2D &p1, const FVector2D &p2, const FVector2D &p3, const FVector2D &p4, FVector2D &intersection);

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static FVector2D ProjectPointToLine2D(const FVector2D &lineP1, const FVector2D &lineP2, const FVector2D &inPoint);

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static FVector ProjectPointToLine(const FVector &lineP1, const FVector &lineP2, const FVector &inPoint);

	// Generate UVs from array of FVector2D converted from FVector3D
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static TArray<FVector2D> GetPolygonUV(TArray<FVector2D> Verts, FVector2D Size);

	// Generate UV based on XY coord only, ignoring Z. Useful for floor
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static TArray<FVector2D> GetPolygonUVWith3DXY(FVector Origin, TArray<FVector> Verts, FVector2D Size, FRotator UvRotation);

	// Displace int array to the highest number of the array
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static TArray<int32> IntegerArrayReplaceHighest(TArray<int32> ExistingInt, TArray<int32> NewInt);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static FVector GetClosestLocationToTargetFromArray(FVector Target, TArray<FVector>Array, int32& ClosestIndex, int32& FarestIndex);

	// Get the next index in the array assuming this is in a loop
	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static int32 LoopArrayGetNextIndex(int32 CurrentIndex, int32 LengthOfArray);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static int32 LoopArrayGetPreviousIndex(int32 CurrentIndex, int32 LengthOfArray);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static bool IsVectorInArray(const TArray<FVector>& Array, const FVector& TargetVector, float Tolerance);

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static bool PointInPoly2D(const FVector2D &p, const TArray<FVector2D> &poly);

	// Make sure triangles are clockwise
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static TArray<int32> ConformTriangleClockwise(TArray<FVector> Vertices, TArray<int32> Triangles);

	// Debug Version: Calculate UV with UV origin at designated local (relative to mesh) anchor.
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static TArray<FVector2D> FloorUVCalculateWithAnchor(TArray<FVector> faceVertsRelative, FVector anchorRelative, FVector faceDirection, FVector horizontalDirection);

	UFUNCTION(BlueprintCallable, Category = "Modumate Assembly")
	static void CalculateFloorParam(FVector Origin, TArray<FVector> anchorUV, FVector topAnchorUV, FRotator topUvRotation, TArray<FVector> InTopVerts, TArray<FVector> InBottomVerts, TArray<FVector>& ReturnVerts, TArray<int32>& ReturnTris, TArray<FVector>& ReturnNormals, TArray<FVector2D>& ReturnUV, TArray<int32>& ReturnTopTris);

	//UFUNCTION(BlueprintCallable, Category = "Modumate Assembly")
	static void GetFloorAssemblyLayerControlPoints(const TArray<FVector>& points, const TArray<FModumateObjectAssemblyLayer>& fal, TArray<FFloorAssemblyLayerControlPoints> &OutLayersCPs, bool bLayersReversed = true, bool bManualLayerCPs = false);

	// Given a component reference (size radius should be 100.0), calculate how much new scale needs to be in order to maintain constant screen size
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static FVector GetWorldComponentToScreenSizeScale(UStaticMeshComponent* Target, FVector DesiredScreenSize);

	// Change component transform as if it is like a material billboard
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static void ComponentAsBillboard(UStaticMeshComponent* Target, FVector DesiredScreenSize);

	// Change component location to as close to camera as possible, acting like an UI element
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static void ComponentToUIScreenPosition(UStaticMeshComponent* Target, FVector WorldLocation, FVector ScreenOffset = FVector::ZeroVector);

	// *(Use with MOI only)Change component location to as close to camera as possible, acting like an UI element
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static void ComponentToUIScreenPositionMoiOffset(UStaticMeshComponent* Target, FVector WorldLocation, FVector Direction, float OffsetScreenDistance, FVector ScreenOffset = FVector::ZeroVector);

	// Adjust the direction of component that uses function ComponentAsBillboard
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static void ScreenSizeComponentFreezeDirection(UStaticMeshComponent* Target, FVector WorldDirection);

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static bool MoiPortalSetNewWidth(AActor* targetActor, float newWidth);

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static bool MoiPortalSetNewHeight(AActor* targetActor, float newHeight);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static bool IsPointInTri(FVector pt, FVector v1, FVector v2, FVector v3);

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

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static FModumateSunPositionData ModumateGetSunPosition(float Latitude, float Longitude, float TimeZone, bool bIsDaylightSavingTime, int32 Year, int32 Month, int32 Day, int32 Hours, int32 Minutes, int32 Seconds);

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static void SetSkylightCubemapAngle(USkyLightComponent* SkyLight, float Angle);

	UFUNCTION(BlueprintPure, Category = "Modumate Vector")
	static float GetViewportDPIScale();

	// Call when wall is updating to update anchor of UV in world space
	UFUNCTION(BlueprintCallable, Category = "Modumate Wall")
	static FVector WallUpdateUVAnchor(FVector currentAnchorOrigin, int32 anchorSide, FVector newP1, FVector newP2, FVector oldP1, FVector oldP2);

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static EObjectType GetMOITypeFromActor(AActor* MOIActor);

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
	static FShoppingItem GetShopItemFromActor(AActor* TargetActor, bool& bSuccess);

	static bool GetCabinetToeKickDimensions(const FModumateObjectAssembly &obAsm, FVector2D &outToeKickDims);

	// Helper non-BP methods for below BP method
	static bool SetMeshMaterial(UMeshComponent *MeshComponent, const FArchitecturalMaterial &Material, int32 MatIndex, UMaterialInstanceDynamic** CachedMIDPtr = nullptr);
	static bool SetMeshMaterialsFromMapping(UMeshComponent *MeshComponent, const TMap<FName, FArchitecturalMaterial> &MaterialMapping, const TMap<FName, int32> *MatIndexMapping = nullptr);
	static bool SetMeshMaterialsFromAssemblyLayer(UMeshComponent *MeshComponent, const FModumateObjectAssemblyLayer &AssemblyLayer, const TMap<FName, int32> *MatIndexMapping = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Modumate Color")
	static FColor GetColorFromHex(FString Hex);

	UFUNCTION(BlueprintCallable, Category = "Modumate Color")
	static FString GetHexFromColor(FColor Color);

	UFUNCTION(BlueprintCallable, Category = "Modumate Color")
	static FLinearColor ModumateMakeFromHSV(uint8 H, uint8 S, uint8 V);

	UFUNCTION(BlueprintCallable, Category = "Modumate Debug")
	static void ModumateDebugDrawSphere(AActor* Outer, FVector Location, float Radius, float DrawTime, FLinearColor DrawColor);

	UFUNCTION(BlueprintPure, Category = "Modumate System")
	static FString GetEnumString(UEnum* EnumClass, uint8 EnumValue);

	UFUNCTION(BlueprintCallable, Category = "Modumate System")
	static void CopyToClipboard(const FString& ClipboardContents);

	UFUNCTION(BlueprintCallable, Category = "Modumate System")
	static void GetClipboardContents(FString& ClipboardContents);

	UFUNCTION(BlueprintPure, Category = "Modumate System")
	static FString GetProjectVersion();

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static void DocTransverseObject(AActor* MoiActors);

	UFUNCTION(BlueprintCallable, Category = "Modumate Document")
	static bool ApplyTileMaterialToMeshes(const TArray<UProceduralMeshComponent*> &ProceduralSubLayers, const FName &AssemblyKey, AEditModelPlayerController_CPP *Controller, EToolMode FromToolMode, const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial, bool AsLayer, FString KeyOverride, bool bUseMarketplaceAsm = false);

	static void PopulatePatternModuleVariables(TMap<FString, float> &patternExprVars, const FVector &moduleDims, int32 moduleIdx);

	static bool ApplyTileMaterialToMeshFromLayer(UProceduralMeshComponent *MeshComponent, const FModumateObjectAssemblyLayer &Layer,
		const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial, UMaterialInstanceDynamic** CachedMIDPtr = nullptr);

	static bool UpdateMaterialsFromAssembly(const TArray<UProceduralMeshComponent*> &ProceduralSubLayers, const FModumateObjectAssembly &Assembly,
		const TArray<UMaterialInterface*> &TilingMaterials, UMaterialInterface *MasterPBRMaterial,
		TArray<UMaterialInstanceDynamic*> *CachedMIDs = nullptr, bool bLayersReversed = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static bool IsTargetCloseToControlPointEdges(const FVector& target, const TArray<FVector>& controlPoints, const FTransform& parentTransform, float tolerance);

	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static bool ClipLine2DToRectangle(const FVector2D& lineStart, const FVector2D& lineEnd, const FBox2D& rectBox, FVector2D& returnLineStart, FVector2D& returnLineEnd, float tolerance = 0.02f);

	// Get the extent of a component without world rotation and scale to 1
	UFUNCTION(BlueprintCallable, Category = "Modumate Vector")
	static FVector GetComponentExtentLocalSpace(const USceneComponent* Comp);

	// Text statics - if we get 3 functions here, break out to their own module
	static FString GetNextStringInNumericalSequence(const FString &currentString, const TCHAR firstDigitChar, const TCHAR lastDigitChar);
};
