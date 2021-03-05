// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Quantities/QuantitiesVisitor.h"

#include "Quantities/QuantitiesManager.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "BIMKernel/AssemblySpec/BIMLayerSpec.h"
#include "BIMKernel/AssemblySpec/BIMPartSlotSpec.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Math/NumericLimits.h"
#include "Engine/Polys.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/LayerGeomDef.h"
#include "Graph/Graph3DFace.h"

uint32 GetTypeHash(const FQuantityKey& Key)
{
	return GetTypeHash(Key.Item) ^ GetTypeHash(Key.Parent);
}

uint32 GetTypeHash(const FQuantityItemId& ItemId)
{
	return GetTypeHash(ItemId.Id) ^ GetTypeHash(ItemId.Subname);
}

FQuantitiesVisitor::FQuantitiesVisitor(FQuantitiesManager* Manager)
	: QuantitiesManager(Manager)
{ }

void FQuantitiesVisitor::AddAreaQuantity(const FGuid& ItemGuid, float Area)
{
	FQuantityKey key = { {ItemGuid} };
	FQuantity value = { 0.0f, 0.0f, Area, 0.0f };
	Quantities.FindOrAdd(key) += value;
}

void FQuantitiesVisitor::AddAreaQuantity(const FGuid& ItemGuid, const FGuid& ParentGuid, float Area)
{
	FQuantityKey key = { {ItemGuid}, {ParentGuid} };
	FQuantity value = { 0.0f, 0.0f, Area, 0.0f };
	Quantities.FindOrAdd(key) += value;
}

void FQuantitiesVisitor::AddLayersQuantity(const TArray<FLayerGeomDef>& Layers, const TArray<FBIMLayerSpec>& LayerSpecs, const FGuid& ParentGuid, float Multiplier /*= 1.0f*/)
{
	const int32 numLayers = Layers.Num();
	for (int32 layer = 0; layer < numLayers; ++layer)
	{
		const FBIMLayerSpec& layerSpec = LayerSpecs[layer];
		float layerArea = AreaOfLayer(Layers[layer]);
		auto& layerGuid = layerSpec.PresetGUID;
		AddAreaQuantity(layerGuid, ParentGuid, layerArea * Multiplier);
		for (const auto& module : layerSpec.Modules)
		{
			if (module.Key.IsValid())
			{
				FQuantity quantity;
				GetQuantitiesForModule(&layerSpec, layerArea, quantity);
				quantity *= Multiplier;
				AddQuantity(module.Key, layerGuid, quantity);
			}
		}
	}
}

void FQuantitiesVisitor::AddPartsQuantity(const FString& Name, const TArray<FBIMPartSlotSpec>& Parts, const FGuid& ParentGuid /*= FGuid()*/)
{
	for (const auto& part : Parts)
	{
		if (part.ParentSlotIndex >= 0)
		{
			AddQuantity(part.PresetGUID, Name, Parts[part.ParentSlotIndex].PresetGUID, Name, 1.0f);

		}
		else
		{
			AddQuantity(part.PresetGUID, Name, ParentGuid, FString(), 1.0f);
		}
	}
}

float FQuantitiesVisitor::GetModuleUnitsInArea(const FBIMPresetInstance* Preset, const FLayerPatternModule* Module, float Area)
{
	return QuantitiesManager->GetModuleUnitsInArea(Preset, Module, Area);
}

void FQuantitiesVisitor::GetQuantitiesForModule(const FBIMLayerSpec* LayerSpec, float Area, FQuantity& OutQuantity)
{
	static const FName nameDepth{ TEXT("Depth") };
	static const FName nameWidth{ TEXT("Width") };
	const FLayerPattern& pattern = LayerSpec->Pattern;
	if (pattern.ModuleCount > 0)
	{
		if (pattern.ThicknessDimensionPropertyName == nameWidth)
		{
			float unitArea = pattern.Extents.X * pattern.Extents.Y / pattern.ParameterizedModuleDimensions.Num();
			OutQuantity.Count += Area / unitArea;
		}
		else if (pattern.ThicknessDimensionPropertyName == nameDepth)
		{	// 1D layer; map pattern name for now.
			// This should cover all current default presets.
			static const TMap<FString, float> nameToPeriodMap =
			{ 
				{TEXT("12-Inch O.C."), 12 * UModumateDimensionStatics::InchesToCentimeters},
				{TEXT("16-Inch O.C."), 16 * UModumateDimensionStatics::InchesToCentimeters},
				{TEXT("19.2-Inch O.C."), 19.2f * UModumateDimensionStatics::InchesToCentimeters},
				{TEXT("24-Inch O.C."), 24 * UModumateDimensionStatics::InchesToCentimeters}
			};
			const float* period = nameToPeriodMap.Find(pattern.DisplayName.ToString());
			if (period)
			{
				OutQuantity.Linear += Area / *period;
			}
		}
	}
}

void FQuantitiesVisitor::AddQuantity(const FGuid& ItemGuid, float Count, float Linear /*= 0.0f*/, float Area /*= 0.0f*/, float Volume /*= 0.0f*/)
{
	FQuantityKey key = { {ItemGuid} };
	FQuantity value = { Count, Linear, Area, Volume };
	Quantities.FindOrAdd(key) += value;
}

void FQuantitiesVisitor::AddQuantity(const FGuid& ItemGuid, const FGuid& ParentGuid, float Count, float Linear /*= 0.0f*/, float Area /*= 0.0f*/, float Volume /*= 0.0f*/)
{
	FQuantityKey key = { {ItemGuid}, {ParentGuid} };
	FQuantity value = { Count, Linear, Area, Volume };
	Quantities.FindOrAdd(key) += value;
}

void FQuantitiesVisitor::AddQuantity(const FGuid& ItemGuid, const FGuid& ParentGuid, const FQuantity& Quantity)
{
	FQuantityKey key = { {ItemGuid}, {ParentGuid} };
	Quantities.FindOrAdd(key) += Quantity;
}

void FQuantitiesVisitor::AddQuantity(const FGuid& ItemGuid, const FString& ItemSubname, const FGuid& ParentGuid,
	const FString& ParentSubname, float Count, float Linear, float Area, float Volume)
{
	FQuantityKey key = { {ItemGuid, ItemSubname}, {ParentGuid, ParentSubname} };
	FQuantity value = { Count, Linear, Area, Volume };
	Quantities.FindOrAdd(key) += value;
}

float FQuantitiesVisitor::AreaOfFace(const Modumate::FGraph3DFace& Face)
{
	FPoly poly;
	int32 i = 0;
	for (const auto& vert: Face.CachedPositions)
	{
		poly.InsertVertex(i++, vert);
	}
	return poly.Area();
}

float FQuantitiesVisitor::LengthOfWallFace(const Modumate::FGraph3DFace& Face)
{
	FVector2D normal(Face.CachedPlane.Y, -Face.CachedPlane.X);
	normal.Normalize();
	float minProj = TNumericLimits<float>::Max();
	float maxProj = TNumericLimits<float>::Min();
	for (const auto& vert : Face.CachedPositions)
	{
		float proj = normal | FVector2D(vert);
		minProj = FMath::Min(proj, minProj);
		maxProj = FMath::Max(proj, minProj);
	}
	return maxProj - minProj;
}

float FQuantitiesVisitor::AreaOfLayer(const FLayerGeomDef& LayerGeom)
{
	return AreaOfPoly(LayerGeom.UniquePointsA);
}

float FQuantitiesVisitor::AreaOfPoly(const TArray<FVector>& Poly)
{
	FPoly polygon;
	int32 i = 0;
	for (const auto& vert : Poly)
	{
		polygon.InsertVertex(i++, vert);
	}
	return polygon.Area();
}

FQuantity& FQuantity::operator+=(const FQuantity& Rhs)
{
	Count += Rhs.Count;
	Linear += Rhs.Linear;
	Area += Rhs.Area;
	Volume += Rhs.Volume;
	return *this;
}

FQuantity& FQuantity::operator*=(float rhs)
{
	Count *= rhs;
	Linear *= rhs;
	Area *= rhs;
	Volume *= rhs;
	return *this;
}

FQuantityItemId::operator FString() const
{
	return Subname.IsEmpty() ? FString() : FString(TEXT(" (")) + Subname + TEXT(")");
}
