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
	NumberProperty,
	AssetProperty,
	DimensionProperty,
	MaterialBinding,
	LayerPriorityGroup,
	LayerPriorityValue,
	ConstructionCostLabor,
	ConstructionCostMaterial
};

/* TODO:
* It is unclear at this stage whether editor deltas will be in their own undo/redo space or integrated with the document
* Future refactor may call for edits to live update the document rather than dirty editor nodes
* If we move in that direction, this delta will supplant the FBIMPresetDocumentDelta and provide fine-grained undo/redoable changes
* For now, we just use editor deltas internally and send whole preset changes to the document in a document delta
*/

// BIM deltas are not serialized
struct MODUMATE_API FBIMPresetEditorDelta
{
	EBIMPresetEditorField FieldType = EBIMPresetEditorField::None;

	FName FieldName;

	FString NewStringRepresentation;

	FString OldStringRepresentation;

	EMaterialChannelFields MaterialChannelSubField = EMaterialChannelFields::None;

	EBIMPresetLayerPriorityGroup LayerPriorityGroup = EBIMPresetLayerPriorityGroup::Other;

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
	FString EnumClassName;

	UPROPERTY()
	EMaterialChannelFields MaterialChannelSubField = EMaterialChannelFields::None;

	UPROPERTY()
	EBIMFormElementWidget FormElementWidgetType = EBIMFormElementWidget::None;

	// Form values are not serialized, only used to create BIM deltas
	FString StringRepresentation;

};

USTRUCT()
struct MODUMATE_API FBIMPresetForm
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBIMPresetFormElement> Elements;

	EBIMResult AddPropertyElement(const FText& DisplayName, const FName& FieldName, EBIMPresetEditorField FieldType);
	EBIMResult AddMaterialBindingElement(const FText& DisplayName, const FName& ChannelName, EMaterialChannelFields MaterialSubField);
	EBIMResult AddLayerPriorityGroupElement();
	EBIMResult AddLayerPriorityValueElement();
	EBIMResult AddConstructionCostElements();

	bool HasField(const FName& Field) const;

	bool operator==(const FBIMPresetForm& RHS) const;
	bool operator!=(const FBIMPresetForm& RHS) const;
};
