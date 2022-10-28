// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"
#include "BIMKernel/Presets/BIMPresetLayerPriority.h"
#include "BIMKernel/Core/BIMEnums.h"

#include "BIMPresetEditorForm.generated.h"

struct FBIMConstructionCost;

// TODO: Only property deltas implemented for now
// Coming soon: custom data deltas (material bindings, construction details)

UENUM()
enum class EBIMPresetEditorField : uint8
{
	None = 0,
	TextProperty,
	IntegerProperty,
	NumberProperty,
	AssetProperty,
	NameProperty,
	ColorProperty,
	DimensionProperty,
	DropdownProperty,
	
	MaterialBinding,
};

// BIM deltas are not serialized
struct MODUMATE_API FBIMPresetEditorDelta
{
	EBIMPresetEditorField FieldType = EBIMPresetEditorField::None;

	FString FieldName;
	
	FString CustomDataStructName;

	FString NewStringRepresentation;

	FString OldStringRepresentation;
	
	FString EnumClassName;

	EMaterialChannelFields MaterialChannelSubField = EMaterialChannelFields::None;

	FBIMPresetEditorDelta Inverted() const;
};

UENUM()
enum class EBIMFormElementWidget : uint8
{
	None = 0,
	TextEntry,
	EnumSelect,
	GUIDSwap,
	ColorPicker
};

USTRUCT()
struct MODUMATE_API FBIMPresetFormElement
{
	GENERATED_BODY()

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	EBIMPresetEditorField FieldType = EBIMPresetEditorField::None;

	UPROPERTY()
	FString FieldName;
	
	UPROPERTY()
	FString CustomDataStructName;

	UPROPERTY()
	FString EnumClassName;

	UPROPERTY()
	EMaterialChannelFields MaterialChannelSubField = EMaterialChannelFields::None;

	UPROPERTY()
	EBIMFormElementWidget FormElementWidgetType = EBIMFormElementWidget::None;
	
	FString StringRepresentation;
};

USTRUCT()
struct MODUMATE_API FBIMPresetForm
{
	GENERATED_BODY()
	
	TArray<FBIMPresetFormElement> Elements;

	EBIMResult AddPropertyElement(const FText& DisplayName, const FName& DataStruct, const FName& FieldName, EBIMPresetEditorField FieldType);
	EBIMResult AddMaterialBindingElement(const FText& DisplayName, const FName& ChannelName, EMaterialChannelFields MaterialSubField);
	bool HasField(const FName& Field) const;

	bool operator==(const FBIMPresetForm& RHS) const;
	bool operator!=(const FBIMPresetForm& RHS) const;
};
