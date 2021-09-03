// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "ModumateCore/ModumateDimensionString.h"

#include "ModumateSettingsMenu.generated.h"

#define LOCTEXT_NAMESPACE "Settings"

// Helper enum for use only for displaying and selecting dimension type / unit override in the actual preferences.
UENUM()
enum class EDimensionPreference : uint8
{
	FeetAndInches,
	Meters,
	Millimeters
};

UCLASS(BlueprintType)
class MODUMATE_API UModumateSettingsMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	void UpdateFromCurrentSettings();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateDropDownUserWidget* DimensionPrefDropdown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateDropDownUserWidget* DistIncrementDropdown;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	UModumateDropDownUserWidget* MicSourceDropdown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	UModumateDropDownUserWidget* SpeakerSourceDropdown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UModumateComboBoxStringItem> ItemWidgetOverrideClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateSlider* SliderGraphicShadows = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateSlider* SliderGraphicAntiAliasing = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* CloseButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* VersionText;

	static bool GetDimensionTypesFromPref(EDimensionPreference DimensionPref, EDimensionUnits& OutDimensionType, EUnit& OutDimensionUnit);
	static bool GetDimensionPrefFromTypes(EDimensionUnits DimensionType, EUnit DimensionUnit, EDimensionPreference& OutDimensionPref);

protected:
	UFUNCTION()
	void OnDimensionPrefChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnDistIncrementPrefChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnMicDeviceChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnSpeakerDeviceChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void OnMouseCaptureEndSliderGraphicShadows();

	UFUNCTION()
	void OnMouseCaptureEndSliderGraphicAntiAliasing();

	UPROPERTY()
	EDimensionPreference CurDimensionPref;

	virtual void AudioDevicesChangedHandler();
	virtual void VoiceClientConnectedHandler();
	virtual void PopulateAudioDevices();

	UPROPERTY()
	double CurDistIncrement = 0.0;

	void ApplyCurrentSettings();
	void PopulateDistIncrement();
	void UpdateAndSaveGraphicsSettings();

	TMap<FString, EDimensionPreference> DimPrefsByDisplayString;
	TMap<FString, double> DistIncrementsByDisplayString;

	bool GetAudioInputs(TMap<FString, FString> &OutInputs);
	bool GetAudioOutputs(TMap<FString, FString> &OutOutputs);

	// TODO: Find a better way to tell Vivox "Use defaults". 
	// Need to clean up the ModumateVoice API a bit to allow this -JN
	FText DefaultSelection = LOCTEXT("DefaultSystemDevice", "Default System Device");

	FString CurSelectedMic = TEXT("");
	FString CurSelectedSpeaker = TEXT("");
};

#undef LOCTEXT_NAMESPACE