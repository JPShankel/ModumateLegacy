// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"

struct FBIMConstructionCost;

struct MODUMATE_API FQuantityItemId
{
	FGuid Id;
	FString Subname;
	bool operator==(const FQuantityItemId& Rhs) const
	{
		return Id == Rhs.Id && Subname == Rhs.Subname;
	}
	operator FString() const;
};

struct MODUMATE_API FQuantityKey
{
	FQuantityItemId Item;
	FQuantityItemId Parent;
	bool operator==(const FQuantityKey& Rhs) const
	{
		return Item == Rhs.Item && Parent == Rhs.Parent;
	}
};

uint32 GetTypeHash(const FQuantityItemId& Key);
uint32 GetTypeHash(const FQuantityKey& Key);

struct MODUMATE_API FQuantity
{
	float Count = 0.0f;
	float Linear = 0.0f;
	float Area = 0.0f;
	float Volume = 0.0f;
	float MaterialCost = 0.0f;
	float LaborCost = 0.0f;
	FQuantity& operator+=(const FQuantity& Rhs);
	FQuantity& operator*=(float rhs);
	void CalculateCosts(const FBIMConstructionCost& CostRate);
	FWebQuantity toWebQuantity() const;
};

class FQuantitiesManager;
class UModumateDocument;
struct FLayerGeomDef;
struct FBIMPresetInstance;
struct FBIMLayerSpec;
struct FBIMPartSlotSpec;
struct FLayerPatternModule;
struct FPolyHole3D;

class FGraph3DFace;
class FGraph2DPolygon;

class MODUMATE_API FQuantitiesCollection
{
public:
	FQuantitiesCollection() { }

	using QuantitiesMap = TMap<FQuantityKey, FQuantity>;

	void AddQuantity(const FGuid& ItemGuid, float Count, float Linear = 0.0f, float Area = 0.0f, float Volume = 0.0f);
	void AddQuantity(const FGuid& ItemGuid, const FGuid& ParentGuid, float Count, float Linear = 0.0f, float Area = 0.0f,
		float Volume = 0.0f);
	void AddQuantity(const FGuid& ItemGuid, const FString& ItemSubname, const FGuid& ParentGuid,
		const FString& ParentSubname, float Count, float Linear = 0.0f, float Area = 0.0f,
		float Volume = 0.0f);
	void AddQuantity(const FGuid& ItemGuid, const FGuid& ParentGuid, const FQuantity& Quantity);
	void AddAreaQuantity(const FGuid& ItemGuid, float Area);
	void AddAreaQuantity(const FGuid& ItemGuid, const FGuid& ParentGuid, float Area);

	void AddLayersQuantity(const TArray<FLayerGeomDef>& Layers, const TArray<FBIMLayerSpec>& LayerSpecs,
		const FGuid& ParentGuid, float Multiplier = 1.0f);
	void AddPartsQuantity(const FString& Name, const TArray<FBIMPartSlotSpec>& Parts, const FGuid& ParentGuid = FGuid());

	void GetQuantitiesForModule(const FBIMLayerSpec* LayerSpec, float Area, FQuantity& OutQuantity);
	void Empty() { Quantities.Reset(); }
	FQuantitiesCollection& Add(const FQuantitiesCollection& Other);

	const QuantitiesMap& GetQuantities() const { return Quantities; }
	int32 Num() const { return Quantities.Num(); }

	static float AreaOfFace(const FGraph2DPolygon& Face);
	static float LengthOfWallFace(const FGraph3DFace& Face);
	static float AreaOfLayer(const FLayerGeomDef& LayerGeom);
	static float AreaOfPoly(const TArray<FVector>& Poly);
	static float AreaOfPoly(const TArray<FVector2D>& Poly);

private:
	QuantitiesMap Quantities;
};

using FQuantitiesMap = FQuantitiesCollection::QuantitiesMap;
