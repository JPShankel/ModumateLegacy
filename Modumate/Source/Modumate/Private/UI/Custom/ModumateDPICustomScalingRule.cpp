// Copyright 2021 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/ModumateDPICustomScalingRule.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"

float UModumateDPICustomScalingRule::GetDPIScaleBasedOnSize(FIntPoint Size) const
{
	float appScale = 1.f;
	TSharedPtr<SViewport> viewport = FSlateApplication::Get().GetGameViewport();
	if (viewport)
	{
		TSharedPtr<SWindow> window = FSlateApplication::Get().FindWidgetWindow(viewport.ToSharedRef());
		if (window)
		{
			appScale = window->GetDPIScaleFactor();
		}
	}
	return appScale;
}
