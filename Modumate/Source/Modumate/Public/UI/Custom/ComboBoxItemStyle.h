// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateWidgetStyleContainerBase.h"
#include "Styling/SlateTypes.h"
#include "ComboBoxItemStyle.generated.h"

/**
 *
 */

UCLASS(hidecategories=Object, MinimalAPI)
class UComboBoxItemStyle : public USlateWidgetStyleContainerBase
{
	GENERATED_BODY()

public:
	// ItemStyle in combobox uses TableRowStyle
	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FTableRowStyle ComboBoxItemStyle;

	virtual const struct FSlateWidgetStyle* const GetStyle() const override
	{
		return static_cast< const struct FSlateWidgetStyle* >( &ComboBoxItemStyle);
	}
};
