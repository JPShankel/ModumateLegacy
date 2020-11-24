// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/ICursor.h"
#include "Framework/Application/IInputProcessor.h"
#include "UObject/WeakObjectPtr.h"

class FSlateApplication;
class FViewportWorldInteractionManager;
struct FAnalogInputEvent;
struct FKeyEvent;
struct FPointerEvent;

class FAutomationCaptureInputProcessor : public IInputProcessor
{
public:
	FAutomationCaptureInputProcessor(class UEditModelInputAutomation* InInputAutomationComponent);

	// IInputProcess overrides
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyCharEvent(FSlateApplication& SlateApp, const FCharacterEvent& InCharEvent) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override;

private:

	/** The UEditModelInputAutomation that will receive the input */
	TWeakObjectPtr<class UEditModelInputAutomation> InputAutomationComponent;
};
