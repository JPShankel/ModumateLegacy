// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DocumentWebBridge.generated.h"

class UModumateDocument;

UCLASS()
class MODUMATE_API UModumateDocumentWebBridge : public UObject
{
	GENERATED_BODY()

public:

	UModumateDocumentWebBridge();
	~UModumateDocumentWebBridge();

	UPROPERTY()
	UModumateDocument* Document;

	UFUNCTION()
	void trigger_update(const TArray<FString>& ObjectTypes);

	UFUNCTION()
	void drawing_apply_delta(const FString& InDelta);

	UFUNCTION()
	void drawing_get_drawing_image(const FString& InRequest);

	UFUNCTION()
	void drawing_get_clicked(const FString& InRequest);

	UFUNCTION()
	void drawing_get_cutplane_lines(const FString& InRequest);

	UFUNCTION()
	void get_preset_thumbnail(const FString& InRequest);

	UFUNCTION()
	void string_to_inches(const FString& InRequest);

	UFUNCTION()
	void string_to_centimeters(const FString& InRequest);

	UFUNCTION()
	void centimeters_to_string(const FString& InRequest);

	UFUNCTION()
	void create_mois(TArray<FString> MOIType, TArray<int32>ParentID);

	UFUNCTION()
	void delete_mois(TArray<int32> ID);

	UFUNCTION()
	void update_mois(TArray<int32> ID,TArray<FString> MOIData);

	UFUNCTION()
	void update_player_state(const FString& PlayerStateData);

	UFUNCTION()
	void update_project_settings(const FString& InRequest);

	UFUNCTION()
	void update_auto_detect_graphic_settings();

	UFUNCTION()
	void download_pdf_from_blob(const FString& Blob, const FString& DefaultName);

	UFUNCTION()
	void open_bim_designer(const FString& InGUID);

	UFUNCTION()
	void open_detail_designer();

	UFUNCTION()
	void open_delete_preset_menu(const FString& InGUID);
	
	UFUNCTION()
	void duplicate_preset(const FString& InGUID);

	UFUNCTION()
	void export_estimates();

	UFUNCTION()
	void export_dwgs(TArray<int32> InCutPlaneIDs);

	UFUNCTION()
	void create_or_swap_edge_detail(TArray<int32> SelectedEdges, const FString&  InNewDetailPresetGUID, const FString&  InCurrentPresetGUID);

	UFUNCTION()
	void create_preset(const FString& PresetData);

	UFUNCTION()
	void update_presets(TArray<FString>& PresetsData);

};