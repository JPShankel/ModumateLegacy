// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Quantities/QuantitiesCollection.h"

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
#include "Polygon2.h"

uint32 GetTypeHash(const FQuantityKey& Key)
{
	return GetTypeHash(Key.Item) ^ GetTypeHash(Key.Parent);
}

uint32 GetTypeHash(const FQuantityItemId& ItemId)
{
	return GetTypeHash(ItemId.Id) ^ GetTypeHash(ItemId.Subname);
}

void FQuantitiesCollection::AddAreaQuantity(const FGuid& ItemGuid, float Area)
{
	FQuantityKey key = { {ItemGuid} };
	FQuantity value = { 0.0f, 0.0f, Area, 0.0f };
	Quantities.FindOrAdd(key) += value;
}

void FQuantitiesCollection::AddAreaQuantity(const FGuid& ItemGuid, const FGuid& ParentGuid, float Area)
{
	FQuantityKey key = { {ItemGuid}, {ParentGuid} };
	FQuantity value = { 0.0f, 0.0f, Area, 0.0f };
	Quantities.FindOrAdd(key) += value;
}

void FQuantitiesCollection::AddLayersQuantity(const TArray<FLayerGeomDef>& Layers, const TArray<FBIMLayerSpec>& LayerSpecs, const FGuid& ParentGuid, float Multiplier /*= 1.0f*/)
{
	const int32 numLayers = FMath::Min(Layers.Num(), LayerSpecs.Num());
	for (int32 layer = 0; layer < numLayers; ++layer)
	{
		const FBIMLayerSpec& layerSpec = LayerSpecs[layer];
		float layerArea = AreaOfLayer(Layers[layer]);
		auto& layerGuid = layerSpec.PresetGUID;
		AddAreaQuantity(layerGuid, ParentGuid, layerArea * Multiplier);
		for (const auto& module : layerSpec.Modules)
		{   // Guard against simple layers that just contain themselves as a module.
			if (module.Key.IsValid() && module.Key != layerGuid)
			{
				FQuantity quantity;
				GetQuantitiesForModule(&layerSpec, layerArea, quantity);
				quantity *= Multiplier;
				AddQuantity(module.Key, layerGuid, quantity);
			}
		}
	}
}

void FQuantitiesCollection::AddPartsQuantity(const FString& Name, const TArray<FBIMPartSlotSpec>& Parts, const FGuid& ParentGuid /*= FGuid()*/)
{
	for (const auto& part : Parts)
	{
		EPresetMeasurementMethod  method = part.MeasurementMethod;
		FString partName = method == EPresetMeasurementMethod::None || method == EPresetMeasurementMethod::PartBySizeGroup
			? Name : FString();
		if (part.ParentSlotIndex >= 0)
		{
			const auto& parentPart = Parts[part.ParentSlotIndex];
			EPresetMeasurementMethod parentmethod = parentPart.MeasurementMethod;
			FString parentPartName = parentmethod == EPresetMeasurementMethod::None || parentmethod == EPresetMeasurementMethod::PartBySizeGroup
				? Name : FString();
			AddQuantity(part.PresetGUID, partName, parentPart.PresetGUID, parentPartName, 1.0f);
		}
		else
		{
			AddQuantity(part.PresetGUID, partName, ParentGuid, FString(), 1.0f);
		}
	}
}

void FQuantitiesCollection::GetQuantitiesForModule(const FBIMLayerSpec* LayerSpec, float Area, FQuantity& OutQuantity)
{
	const FLayerPattern& pattern = LayerSpec->Pattern;
	if (pattern.ModuleCount > 0)
	{
		if (pattern.ParameterizedModuleDimensions.Num() > 0)
		{
			float unitArea = pattern.CachedExtents.X * pattern.CachedExtents.Y / pattern.ParameterizedModuleDimensions.Num();
			OutQuantity.Count += Area / unitArea;
		}
		else
		{
			float period = pattern.CachedExtents.X * UModumateDimensionStatics::InchesToCentimeters;
			OutQuantity.Linear += Area / period;
		}
	}
}

FQuantitiesCollection& FQuantitiesCollection::Add(const FQuantitiesCollection& Other)
{
	for (const auto& q: Other.Quantities)
	{
		Quantities.FindOrAdd(q.Key) += q.Value;
	}
	return *this;
}

void FQuantitiesCollection::AddQuantity(const FGuid& ItemGuid, float Count, float Linear /*= 0.0f*/, float Area /*= 0.0f*/, float Volume /*= 0.0f*/)
{
	FQuantityKey key = { {ItemGuid} };
	FQuantity value = { Count, Linear, Area, Volume };
	Quantities.FindOrAdd(key) += value;
}

void FQuantitiesCollection::AddQuantity(const FGuid& ItemGuid, const FGuid& ParentGuid, float Count, float Linear /*= 0.0f*/, float Area /*= 0.0f*/, float Volume /*= 0.0f*/)
{
	FQuantityKey key = { {ItemGuid}, {ParentGuid} };
	FQuantity value = { Count, Linear, Area, Volume };
	Quantities.FindOrAdd(key) += value;
}

void FQuantitiesCollection::AddQuantity(const FGuid& ItemGuid, const FGuid& ParentGuid, const FQuantity& Quantity)
{
	FQuantityKey key = { {ItemGuid}, {ParentGuid} };
	Quantities.FindOrAdd(key) += Quantity;
}

void FQuantitiesCollection::AddQuantity(const FGuid& ItemGuid, const FString& ItemSubname, const FGuid& ParentGuid,
	const FString& ParentSubname, float Count, float Linear, float Area, float Volume)
{
	FQuantityKey key = { {ItemGuid, ItemSubname}, {ParentGuid, ParentSubname} };
	FQuantity value = { Count, Linear, Area, Volume };
	Quantities.FindOrAdd(key) += value;
}

float FQuantitiesCollection::LengthOfWallFace(const FGraph3DFace& Face)
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

float FQuantitiesCollection::AreaOfFace(const FGraph2DPolygon& Face)
{
	return AreaOfPoly(Face.CachedPerimeterPoints);
}

float FQuantitiesCollection::AreaOfLayer(const FLayerGeomDef& LayerGeom)
{
	float area = 0.0f;
	area += AreaOfPoly(LayerGeom.UniquePointsA);
	for (const auto& hole : LayerGeom.CachedHoles2D)
	{
		area -= AreaOfPoly(hole.Points);
	}

	return FMath::Max(0.0f, area);
}

// Basically a corrected version of FPoly::Area() for concave shapes.
float FQuantitiesCollection::AreaOfPoly(const TArray<FVector>& Poly)
{
	if (Poly.Num() < 3)
	{
		return 0.0f;
	}

	FVector areaVec(0.0f);
	FVector side1(Poly[1] - Poly[0]);
	for (int32 v = 2; v < Poly.Num(); ++v)
	{
		FVector side2(Poly[v] - Poly[0]);
		areaVec += side1 ^ side2;
		side1 = side2;
	}

	return areaVec.Size() * 0.5f;
}

float FQuantitiesCollection::AreaOfPoly(const TArray<FVector2D>& Poly)
{
	FPolygon2f polygon;
	for (const auto& vert : Poly)
	{
		polygon.AppendVertex(vert);
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
