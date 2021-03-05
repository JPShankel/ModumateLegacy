// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ModumateCore/ModumateTypes.h"

struct FQuantityItemId
{
	FGuid Id;
	FString Subname;
	bool operator==(const FQuantityItemId& Rhs) const
	{
		return Id == Rhs.Id && Subname == Rhs.Subname;
	}
	operator FString() const;
};

struct FQuantityKey
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

struct FQuantity
{
	float Count = 0.0f;
	float Linear = 0.0f;
	float Area = 0.0f;
	float Volume = 0.0f;
	FQuantity& operator+=(const FQuantity& Rhs);
	FQuantity& operator*=(float rhs);
};

class FQuantitiesManager;
class UModumateDocument;
struct FLayerGeomDef;
struct FBIMPresetInstance;
struct FBIMLayerSpec;
struct FBIMPartSlotSpec;
struct FLayerPatternModule;

namespace Modumate
{
	class FGraph3DFace;
}

class MODUMATE_API FQuantitiesVisitor
{
public:
	FQuantitiesVisitor(FQuantitiesManager* Manager);

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

	void AddLayersQuantity(const TArray<FLayerGeomDef>& Layers, const TArray<FBIMLayerSpec>& LayerSpecs, const FGuid& ParentGuid, float Multiplier = 1.0f);
	void AddPartsQuantity(const FString& Name, const TArray<FBIMPartSlotSpec>& Parts, const FGuid& ParentGuid = FGuid());

	float GetModuleUnitsInArea(const FBIMPresetInstance* Preset, const FLayerPatternModule* Module, float Area);
	void GetQuantitiesForModule(const FBIMLayerSpec* LayerSpec, float Area, FQuantity& OutQuantity);

	const QuantitiesMap& GetQuantities() const { return Quantities; }

	static float AreaOfFace(const Modumate::FGraph3DFace& Face);
	static float LengthOfWallFace(const Modumate::FGraph3DFace& Face);
	static float AreaOfLayer(const FLayerGeomDef& LayerGeom);
	static float AreaOfPoly(const TArray<FVector>& Poly);

private:
	FQuantitiesManager* QuantitiesManager;
	QuantitiesMap Quantities;
};
