// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/DocumentWebBridge.h"
#include "DocumentManagement/ModumateDocument.h"

UModumateDocumentWebBridge::UModumateDocumentWebBridge() {}
UModumateDocumentWebBridge::~UModumateDocumentWebBridge() {}

void UModumateDocumentWebBridge::set_web_focus(bool HasFocus)
{
	Document->set_web_focus(HasFocus);
}

void UModumateDocumentWebBridge::toggle_drawing_designer(bool DrawingEnabled)
{
	Document->toggle_drawing_designer(DrawingEnabled);
}

void UModumateDocumentWebBridge::trigger_update(const TArray<FString>& ObjectTypes)
{
	Document->trigger_update(ObjectTypes);
}

void UModumateDocumentWebBridge::drawing_apply_delta(const FString& InDelta)
{
	Document->drawing_apply_delta(InDelta);
}

void UModumateDocumentWebBridge::drawing_get_drawing_image(const FString& InRequest)
{
	Document->drawing_get_drawing_image(InRequest);
}

void UModumateDocumentWebBridge::drawing_get_clicked(const FString& InRequest)
{
	Document->drawing_get_clicked(InRequest);
}

void UModumateDocumentWebBridge::drawing_get_cutplane_lines(const FString& InRequest)
{
	Document->drawing_get_cutplane_lines(InRequest);
}

void UModumateDocumentWebBridge::get_preset_thumbnail(const FString& InRequest)
{
	Document->get_preset_thumbnail(InRequest);
}

void UModumateDocumentWebBridge::string_to_inches(const FString& InRequest)
{
	Document->string_to_inches(InRequest);
}

void UModumateDocumentWebBridge::string_to_centimeters(const FString& InRequest)
{
	Document->string_to_centimeters(InRequest);
}

void UModumateDocumentWebBridge::centimeters_to_string(const FString& InRequest)
{
	Document->centimeters_to_string(InRequest);
}

// MOI WEB API
void UModumateDocumentWebBridge::create_mois(TArray<FString> MOIType, TArray<int32>ParentID)
{
	Document->create_mois(MOIType, ParentID);
}

void UModumateDocumentWebBridge::delete_mois(TArray<int32> ID)
{
	Document->delete_mois(ID);
}

void UModumateDocumentWebBridge::update_mois(TArray<int32> ID, TArray<FString> MOIData)
{
	Document->update_mois(ID, MOIData);
}

void UModumateDocumentWebBridge::update_player_state(const FString& PlayerStateData)
{
	Document->update_player_state(PlayerStateData);
}

void UModumateDocumentWebBridge::update_project_settings(const FString& InRequest)
{
	Document->update_project_settings(InRequest);
}

void UModumateDocumentWebBridge::update_auto_detect_graphic_settings()
{
	Document->update_auto_detect_graphic_settings();
}

void UModumateDocumentWebBridge::download_pdf_from_blob(const FString& Blob, const FString& DefaultName)
{
	Document->download_pdf_from_blob(Blob, DefaultName);
}

void UModumateDocumentWebBridge::open_bim_designer(const FString& InGUID)
{
	Document->open_bim_designer(InGUID);
}

void UModumateDocumentWebBridge::open_detail_designer()
{
	Document->open_detail_designer();
}

void UModumateDocumentWebBridge::open_delete_preset_menu(const FString& InGUID)
{
	Document->open_delete_preset_menu(InGUID);
}

void UModumateDocumentWebBridge::duplicate_preset(const FString& InGUID)
{
	Document->duplicate_preset(InGUID);
}

void UModumateDocumentWebBridge::export_estimates()
{
	Document->export_estimates();
}

void UModumateDocumentWebBridge::export_dwgs(TArray<int32> InCutPlaneIDs)
{
	Document->export_dwgs(InCutPlaneIDs);
}

void UModumateDocumentWebBridge::create_or_swap_edge_detail(TArray<int32> SelectedEdges, const FString& InNewDetailPresetGUID, const FString& InCurrentPresetGUID)
{
	Document->create_or_swap_edge_detail(SelectedEdges, InNewDetailPresetGUID, InCurrentPresetGUID);
}

void UModumateDocumentWebBridge::update_presets(TArray<FString>& PresetsData)
{}

void UModumateDocumentWebBridge::create_preset(const FString& PresetData)
{}
