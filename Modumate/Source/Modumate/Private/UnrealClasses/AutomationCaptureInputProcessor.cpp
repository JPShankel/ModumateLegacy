// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/AutomationCaptureInputProcessor.h"

#include "Input/Events.h"
#include "Misc/App.h"
#include "UnrealClasses/EditModelInputAutomation.h"

FAutomationCaptureInputProcessor::FAutomationCaptureInputProcessor(UEditModelInputAutomation* InInputAutomationComponent)
	: InputAutomationComponent(InInputAutomationComponent)
{
}

void FAutomationCaptureInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
}

bool FAutomationCaptureInputProcessor::HandleKeyCharEvent(FSlateApplication& SlateApp, const FCharacterEvent& InCharEvent)
{
	return InputAutomationComponent.IsValid() ? InputAutomationComponent->PreProcessKeyCharEvent(InCharEvent) : false;
}

bool FAutomationCaptureInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	return InputAutomationComponent.IsValid() ? InputAutomationComponent->PreProcessKeyDownEvent(InKeyEvent) : false;
}

bool FAutomationCaptureInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	return InputAutomationComponent.IsValid() ? InputAutomationComponent->PreProcessKeyUpEvent(InKeyEvent) : false;
}

bool FAutomationCaptureInputProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	return InputAutomationComponent.IsValid() ? InputAutomationComponent->PreProcessMouseMoveEvent(MouseEvent) : false;
}

bool FAutomationCaptureInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	return InputAutomationComponent.IsValid() ? InputAutomationComponent->PreProcessMouseButtonDownEvent(MouseEvent) : false;
}

bool FAutomationCaptureInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	return InputAutomationComponent.IsValid() ? InputAutomationComponent->PreProcessMouseButtonUpEvent(MouseEvent) : false;
}

bool FAutomationCaptureInputProcessor::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent)
{
	return InputAutomationComponent.IsValid() ? InputAutomationComponent->PreProcessMouseWheelEvent(InWheelEvent) : false;
}
