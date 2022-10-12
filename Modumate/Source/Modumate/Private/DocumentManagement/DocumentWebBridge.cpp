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
#include "UnrealClasses/DynamicIconGenerator.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/SkyActor.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"
#include "DrawingDesigner/DrawingDesignerDocumentDelta.h"
#include "DrawingDesigner/DrawingDesignerRenderControl.h"
#include "Drafting/DraftingManager.h"
#include "Drafting/ModumateDraftingView.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "DrawingDesigner/DrawingDesignerAutomation.h"
#include "Objects/CameraView.h"
#include "Objects/CutPlane.h"
#include "Objects/DesignOption.h"
#include "ModumateCore/EnumHelpers.h"
#include "UI/Custom/ModumateWebBrowser.h"
#include "Objects/EdgeDetailObj.h"
#include "Objects/MetaEdge.h"
#include "Objects/MetaPlane.h"
#include "ModumateCore/EnumHelpers.h"
#include "Online/ModumateCloudConnection.h"
#include "ModumateCore/PlatformFunctions.h"
#include "ModumateCore/ModumateBrowserStatics.h"
#include "UI/Custom/ModumateWebBrowser.h"
#include "UI/EditModelUserWidget.h"

#define LOCTEXT_NAMESPACE "DocumentWebBridge"


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

void UModumateDocumentWebBridge::drawing_get_drawing_autodims(const FString& CutplaneId)
{
	int32 cutPlane = FCString::Atoi(*CutplaneId);

	if (cutPlane != MOD_ID_NONE)
	{
		FDrawingDesignerAutomation generator{Document};
		auto dims = generator.GenerateDimensions(cutPlane);
		FString rsp;
		//We need a container because you cannot directly serialize TArray<>
		FDrawingDesignerDimensionResponse container;
		container.dimensions = dims;
		container.view = CutplaneId;
		WriteJsonGeneric(rsp, &container);
		Document->DrawingSendResponse(TEXT("pushAutoDims"), rsp);
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
		moiResponse.moiId = INDEX_NONE;
		moiResponse.spanId = INDEX_NONE;

		AModumateObjectInstance* moi = Document->GetObjectById(req.viewId);
		if (moi)
		{
			if (moi->GetObjectType() == EObjectType::OTCutPlane)
			{
				AMOICutPlane* cutPlane = Cast<AMOICutPlane>(moi);
				FVector2D uvVector; uvVector.X = req.uvPosition.x; uvVector.Y = req.uvPosition.y;
				int32 hitMoiId = INDEX_NONE;
				Document->DrawingDesignerRenderControl->GetMoiFromView(uvVector, *cutPlane, hitMoiId);
				AModumateObjectInstance* hitMoi = Document->GetObjectById(hitMoiId);
				if(hitMoi)
				{
					moiResponse.moiId = hitMoiId;
					moiResponse.spanId = hitMoi->GetParentID();
					const FBIMPresetInstance* preset = Document->BIMPresetCollection.PresetFromGUID(hitMoi->GetAssembly().PresetGUID);
					if (preset)
					{
						moiResponse.presetId = preset->GUID;
					}			
				}
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
	FDrawingDesignerGenericRequest req;
	if (req.ReadJson(InRequest))
	{
		if (req.requestType != EDrawingDesignerRequestType::getPresetThumbnail) return;
		FDrawingDesignerGenericStringResponse rsp;

		FString jsonResponse;
		rsp.request = req;
		auto* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
		if (controller && controller->DynamicIconGenerator &&
			controller->DynamicIconGenerator->GetIconMeshForAssemblyForWeb(FGuid(req.data), rsp.answer))
		{
			rsp.WriteJson(jsonResponse);
			Document->DrawingSendResponse(TEXT("onGenericResponse"), jsonResponse);
		}
	}
}

void UModumateDocumentWebBridge::string_to_inches(const FString& InRequest)
{
	FDrawingDesignerGenericRequest req;

	if (req.ReadJson(InRequest))
	{
		if (req.requestType != EDrawingDesignerRequestType::stringToInches) return;
		FDrawingDesignerGenericStringResponse rsp;

		FString jsonResponse;
		rsp.request = req;
		const FDocumentSettings& settings = Document->GetCurrentSettings();
		const FModumateFormattedDimension formattedDim = UModumateDimensionStatics::StringToSettingsFormattedDimension(req.data, settings);
		rsp.answer = FString::SanitizeFloat(formattedDim.Centimeters * UModumateDimensionStatics::CentimetersToInches);
		rsp.WriteJson(jsonResponse);

		Document->DrawingSendResponse(TEXT("onGenericResponse"), jsonResponse);
	}
}

void UModumateDocumentWebBridge::string_to_centimeters(const FString& InRequest)
{
	FDrawingDesignerGenericRequest req;

	if (req.ReadJson(InRequest))
	{
		if (req.requestType != EDrawingDesignerRequestType::stringToCentimeters) return;
		FDrawingDesignerGenericFloatResponse rsp;

		FString jsonResponse;
		rsp.request = req;
		const FDocumentSettings& settings = Document->GetCurrentSettings();
		const FModumateFormattedDimension formattedDim = UModumateDimensionStatics::StringToSettingsFormattedDimension(req.data, settings);
		rsp.answer = formattedDim.Centimeters;
		rsp.WriteJson(jsonResponse);

		Document->DrawingSendResponse(TEXT("onGenericResponse"), jsonResponse);
	}
}

void UModumateDocumentWebBridge::centimeters_to_string(const FString& InRequest)
{
	FDrawingDesignerGenericRequest req;

	if (req.ReadJson(InRequest))
	{
		if (req.requestType != EDrawingDesignerRequestType::centimetersToString) return;
		FDrawingDesignerGenericStringResponse rsp;

		FString jsonResponse;
		rsp.request = req;
		FString data = req.data;
		double cm = FCString::Atof(*data);
		rsp.answer = UModumateDimensionStatics::CentimetersToDisplayText(cm).ToString();
		rsp.WriteJson(jsonResponse);

		Document->DrawingSendResponse(TEXT("onGenericResponse"), jsonResponse);
	}
}

// MOI WEB API
void UModumateDocumentWebBridge::create_mois(TArray<FString> MOIType, TArray<int32>ParentID)
{
	//MOIType array length and ParentID array length should always be equal
	if (MOIType.Num() != ParentID.Num())
	{
		return;
	}
	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();

	int32 localNextID = Document->GetNextAvailableID();
	for (int32 i = 0; i < MOIType.Num(); i++)
	{
		EObjectType objectType = EObjectType::OTNone;
		FindEnumValueByString<EObjectType>(MOIType[i], objectType);
		if (objectType == EObjectType::OTDesignOption)
		{
			FString newName = Document->GetNextMoiName(EObjectType::OTDesignOption, LOCTEXT("NewDesignOptionMoiName", "Design Option ").ToString());
			if (ParentID[i] == MOD_ID_NONE && Document->RootDesignOptionID != MOD_ID_NONE)
			{
				ParentID[i] = Document->RootDesignOptionID;
			}
			FMOIDesignOptionData optionData;
			optionData.hexColor = TEXT("#000000");
			FMOIStateData stateData;
			stateData.ParentID = ParentID[i];
			stateData.DisplayName = newName;
			stateData.ID = localNextID++;
			stateData.CustomData.SaveStructData<FMOIDesignOptionData>(optionData);
			stateData.ObjectType = EObjectType::OTDesignOption;
			delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
			AMOIDesignOption* parent = Cast<AMOIDesignOption>(Document->GetObjectById(ParentID[i]));
			if (parent != nullptr)
			{
				FMOIStateData currentState = parent->GetStateData();
				FMOIStateData nextState = currentState;
				FMOIDesignOptionData doData;
				nextState.CustomData.LoadStructData(doData);
				doData.subOptions.AddUnique(stateData.ID);
				nextState.CustomData.SaveStructData(doData);
				delta->AddMutationState(parent, currentState, nextState);
			}
		}
		else if (objectType == EObjectType::OTCameraView)
		{
			auto* Controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
			UCameraComponent* cameraComp = Controller->GetViewTarget()->FindComponentByClass<UCameraComponent>();
			if (cameraComp)
			{
				FDateTime dateTime = Controller->SkyActor->GetCurrentDateTime();
				FString newViewName = Document->GetNextMoiName(EObjectType::OTCameraView, LOCTEXT("NewCameraViewMoiName", "New Camera View ").ToString());
				UModumateBrowserStatics::CreateCameraViewAsMoi(this, cameraComp, newViewName, dateTime, localNextID, 0);
			}
		}
		else
		{
			ensureAlwaysMsgf(false, TEXT("Attempt to create unsupported MOI type via web!"));
		}
	}
	Document->ApplyDeltas({ delta }, GetWorld());
}

void UModumateDocumentWebBridge::delete_mois(TArray<int32> ID)
{
	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
	for (int32 i = 0; i < ID.Num(); i++)
	{
		const AModumateObjectInstance* moi = Document->GetObjectById(ID[i]);
		if (moi != nullptr)
		{
			EObjectType type = moi->GetObjectType();

			delta->AddCreateDestroyState(moi->GetStateData(), EMOIDeltaType::Destroy);

		}
	}
	Document->ApplyDeltas({ delta }, GetWorld());
}

void UModumateDocumentWebBridge::update_mois(TArray<int32> ID, TArray<FString> MOIData)
{
	//Make sure array lengths are equal
	if (ID.Num() != MOIData.Num())
	{
		return;
	}
	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
	for (int32 i = 0; i < ID.Num(); i++)
	{
		AModumateObjectInstance* moi = Document->GetObjectById(ID[i]);
		if (moi == nullptr)
		{
			continue;
		}

		// Preserve state data, alter MOI, make delta to new state, restore state, apply delta
		moi->UpdateStateDataFromObject();
		FMOIStateData stateData = moi->GetStateData();
		moi->FromWebMOI(MOIData[i]);

		delta->AddMutationState(moi, stateData, moi->GetStateData());
		moi->SetStateData(stateData);


		UModumateObjectStatics::UpdateDesignOptionVisibility(Document);

		AMOICameraView* cameraView = Cast<AMOICameraView>(Document->GetObjectById(Document->CachedCameraViewID));

		if (cameraView == nullptr)
		{
			continue;;
		}

		cameraView->InstanceData.SavedVisibleDesignOptions.Empty();
		Algo::TransformIf(Document->GetObjectsOfType(EObjectType::OTDesignOption),
			cameraView->InstanceData.SavedVisibleDesignOptions,
			[](AModumateObjectInstance* MOI)
			{
				AMOIDesignOption* mod = Cast<AMOIDesignOption>(MOI);
				return (ensure(mod) && mod->InstanceData.isShowing);
			}, [](AModumateObjectInstance* MOI)
			{
				return MOI->ID;
			}
			);
	}

	if (delta->IsValid())
	{
		Document->ApplyDeltas({ delta }, GetWorld());
	}
}

void UModumateDocumentWebBridge::update_player_state(const FString& InStateJson)
{
	auto* localPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	auto* controller = localPlayer ? Cast<AEditModelPlayerController>(localPlayer->GetPlayerController(GetWorld())) : nullptr;
	if (!ensure(controller && controller->EMPlayerState))
	{
		return;
	}

	FWebEditModelPlayerState webState;
	if (ReadJsonGeneric<FWebEditModelPlayerState>(InStateJson, &webState))
	{
		controller->EMPlayerState->FromWebPlayerState(webState);
	}
}

void UModumateDocumentWebBridge::update_project_settings(const FString& InRequest)
{
	FWebProjectSettings inWebSettings;
	if (!ReadJsonGeneric<FWebProjectSettings>(InRequest, &inWebSettings))
	{
		return;
	}
	FDocumentSettings nextSettings = Document->CurrentSettings;
	// To document settings
	nextSettings.FromWebProjectSettings(inWebSettings);
	auto settingsDelta = MakeShared<FDocumentSettingDelta>(Document->CurrentSettings, nextSettings);
	Document->ApplyDeltas({ settingsDelta }, GetWorld());

	// To voice settings
	auto player = GetWorld()->GetFirstLocalPlayerFromController();
	auto controller = player ? Cast<AEditModelPlayerController>(player->GetPlayerController(GetWorld())) : nullptr;
	if (controller && controller->VoiceClient)
	{
		controller->VoiceClient->FromWebProjectSettings(inWebSettings);
	}
	// To graphics settings
	UModumateGameInstance* gameInstance = GetWorld()->GetGameInstance<UModumateGameInstance>();
	if (gameInstance)
	{
		gameInstance->UserSettings.GraphicsSettings.FromWebProjectSettings(inWebSettings);
		gameInstance->UserSettings.SaveLocally();
		gameInstance->ApplyGraphicsFromModumateUserSettings();
	}
	Document->UpdateWebProjectSettings();
}

void UModumateDocumentWebBridge::update_auto_detect_graphic_settings()
{
	UModumateGameInstance* gameInstance = GetWorld()->GetGameInstance<UModumateGameInstance>();
	if (gameInstance)
	{
		gameInstance->AutoDetectAndSaveModumateUserSettings();
	}
	Document->UpdateWebProjectSettings();
}

void UModumateDocumentWebBridge::download_pdf_from_blob(const FString& Blob, const FString& DefaultName)
{
	FString OutPath;
	
	UWorld* world = GetWorld();
	if (world) 
	{
		UModumateGameInstance* gameInstance = world->GetGameInstance<UModumateGameInstance>();
		if (gameInstance)
		{
			auto cloudConnection = gameInstance->GetCloudConnection();
			if (cloudConnection)
			{
				TFunction<bool()> networkTickCall = [cloudConnection, world]() { cloudConnection->NetworkTick(world); return true; };

				if (FModumatePlatform::GetSaveFilename(OutPath, networkTickCall, FModumatePlatform::INDEX_PDFFILE))
				{
					TArray<uint8> PdfBytes;
					FBase64::Decode(Blob, PdfBytes);

					FFileHelper::SaveArrayToFile(PdfBytes, *OutPath);
					FPlatformProcess::LaunchFileInDefaultExternalApplication(*OutPath, nullptr, ELaunchVerb::Open);
					Document->NotifyWeb(ENotificationLevel::INFO, TEXT("PDF Exported Successfully"));
				}
			}
		}
	}}

void UModumateDocumentWebBridge::open_bim_designer(const FString& InGUID)
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->EditExistingAssembly(FGuid(InGUID));
	}
}

void UModumateDocumentWebBridge::open_detail_designer()
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->EditDetailDesignerFromSelection();
	}
}

void UModumateDocumentWebBridge::open_delete_preset_menu(const FString& InGUID)
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->CheckDeletePresetFromWebUI(FGuid(InGUID));
	}
}

void UModumateDocumentWebBridge::duplicate_preset(const FString& InGUID)
{
	FBIMPresetInstance newPreset;
	ensureAlways(Document->DuplicatePreset(GetWorld(), FGuid(InGUID), newPreset));
}

void UModumateDocumentWebBridge::export_estimates()
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (controller)
	{
		controller->OnCreateQuantitiesCsv();
	}
}

void UModumateDocumentWebBridge::export_dwgs(TArray<int32> InCutPlaneIDs)
{
	if (InCutPlaneIDs.Num() == 0)
	{
		return;
	}

	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (controller)
	{
		controller->OnCreateDwg(InCutPlaneIDs);
	}
}

void UModumateDocumentWebBridge::create_or_swap_edge_detail(TArray<int32> SelectedEdges, const FString& InNewDetailPresetGUID, const FString& InCurrentPresetGUID)
{
	auto world = GetWorld();
	auto* localPlayer = world ? world->GetFirstLocalPlayerFromController() : nullptr;
	auto* controller = localPlayer ? Cast<AEditModelPlayerController>(localPlayer->GetPlayerController(world)) : nullptr;
	if (!ensure(controller && controller->EMPlayerState))
	{
		return;
	}

	auto document = controller ? controller->GetDocument() : nullptr;

	if (!ensure(document && (SelectedEdges.Num() > 0)))
	{
		return;
	}

	FGuid newDetailPresetGuid = FGuid(InNewDetailPresetGUID);
	FGuid existingDetailPresetID = FGuid(InCurrentPresetGUID);
	bool bPopulatedDetailData = false;
	FEdgeDetailData newDetailData;
	FBIMPresetInstance* swapPresetInstance = nullptr;
	auto& presetCollection = document->GetPresetCollection();
	bool bClearingDetail = existingDetailPresetID.IsValid() && !newDetailPresetGuid.IsValid();

	if (newDetailPresetGuid.IsValid())
	{
		swapPresetInstance = presetCollection.PresetFromGUID(newDetailPresetGuid);

		// If we passed in a new preset ID, it better already be in the preset collection and have detail data.
		if (!ensure(swapPresetInstance && swapPresetInstance->TryGetCustomData(newDetailData)))
		{
			return;
		}

		bPopulatedDetailData = true;
	}

	TMap<int32, int32> validDetailOrientationByEdgeID;
	TArray<int32> tempDetailOrientations;

	for (int32 edgeID : SelectedEdges)
	{
		auto edgeMOI = Cast<AMOIMetaEdge>(Document->GetObjectById(edgeID));
		if (!ensure(edgeMOI))
		{
			return;
		}

		if (bPopulatedDetailData)
		{
			FEdgeDetailData curDetailData(edgeMOI->GetMiterInterface());
			if (!ensure(newDetailData.CompareConditions(curDetailData, tempDetailOrientations) && (tempDetailOrientations.Num() > 0)))
			{
				return;
			}
			validDetailOrientationByEdgeID.Add(edgeID, tempDetailOrientations[0]);
		}
		else
		{
			newDetailData.FillFromMiterNode(edgeMOI->GetMiterInterface());
			validDetailOrientationByEdgeID.Add(edgeID, 0);
		}
	}

	TArray<FDeltaPtr> deltas;

	// If we don't already have a new preset ID, and we're not clearing an existing detail, then make a delta to create one.
	if (!newDetailPresetGuid.IsValid() && !bClearingDetail)
	{
		FBIMPresetInstance newDetailPreset;
		FBIMTagPath edgeDetalNCP = FBIMTagPath(TEXT("Property_Spatiality_ConnectionDetail_Edge"));
		if (!ensure((presetCollection.GetBlankPresetForNCP(edgeDetalNCP, newDetailPreset) == EBIMResult::Success) &&
			newDetailPreset.SetCustomData(newDetailData) == EBIMResult::Success))
		{
			return;
		}

		// Try to make a unique display name for this detail preset
		UModumateDocument::TryMakeUniquePresetDisplayName(presetCollection, newDetailData, newDetailPreset.DisplayName);

		auto makedetailPresetDelta = presetCollection.MakeCreateNewDelta(newDetailPreset, this);
		newDetailPresetGuid = newDetailPreset.GUID;
		deltas.Add(makedetailPresetDelta);
	}

	// Now, create MOI delta(s) for creating/updating/deleting the edge detail MOI(s) for the selected edge(s)
	auto updateDetailMOIDelta = MakeShared<FMOIDelta>();
	int nextID = document->GetNextAvailableID();
	for (int32 edgeID : SelectedEdges)
	{
		AMOIMetaEdge* edgeMOI = Cast<AMOIMetaEdge>(document->GetObjectById(edgeID));

		// If there already was a preset and the new one is empty, then only delete the edge detail MOI for each selected edge.
		if (bClearingDetail && ensure(edgeMOI && edgeMOI->CachedEdgeDetailMOI))
		{
			updateDetailMOIDelta->AddCreateDestroyState(edgeMOI->CachedEdgeDetailMOI->GetStateData(), EMOIDeltaType::Destroy);
			continue;
		}

		// Otherwise, update the state data and detail assembly for the edge.
		FMOIStateData newDetailState;
		if (edgeMOI->CachedEdgeDetailMOI)
		{
			newDetailState = edgeMOI->CachedEdgeDetailMOI->GetStateData();
		}
		else
		{
			newDetailState = FMOIStateData(nextID++, EObjectType::OTEdgeDetail, edgeID);
		}

		newDetailState.AssemblyGUID = newDetailPresetGuid;
		newDetailState.CustomData.SaveStructData(FMOIEdgeDetailData(validDetailOrientationByEdgeID[edgeID]));

		if (edgeMOI->CachedEdgeDetailMOI)
		{
			updateDetailMOIDelta->AddMutationState(edgeMOI->CachedEdgeDetailMOI, edgeMOI->CachedEdgeDetailMOI->GetStateData(), newDetailState);
		}
		else
		{
			updateDetailMOIDelta->AddCreateDestroyState(newDetailState, EMOIDeltaType::Create);
		}
	}

	if (updateDetailMOIDelta->IsValid())
	{
		deltas.Add(updateDetailMOIDelta);
	}

	document->ApplyDeltas(deltas, world);
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

	UpdateOrAddPreset(newPreset);
}

void UModumateDocumentWebBridge::push_initial_presets_and_taxonomy(const FString& InitialPresets,
	const FString& Taxonomy)
{
	UE_LOG(LogCallTrace, Log, TEXT("push_initial_presets_and_taxonomy() started"));
	ensure(Document->GetPresetCollection().PresetTaxonomy.FromWebTaxonomyJson(Taxonomy) == EBIMResult::Success);
	
	TSharedPtr<FJsonObject> initialPresetsJson = MakeShareable(new FJsonObject);
	TSharedRef<TJsonReader<>> initialPresetsReader = TJsonReaderFactory<>::Create(InitialPresets);
	if(!ensure(FJsonSerializer::Deserialize(initialPresetsReader, initialPresetsJson)))
	{
		return;
	}
	TMap<FString, TSharedPtr<FJsonValue>> topLevelEntries = initialPresetsJson->Values;
	for(const auto& entry: topLevelEntries)
	{
		FBIMPresetInstance newPreset;
		FBIMWebPreset webPreset;
		TSharedPtr<FJsonObject> objPreset = entry.Value->AsObject();
		FJsonObjectConverter::JsonObjectToUStruct<FBIMWebPreset>(objPreset.ToSharedRef(), &webPreset);
		newPreset.FromWebPreset(webPreset, GetWorld());
		UpdateOrAddPreset(newPreset);
	}
	UE_LOG(LogCallTrace, Log, TEXT("push_initial_presets_and_taxonomy() finished"));
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
				if (subject->GetStateData().AssemblyGUID == moiAlignment.subjectPZP.presetGuid)
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

	const FBIMPresetInstance* basePreset = Document->GetPresetCollection().PresetFromGUID(alignmentPresets.Last());

	if (!ensure(basePreset))
	{
		return;
	}

	subject->GetStateData().Alignment.subjectPZP.presetGuid = subject->GetAssembly().PresetGUID;

	FBIMPresetInstance newPreset = *basePreset;
	newPreset.GUID = FGuid();
	newPreset.SetCustomData(subject->GetStateData().Alignment);

	TArray<FDeltaPtr> deltas;
	auto newPresetDelta = Document->GetPresetCollection().MakeCreateNewDelta(newPreset);
	deltas.Add(newPresetDelta);

	Document->ApplyDeltas({ newPresetDelta }, GetWorld());
}

void UModumateDocumentWebBridge::export_views(TArray<int32> CameraViewIDs)
{
	auto world = GetWorld();
	if (!world)
		return;
	TArray<AMOICameraView*> outActors, rayTracingEnabledViews, nonRTViews;
	UModumateObjectStatics::FindAllActorsOfClass(world, outActors);
	if (outActors.Num() == 0)
	{
		//no camera views found
		return;
	}
	
	for (AMOICameraView* cv : outActors)
	{
		if (cv != nullptr && CameraViewIDs.Contains(cv->ID))
		{
			if (cv->InstanceData.bRTEnabled)
			{
				rayTracingEnabledViews.Add(cv);
			}
			else 
			{
				nonRTViews.Add(cv);
			}
		}
	}
	if (rayTracingEnabledViews.Num() == 0 && nonRTViews.Num() == 0)
	{
		//selected camera views not found
		return;
	}
	// Get path from dialog
	FString filePath;
	AEditModelPlayerController* controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if (!controller->GetFolderPathWithDialog(filePath))
	{
		return;
	}

	for (AMOICameraView* cv : nonRTViews)
	{
		controller->CaptureCameraView(filePath, cv);
	}
	controller->CaptureCameraViewsRayTracing(rayTracingEnabledViews, filePath);
}

void UModumateDocumentWebBridge::UpdateOrAddPreset(FBIMPresetInstance& Preset)
{
	TArray<FDeltaPtr> deltas;
	auto* existingPreset = Document->GetPresetCollection().PresetFromGUID(Preset.GUID);

	if (existingPreset)
	{
		auto updatePresetDelta = Document->GetPresetCollection().MakeUpdateDelta(Preset);
		deltas.Add(updatePresetDelta);
	}
	else
	{
		auto newPresetDelta = Document->GetPresetCollection().MakeCreateNewDelta(Preset);
		deltas.Add(newPresetDelta);
	}


	Document->ApplyDeltas(deltas, GetWorld());
}

#undef LOCTEXT_NAMESPACE