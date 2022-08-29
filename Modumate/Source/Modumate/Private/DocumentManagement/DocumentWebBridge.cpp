// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/DocumentWebBridge.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"
#include "DrawingDesigner/DrawingDesignerDocumentDelta.h"
#include "DrawingDesigner/DrawingDesignerRenderControl.h"
#include "Drafting/DraftingManager.h"
#include "Drafting/ModumateDraftingView.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "Objects/CameraView.h"
#include "Objects/CutPlane.h"
#include "Objects/DesignOption.h"
#include "ModumateCore/EnumHelpers.h"
#include "UI/Custom/ModumateWebBrowser.h"


UModumateDocumentWebBridge::UModumateDocumentWebBridge() {}
UModumateDocumentWebBridge::~UModumateDocumentWebBridge() {}

void UModumateDocumentWebBridge::set_web_focus(bool bHasFocus)
{
	const auto player = GetWorld()->GetFirstLocalPlayerFromController();
	const auto controller = player ? Cast<AEditModelPlayerController>(player->GetPlayerController(GetWorld())) : nullptr;
	if (controller)
	{
		if (bHasFocus)
		{
			controller->InputHandlerComponent->RequestInputDisabled(UModumateWebBrowser::MODUMATE_WEB_TAG, true);
		}
		else
		{
			//KLUDGE: Hack for 3.0.0 release. The asynchronus nature of the Javascript communication makes this
			// necessary so we do not remove focus and evaluate the input in the same frame. For example, closing a menu
			// with escape, but having that escape ALSO get passed to the input handler component. -JN

			//We use GameInstance because it is valid throughout the lifetime of the application, vs 'this' or 'world'
			auto gameInstance = GetWorld()->GetGameInstance();
			GetWorld()->GetTimerManager().SetTimerForNextTick([gameInstance]()
				{
					const auto player = gameInstance->GetWorld()->GetFirstLocalPlayerFromController();
					const auto controller = player ? Cast<AEditModelPlayerController>(player->GetPlayerController(gameInstance->GetWorld())) : nullptr;

					if (controller)
					{
						controller->InputHandlerComponent->RequestInputDisabled(UModumateWebBrowser::MODUMATE_WEB_TAG, false);
					}
				});
		}
	}
}

void UModumateDocumentWebBridge::toggle_drawing_designer(bool bDrawingEnabled)
{
	auto player = GetWorld()->GetFirstLocalPlayerFromController();
	auto controller = player ? Cast<AEditModelPlayerController>(player->GetPlayerController(GetWorld())) : nullptr;

	if (controller)
	{
		controller->ToggleDrawingDesigner(bDrawingEnabled);
	}
}

void UModumateDocumentWebBridge::trigger_update(const TArray<FString>& ObjectTypes)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::trigger_update"));
	Document->DrawingPushDD();
	Document->UpdateWebProjectSettings();

	for (auto& ot : ObjectTypes)
	{
		EObjectType objectType = EObjectType::OTNone;
		FindEnumValueByString<EObjectType>(ot, objectType);
		Document->NetworkClonedObjectTypes.Add(objectType);
	}

	for (auto& type : Document->NetworkClonedObjectTypes) {
		Document->UpdateWebMOIs(type);
	}

	auto* localPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	auto* controller = localPlayer ? Cast<AEditModelPlayerController>(localPlayer->GetPlayerController(GetWorld())) : nullptr;
	if (!ensure(controller && controller->EMPlayerState))
	{
		return;
	}

	controller->EMPlayerState->SendWebPlayerState();

	// initial state of presets
	Document->UpdateWebPresets();
}

void UModumateDocumentWebBridge::drawing_apply_delta(const FString& InDelta)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::drawing_apply_delta"));
	FDrawingDesignerJsDeltaPackage  package;
	if (!package.ReadJson(InDelta))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to read JSON package"));
	}
	else
	{
		TSharedPtr<FDocumentDelta> delta = MakeShared<FDrawingDesignerDocumentDelta>(Document->DrawingDesignerDocument, package);
		TArray<FDeltaPtr> wrapped;
		wrapped.Add(delta);
		if (!Document->ApplyDeltas(wrapped, GetWorld(), package.bAddToUndo))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to apply Delta"));
		}
		else
		{
			Document->DrawingPushDD();
		}
	}
}

void UModumateDocumentWebBridge::drawing_get_drawing_image(const FString& InRequest)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::drawing_get_view_image"));
	FString response;
	bool bResult = Document->DrawingDesignerRenderControl->GetView(InRequest, response);
	if (ensure(bResult))
	{
		Document->DrawingSendResponse(TEXT("pushViewImage"), response);
	}
}

void UModumateDocumentWebBridge::drawing_get_clicked(const FString& InRequest)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::drawing_get_clicked_moi"));
	FDrawingDesignerGenericRequest req;

	if (req.ReadJson(InRequest))
	{
		if (req.requestType != EDrawingDesignerRequestType::getClickedMoi) return;

		FDrawingDesignerMoiResponse moiResponse;
		moiResponse.request = req;

		AModumateObjectInstance* moi = Document->GetObjectById(req.viewId);
		if (moi)
		{
			if (moi->GetObjectType() == EObjectType::OTCutPlane)
			{
				AMOICutPlane* cutPlane = Cast<AMOICutPlane>(moi);
				FVector2D uvVector; uvVector.X = req.uvPosition.x; uvVector.Y = req.uvPosition.y;
				Document->DrawingDesignerRenderControl->GetMoiFromView(uvVector, *cutPlane, moiResponse.moiId);
			}
			moiResponse.spanId = moi->GetParentID();
			const FBIMPresetInstance* preset = Document->BIMPresetCollection.PresetFromGUID(moi->GetAssembly().PresetGUID);
			if (preset)
			{
				moiResponse.presetId = preset->GUID;
			}
		}


		FString jsonResponse;
		moiResponse.WriteJson(jsonResponse);
		Document->DrawingSendResponse(TEXT("onGenericResponse"), jsonResponse);
	}
}

void UModumateDocumentWebBridge::drawing_get_cutplane_lines(const FString& InRequest)
{
	FDrawingDesignerGenericRequest req;
	if (req.ReadJson(InRequest) && req.requestType == EDrawingDesignerRequestType::getCutplaneLines)
	{
		int32 cutPlane = FCString::Atoi(*req.data);

		if (cutPlane != MOD_ID_NONE)
		{
			Document->CurrentDraftingView = MakeShared<FModumateDraftingView>(GetWorld(), Document, UDraftingManager::kDD);
			Document->CurrentDraftingView->GeneratePageForDD(cutPlane, req);
		}
	}
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
	if (ensure(Document != nullptr))
	{
		Document->update_mois(ID, MOIData);
	}
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

void UModumateDocumentWebBridge::create_or_update_preset(const FString& PresetData)
{
	FBIMPresetInstance newPreset;
	FBIMWebPreset webPreset;

	if (!ReadJsonGeneric<FBIMWebPreset>(PresetData, &webPreset))
	{
		return;
	}

	if (!ensure(newPreset.FromWebPreset(webPreset,GetWorld()) == EBIMResult::Success))
	{
		return;
	}

	TArray<FDeltaPtr> deltas;
	auto* existingPreset = Document->GetPresetCollection().PresetFromGUID(newPreset.GUID);

	if (existingPreset)
	{
		auto updatePresetDelta = Document->GetPresetCollection().MakeUpdateDelta(newPreset);
		deltas.Add(updatePresetDelta);
	}
	else
	{
		auto newPresetDelta = Document->GetPresetCollection().MakeCreateNewDelta(newPreset);
		deltas.Add(newPresetDelta);
	}

	Document->ApplyDeltas(deltas, GetWorld());
}

void UModumateDocumentWebBridge::delete_preset(const FString& InGUID)
{
	if (!ensure(Document!=nullptr))
	{
		return;
	}

	FGuid presetGuid;
	FGuid::Parse(InGUID, presetGuid);
	TArray<FDeltaPtr> deltas;
	if (ensure(Document->GetPresetCollection().MakeDeleteDeltas(presetGuid, FGuid(), deltas) == EBIMResult::Success) && deltas.Num() > 0)
	{
		Document->ApplyDeltas(deltas, GetWorld());
	}
}

void UModumateDocumentWebBridge::request_alignment_presets(const FString& GenericRequestJson)
{
	FDrawingDesignerGenericRequest genericRequest;
	FAlignmentPresetWebResponse presetResponse;

	if (!ensure(ReadJsonGeneric<FDrawingDesignerGenericRequest>(GenericRequestJson,&genericRequest)))
	{
		return;
	}

	if (!ensure(genericRequest.requestType == EDrawingDesignerRequestType::getAlignmentPresets))
	{
		return;
	}

	if (!ensure(Document))
	{
		return;
	}

	AModumateObjectInstance* subject = Document->GetObjectById(FCString::Atoi(*genericRequest.data));

	if (!ensure(subject))
	{
		return;
	}

	FBIMTagPath tagPath;
	tagPath.FromString(TEXT("Property_Spatiality_ConnectionDetail_Alignment"));

	TArray<FGuid> alignmentPresets;
	EBIMResult result = Document->GetPresetCollection().GetPresetsForNCP(tagPath, alignmentPresets, true);
	if (!ensure(result == EBIMResult::Success))
	{
		return;
	}

	presetResponse.TagPath = tagPath;

	for (auto& presetGUID : alignmentPresets)
	{
		const FBIMPresetInstance* preset = Document->GetPresetCollection().PresetFromGUID(presetGUID);
		if (!ensure(preset != nullptr))
		{
			continue;
		}

		FMOIAlignment moiAlignment;
		if (preset->TryGetCustomData(moiAlignment))
		{
			if (ensure(subject != nullptr))
			{
				if (subject->GetStateData().AssemblyGUID == moiAlignment.SubjectPZP.PresetGUID)
				{
					FBIMWebPreset webPreset;
					if (ensure(preset->ToWebPreset(webPreset, GetWorld()) == EBIMResult::Success))
					{
						presetResponse.Presets.Add(webPreset);
					}
				}
			}
		}
	}

	FDrawingDesignerGenericStringResponse stringResponse;
	stringResponse.request = genericRequest;

	if (ensure(WriteJsonGeneric<FAlignmentPresetWebResponse>(stringResponse.answer, &presetResponse)))
	{
		FString jsonToSend;
		stringResponse.WriteJson(jsonToSend);
		Document->DrawingSendResponse(TEXT("onGenericResponse"), jsonToSend);
	}
}

void UModumateDocumentWebBridge::create_alignment_preset(int32 SubjectMOI, int32 TargetMOI)
{
	if (!ensure(Document))
	{
		return;
	}

	AModumateObjectInstance* subject = Document->GetObjectById(SubjectMOI);

	if (!ensure(subject))
	{
		return;
	}
	
	FBIMTagPath tagPath;
	tagPath.FromString(TEXT("Property_Spatiality_ConnectionDetail_Alignment"));

	if (!ensure(tagPath.Tags.Num() > 0))
	{
		return;
	}

	TArray<FGuid> alignmentPresets;
	if (!ensure(Document->GetPresetCollection().GetPresetsForNCP(tagPath, alignmentPresets, true) == EBIMResult::Success))
	{
		return;
	}

	if (!ensure(alignmentPresets.Num() > 0))
	{
		return;
	}

	const FBIMPresetInstance* basePreset = Document->GetPresetCollection().PresetsByGUID.Find(alignmentPresets.Last());

	if (!ensure(basePreset))
	{
		return;
	}

	subject->GetStateData().Alignment.SubjectPZP.PresetGUID = subject->GetAssembly().PresetGUID;

	FBIMPresetInstance newPreset = *basePreset;
	newPreset.GUID = FGuid();
	newPreset.SetCustomData(subject->GetStateData().Alignment);

	TArray<FDeltaPtr> deltas;
	auto newPresetDelta = Document->GetPresetCollection().MakeCreateNewDelta(newPreset);
	deltas.Add(newPresetDelta);

	Document->ApplyDeltas({ newPresetDelta }, GetWorld());
}

