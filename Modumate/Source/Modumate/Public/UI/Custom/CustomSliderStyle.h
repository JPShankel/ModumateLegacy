// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateWidgetStyleContainerBase.h"
#include "Styling/SlateTypes.h"
#include "CustomSliderStyle.generated.h"

/**
 *
 */

UCLASS(hidecategories=Object, MinimalAPI)
class UCustomSliderStyle : public USlateWidgetStyleContainerBase
{
	GENERATED_BODY()

public:

	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FSliderStyle CustomSliderStyle;

	virtual const struct FSlateWidgetStyle* const GetStyle() const override
	{
		return static_cast< const struct FSlateWidgetStyle* >( &CustomSliderStyle);
	}
};
