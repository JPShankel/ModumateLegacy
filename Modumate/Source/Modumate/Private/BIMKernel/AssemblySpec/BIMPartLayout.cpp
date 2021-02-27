// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMPartLayout.h"
#include "BIMKernel/AssemblySpec/BIMPartSlotSpec.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Algo/Transform.h"

// Common named parameters
const FString FBIMPartLayout::ScaledSizeX = TEXT("ScaledSizeX");
const FString FBIMPartLayout::ScaledSizeY = TEXT("ScaledSizeY");
const FString FBIMPartLayout::ScaledSizeZ = TEXT("ScaledSizeZ");

const FString FBIMPartLayout::NativeSizeX = TEXT("NativeSizeX");
const FString FBIMPartLayout::NativeSizeY = TEXT("NativeSizeY");
const FString FBIMPartLayout::NativeSizeZ = TEXT("NativeSizeZ");

const FString FBIMPartLayout::LocationX = TEXT("LocationX");
const FString FBIMPartLayout::LocationY = TEXT("LocationY");
const FString FBIMPartLayout::LocationZ = TEXT("LocationZ");

const FString FBIMPartLayout::RotationX = TEXT("RotationX");
const FString FBIMPartLayout::RotationY = TEXT("RotationY");
const FString FBIMPartLayout::RotationZ = TEXT("RotationZ");

const FString FBIMPartLayout::Self = TEXT("Self");
const FString FBIMPartLayout::Parent = TEXT("Parent");

// Given a starting part and a fully qualified path to a value, navigate to the source and retrieve the value
// TODO: cache values and add to a global scope map 
bool FBIMPartLayout::TryGetValueForPart(const FBIMAssemblySpec& InAssemblySpec, int32 InPartIndex, const FString& InVar, float& OutVal, TArray<FString>& OutErrors)
{
	// An input var will be a fully qualified value like "Parent.Panel.LocationX"
	// For a qualified var with N elements, the first N-1 describe slot navigation and the final value is a variable on that slot
	TArray<FString> scopes;
	InVar.ParseIntoArray(scopes, TEXT("."));

	// Starting on our input slot, use the scope values to navigate to our destination.
	int32 currentSlot = InPartIndex;
	for (int32 i = 0; i < scopes.Num() - 1; ++i)
	{
		// "Self" is a no-op, just keep parsing
		if (scopes[i].Equals(Self))
		{
			continue;
		}
		// If we find "Parent," then navigate up one slot. The first part is a container with no parent so it should not refer to a parent
		else if (scopes[i].Equals(Parent) && ensureAlways(InAssemblySpec.Parts[currentSlot].ParentSlotIndex != -1))
		{
			currentSlot = InAssemblySpec.Parts[currentSlot].ParentSlotIndex;
		}
		else
		{
			// If it's not "Parent" or "Self" then it's a tag path for one of this slot's children and we want to navigate down
			// Create a tag path and look for any children of this slot with the same tag path (highlander rules: there can be only one)
			FString slotID(scopes[i]);
			bool found = false;
			for (int32 j = 0; j < PartSlotInstances.Num(); ++j)
			{
				if (InAssemblySpec.Parts[j].ParentSlotIndex == currentSlot && InAssemblySpec.Parts[j].SlotID.Equals(slotID))
				{
					currentSlot = j;
					found = true;
					break;
				}
			}
#if WITH_EDITOR
			if (!found)
			{
				FString debugSlotID = InAssemblySpec.Parts[currentSlot].SlotID;
				if (debugSlotID.IsEmpty())
				{
					debugSlotID = TEXT("ROOT");
				}

				FString coreError = TEXT("No such slot for ");

				for (int32 j = 0; j < PartSlotInstances.Num(); ++j)
				{
					if (InAssemblySpec.Parts[j].SlotID.Equals(slotID))
					{
						coreError = TEXT("Bad path for ");
						break;
					}
				}

				OutErrors.Add(FString::Printf(TEXT("%s BIM Scope %s in variable %s for slot %s in assembly %s"), *coreError,*scopes[i], *InVar, *debugSlotID, *InAssemblySpec.DisplayName));
			}
#endif
		}
	}

	// After we've gotten to the destination slot, look for our unqualified value (ie "SizeX") or return 0
	const float* returnVal = PartSlotInstances[currentSlot].VariableValues.Find(scopes.Last());

	PartSlotInstances[currentSlot].VisibleNamedDimensions.AddUnique(scopes.Last());
	if (returnVal == nullptr)
	{
		FModumateUnitValue unitVal;
		if (ensureAlwaysMsgf(FBIMPartSlotSpec::TryGetDefaultNamedDimension(scopes.Last(), unitVal), TEXT("COULD NOT FIND BIM VALUE %s"), *InVar))
		{
			OutVal = unitVal.AsWorldCentimeters();
			return true;
		}
		OutVal = 0.0f;
		return false;
	}
	OutVal = *returnVal;
	return true;
}

// Build a layout for a given rigged assembly
EBIMResult FBIMPartLayout::FromAssembly(const FBIMAssemblySpec& InAssemblySpec, const FVector& InScale)
{
	int32 numSlots = InAssemblySpec.Parts.Num();
	PartSlotInstances.SetNum(numSlots);

	if (!ensureAlways(numSlots != 0))
	{
		return EBIMResult::Error;
	}

	// The first part is created as a parent to the others with no bespoke sizing operation
	// Children will depend on its scaled size (native*scale)
	// This is the one and only input of InScale to the derived placement math

	for (auto& kvp : InAssemblySpec.Parts[0].NamedDimensionValues)
	{
		PartSlotInstances[0].VariableValues.Add(kvp.Key, kvp.Value.AsWorldCentimeters());
	}

	FVector assemblyNativeSize = InAssemblySpec.GetRiggedAssemblyNativeSize();

	PartSlotInstances[0].VariableValues.Add(NativeSizeX, assemblyNativeSize.X);
	PartSlotInstances[0].VariableValues.Add(NativeSizeY, assemblyNativeSize.Y);
	PartSlotInstances[0].VariableValues.Add(NativeSizeZ, assemblyNativeSize.Z);

	assemblyNativeSize *= InScale;
	PartSlotInstances[0].VariableValues.Add(ScaledSizeX, assemblyNativeSize.X);
	PartSlotInstances[0].VariableValues.Add(ScaledSizeY, assemblyNativeSize.Y);
	PartSlotInstances[0].VariableValues.Add(ScaledSizeZ, assemblyNativeSize.Z);

	// First pass, acquire all the a-priori information for these parts, including their meshes and their initial size values
	for (int32 slotIdx = 1; slotIdx < numSlots; ++slotIdx)
	{
		const FBIMPartSlotSpec& assemblyPart = InAssemblySpec.Parts[slotIdx];

		auto* useMesh = &assemblyPart.Mesh;

		if (!useMesh->EngineMesh.IsValid())
		{
			for (int32 childIdx = slotIdx + 1; childIdx < numSlots; ++childIdx)
			{
				if (InAssemblySpec.Parts[childIdx].ParentSlotIndex == slotIdx && InAssemblySpec.Parts[childIdx].Mesh.EngineMesh.IsValid())
				{
					useMesh = &InAssemblySpec.Parts[childIdx].Mesh;
					break;
				}
			}
		}

		for (auto& kvp : InAssemblySpec.Parts[slotIdx].NamedDimensionValues)
		{
			PartSlotInstances[slotIdx].VariableValues.Add(kvp.Key, kvp.Value.AsWorldCentimeters());
		}

		// The initial size value for parts with meshes is the mesh's native size
		// This value is recalculated down below but may depend on this initial value
		if (useMesh->EngineMesh.IsValid())
		{
			FVector nativeSize = useMesh->NativeSize * UModumateDimensionStatics::InchesToCentimeters;
			PartSlotInstances[slotIdx].VariableValues.Add(NativeSizeX, nativeSize.X);
			PartSlotInstances[slotIdx].VariableValues.Add(NativeSizeY, nativeSize.Y);
			PartSlotInstances[slotIdx].VariableValues.Add(NativeSizeZ, nativeSize.Z);
		}
		else
		{
			PartSlotInstances[slotIdx].VariableValues.Add(NativeSizeX, 0.0f);
			PartSlotInstances[slotIdx].VariableValues.Add(NativeSizeY, 0.0f);
			PartSlotInstances[slotIdx].VariableValues.Add(NativeSizeZ, 0.0f);
		}
	}

	TArray<FString> formulaErrors;

	if (!InAssemblySpec.SlotConfigConceptualSizeY.IsEmpty())
	{
		TArray<FString> varNames;
		Modumate::Expression::ExtractVariables(InAssemblySpec.SlotConfigConceptualSizeY, varNames);
		for (const auto& var : varNames)
		{
			float val;
			if (TryGetValueForPart(InAssemblySpec, 0, var, val, formulaErrors))
			{
				PartSlotInstances[0].VariableValues.Add(var, val);
			}
		}

#if WITH_EDITOR
		for (auto& err : formulaErrors)
		{
			err.Append(TEXT(" (ConceptualSize)"));
		}
#endif

		CabinetPanelAssemblyConceptualSizeY = Modumate::Expression::Evaluate(PartSlotInstances[0].VariableValues, InAssemblySpec.SlotConfigConceptualSizeY);
	}

	// Second pass: compute all derived values by applying transformation formulae 
	// Second pass values may depend on values set on first pass on any node or on second pass values further up the hierarchy
		// Convert flip boolean to scale factor.

	for (int32 slotIdx = 0; slotIdx < numSlots; ++slotIdx)
	{
		const FBIMPartSlotSpec& assemblyPart = InAssemblySpec.Parts[slotIdx];

		PartSlotInstances[slotIdx].PresetGUID = InAssemblySpec.Parts[slotIdx].PresetGUID;
		PartSlotInstances[slotIdx].SlotGUID = InAssemblySpec.Parts[slotIdx].SlotGUID;
		// Convert flip boolean to scale factor.
		PartSlotInstances[slotIdx].FlipVector = FVector(
			assemblyPart.Flip[0] ? -1.0f : 1.0f,
			assemblyPart.Flip[1] ? -1.0f : 1.0f,
			assemblyPart.Flip[2] ? -1.0f : 1.0f
		);

		// With initial size values set, extract the variables for all the transform fields
		// Note: ExtractVariables does not clear the container
		TArray<FString> varNames;
		assemblyPart.Size.ExtractVariables(varNames);
		assemblyPart.Translation.ExtractVariables(varNames);
		assemblyPart.Orientation.ExtractVariables(varNames);

		// For each variable, use getVariableValue to tree walk and retrieve data
		for (const auto& var : varNames)
		{
			float val;
			if (TryGetValueForPart(InAssemblySpec, slotIdx, var, val, formulaErrors))
			{
				PartSlotInstances[slotIdx].VariableValues.Add(var, val);
			}
		}

		// Evaluate each part of the transform and keep the values locally for use with mesh components
		assemblyPart.Size.Evaluate(PartSlotInstances[slotIdx].VariableValues, PartSlotInstances[slotIdx].Size);

		PartSlotInstances[slotIdx].VariableValues.Add(ScaledSizeX, PartSlotInstances[slotIdx].Size.X);
		PartSlotInstances[slotIdx].VariableValues.Add(ScaledSizeY, PartSlotInstances[slotIdx].Size.Y);
		PartSlotInstances[slotIdx].VariableValues.Add(ScaledSizeZ, PartSlotInstances[slotIdx].Size.Z);

		assemblyPart.Translation.Evaluate(PartSlotInstances[slotIdx].VariableValues, PartSlotInstances[slotIdx].Location);
		PartSlotInstances[slotIdx].VariableValues.Add(LocationX, PartSlotInstances[slotIdx].Location.X);
		PartSlotInstances[slotIdx].VariableValues.Add(LocationY, PartSlotInstances[slotIdx].Location.Y);
		PartSlotInstances[slotIdx].VariableValues.Add(LocationZ, PartSlotInstances[slotIdx].Location.Z);

		assemblyPart.Orientation.Evaluate(PartSlotInstances[slotIdx].VariableValues, PartSlotInstances[slotIdx].Rotation);
		PartSlotInstances[slotIdx].VariableValues.Add(RotationX, PartSlotInstances[slotIdx].Rotation.X);
		PartSlotInstances[slotIdx].VariableValues.Add(RotationY, PartSlotInstances[slotIdx].Rotation.Y);
		PartSlotInstances[slotIdx].VariableValues.Add(RotationZ, PartSlotInstances[slotIdx].Rotation.Z);
	}
	// The root slot never has a mesh or is flipped by a parent, so start at 1
	for (int32 slotIdx = 1; slotIdx < numSlots; ++slotIdx)
	{
		FPartSlotInstance& slotRef = PartSlotInstances[slotIdx];
		FPartSlotInstance& parentRef = PartSlotInstances[InAssemblySpec.Parts[slotIdx].ParentSlotIndex];

		// Flips and orientations propagate down the child list...a flip of a flip is unflipped
		slotRef.FlipVector *= parentRef.FlipVector;
		slotRef.Rotation += parentRef.Rotation;

		// TODO: reformulate for flip-around-own-origin instead of flip-around-parent
		if (InAssemblySpec.Parts[slotIdx].Mesh.EngineMesh.IsValid())
		{
			FVector locationDelta = slotRef.Location - parentRef.Location;
			slotRef.Location = parentRef.Location + (locationDelta*slotRef.FlipVector);
		}
	}

	return ensureAlwaysMsgf(formulaErrors.Num() == 0,TEXT("Errors found in rigged assembly formulas")) ? EBIMResult::Success : EBIMResult::Error;
}
