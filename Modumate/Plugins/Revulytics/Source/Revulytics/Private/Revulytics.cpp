// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Revulytics.h"

#include "AnalyticsEventAttribute.h"
#include "App.h"
#include "Core.h"
#include "EngineVersion.h"
#include "GeneralProjectSettings.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "RevulyticsProvider.h"

#define LOCTEXT_NAMESPACE "FPLUGIN_RevulyticsModule"

DEFINE_LOG_CATEGORY_STATIC(LogRevulytics, Log, All);

IMPLEMENT_MODULE(FAnalyticsRevulytics, Revulytics)

TSharedPtr<IAnalyticsProvider> FAnalyticsRevulytics::RevulyticsProvider;

void FAnalyticsRevulytics::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-

	// Get the base directory of this plugin
	FString BaseDir = IPluginManager::Get().FindPlugin(TEXT("Revulytics"))->GetBaseDir();

	// Add on the relative location of the third party dll and load it
	FString LibraryPath;
#if PLATFORM_WINDOWS
	FString LibraryRelPath = FString::Printf(
		TEXT("Binaries/ThirdParty/RevulyticsLibrary/Win64/ruiSDK_%s.x64.dll"),
		TEXT(REVULYTICS_VER)
	);
	LibraryPath = FPaths::Combine(*BaseDir, *LibraryRelPath);
#endif // PLATFORM_WINDOWS

	RevulyticsLibraryHandle = !LibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*LibraryPath) : nullptr;

	if (RevulyticsLibraryHandle)
	{
		RevulyticsInstance = ruiCreateInstance(false);

		if (SetConfiguration() && SetProductData())
		{
			bConfiguredSDK = true;
		}
	}
}

bool FAnalyticsRevulytics::SetConfiguration()
{
	FString ProductID, AppName, URL, AESKey;

	const TCHAR *SectionName = TEXT("Analytics");
	const FString &IniName = GEngineIni;

	if (GConfig->GetString(SectionName, TEXT("RevulyticsProductID"), ProductID, IniName) &&
		GConfig->GetString(SectionName, TEXT("RevulyticsAppName"), AppName, IniName) &&
		GConfig->GetString(SectionName, TEXT("RevulyticsURL"), URL, IniName) &&
		GConfig->GetString(SectionName, TEXT("RevulyticsAESKey"), AESKey, IniName))
	{
		UE_LOG(LogRevulytics, Log, TEXT("Configuring Revulytics SDK"));
		
		FString configPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Revulytics"));
#if PLATFORM_WINDOWS
		configPath.ReplaceCharInline(TEXT('/'), TEXT('\\'), ESearchCase::CaseSensitive);
#endif // PLATFORM_WINDOWS
		IFileManager::Get().MakeDirectory(*configPath, true);

		RUIRESULT configResult = ruiCreateConfig(RevulyticsInstance,
			StringCast<char>(*configPath).Get(),
			StringCast<char>(*ProductID).Get(),
			StringCast<char>(*AppName).Get(),
			StringCast<char>(*URL).Get(),
			RUI_PROTOCOL_HTTP_PLUS_ENCRYPTION,
			StringCast<char>(*AESKey).Get(),
			false,
			false
		);

		if (configResult == RUI_OK)
		{
			UE_LOG(LogRevulytics, Log, TEXT("Configured Revulytics SDK"));
			return true;
		}
		else
		{
			UE_LOG(LogRevulytics, Error, TEXT("Failed to configure Revulytics SDK, error: %d"), configResult);
		}
	}

	UE_LOG(LogRevulytics, Error, TEXT("Failed to configure Revulytics SDK, missing configuration values."));
	return false;
}

bool FAnalyticsRevulytics::SetProductData()
{
#if UE_BUILD_SHIPPING && !WITH_EDITOR
	static const char *productEditionANSI = "Professional";
#else
	static const char *productEditionANSI = "Development";
#endif
	static const char *productLanguageANSI = "Us English";

	const auto *projectSettings = GetDefault<UGeneralProjectSettings>();
	const FString &projectVersion = projectSettings->ProjectVersion;
	const char *productVersionANSI = StringCast<char>(*projectVersion).Get();

	const FEngineVersion &engineVersion = FEngineVersion::Current();
	FString productBuildNumber = FString::Printf(TEXT("%d"), engineVersion.GetChangelist());
	const char *productBuildNumberAnsi = StringCast<char>(*productBuildNumber).Get();

	RUIRESULT productResult = ruiSetProductData(RevulyticsInstance,
		productEditionANSI, productLanguageANSI, productVersionANSI, productBuildNumberAnsi);

	if (productResult == RUI_OK)
	{
		UE_LOG(LogRevulytics, Log, TEXT("Set Revulytics product data; Project Version=\"%s\", Build Number=\"%s\""),
			*projectVersion, *productBuildNumber);
		return true;
	}
	else
	{
		UE_LOG(LogRevulytics, Error, TEXT("Failed to set Revulytics product data"));
		return false;
	}
}

bool FAnalyticsRevulytics::StartSDK()
{
	if (!ensureAlways(!bStartedSDK))
	{
		return false;
	}

	RUIRESULT sdkResult = ruiStartSDK(RevulyticsInstance);
	if (sdkResult == RUI_OK)
	{
		bStartedSDK = true;
		UE_LOG(LogRevulytics, Log, TEXT("Started Revulytics SDK."));
	}
	else
	{
		UE_LOG(LogRevulytics, Error, TEXT("Failed to start Revulytics SDK, error: %d"), sdkResult);
	}

	return bStartedSDK;
}

bool FAnalyticsRevulytics::StopSDK()
{
	if (!ensureAlways(bStartedSDK))
	{
		return false;
	}

	RUIRESULT sdkResult = ruiStopSDK(RevulyticsInstance, 0);
	if (sdkResult == RUI_OK)
	{
		bStartedSDK = false;
		UE_LOG(LogRevulytics, Log, TEXT("Stopped Revulytics SDK."));
	}
	else
	{
		UE_LOG(LogRevulytics, Error, TEXT("Failed to stop Revulytics SDK, error: %d"), sdkResult);
	}

	return !bStartedSDK;
}

void FAnalyticsRevulytics::ShutdownModule()
{
	if (RevulyticsInstance)
	{
		ruiDestroyInstance(RevulyticsInstance);
		RevulyticsInstance = nullptr;
	}
}

TSharedPtr<IAnalyticsProvider> FAnalyticsRevulytics::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	if (!ensureAlways(bConfiguredSDK))
	{
		UE_LOG(LogRevulytics, Error, TEXT("Can't create a Revulytics provider if the SDK hasn't been configured!"));
	}

	if (!FAnalyticsRevulytics::RevulyticsProvider.IsValid() && bConfiguredSDK)
	{
		FAnalyticsRevulytics::RevulyticsProvider = MakeShareable(
			new FAnalyticsProviderRevulytics(RevulyticsInstance)
		);
	}

	return FAnalyticsRevulytics::RevulyticsProvider;
}

// Provider

FAnalyticsProviderRevulytics::FAnalyticsProviderRevulytics(RUIInstance* InRevulyticsInstance)
	: bHasSessionStarted(false)
	, RevulyticsInstance(InRevulyticsInstance)
{
}

FAnalyticsProviderRevulytics::~FAnalyticsProviderRevulytics()
{
	if (bHasSessionStarted)
	{
		EndSession();
	}
}

bool FAnalyticsProviderRevulytics::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (!ensureAlways(!bHasSessionStarted))
	{
		UE_LOG(LogRevulytics, Error, TEXT("Can't start a Revulytics session if one is already running!"));
		return false;
	}

	RUIRESULT sdkResult = ruiStartSDK(RevulyticsInstance);
	if (sdkResult == RUI_OK)
	{
		bHasSessionStarted = true;
		UE_LOG(LogRevulytics, Log, TEXT("Started Revulytics SDK."));
	}
	else
	{
		UE_LOG(LogRevulytics, Error, TEXT("Failed to start Revulytics SDK, error: %d"), sdkResult);
	}

	return bHasSessionStarted;
}

void FAnalyticsProviderRevulytics::EndSession()
{
	if (!ensureAlways(bHasSessionStarted))
	{
		UE_LOG(LogRevulytics, Error, TEXT("Can't end a Revulytics session unless one is already running!"));
		return;
	}

	RUIRESULT sdkResult = ruiStopSDK(RevulyticsInstance, 0);
	if (sdkResult == RUI_OK)
	{
		bHasSessionStarted = false;
		UE_LOG(LogRevulytics, Log, TEXT("Stopped Revulytics SDK."));
	}
	else
	{
		UE_LOG(LogRevulytics, Error, TEXT("Failed to stop Revulytics SDK, error: %d"), sdkResult);
	}
}

void FAnalyticsProviderRevulytics::FlushEvents()
{
	
}

void FAnalyticsProviderRevulytics::SetUserID(const FString& InUserID)
{
	if (!bHasSessionStarted)
	{
		UserId = InUserID;
		UE_LOG(LogRevulytics, Display, TEXT("User is now (%s)"), *UserId);
	}
	else
	{
		// Log that we shouldn't switch users during a session
		UE_LOG(LogRevulytics, Warning, TEXT("FAnalyticsProviderRevulytics::SetUserID called while a session is in progress. Ignoring."));
	}
}

FString FAnalyticsProviderRevulytics::GetUserID() const
{
	return UserId;
}

FString FAnalyticsProviderRevulytics::GetSessionID() const
{
	return SessionId;
}

bool FAnalyticsProviderRevulytics::SetSessionID(const FString& InSessionID)
{
	if (!bHasSessionStarted)
	{
		SessionId = InSessionID;
		UE_LOG(LogRevulytics, Display, TEXT("Session is now (%s)"), *SessionId);
	}
	else
	{
		// Log that we shouldn't switch session ids during a session
		UE_LOG(LogRevulytics, Warning, TEXT("FAnalyticsProviderRevulytics::SetSessionID called while a session is in progress. Ignoring."));
	}
	return !bHasSessionStarted;
}

void FAnalyticsProviderRevulytics::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	ensureAlways(RevulyticsInstance != nullptr);

	FString eventCategory(TEXT("Default"));

	static const FString attrNameCategory(TEXT("Category"));
	static const FString attrNameCustomValue(TEXT("CustomValue"));

	const FAnalyticsEventAttribute *customValue = nullptr;

	for (const FAnalyticsEventAttribute &eventAttr : Attributes)
	{
		if (eventAttr.AttrName == attrNameCategory)
		{
			eventCategory = eventAttr.ToString();
		}
		else if ((eventAttr.AttrName == attrNameCustomValue) &&
			ensureAlwaysMsgf(customValue == nullptr, TEXT("Event %s specified more than one custom value!")))
		{
			customValue = &eventAttr;
		}
	}

	const char *eventCategoryCStr = StringCast<char>(*eventCategory).Get();
	const char *eventNameCStr = StringCast<char>(*EventName).Get();
	const char *sessionIdCStr = StringCast<char>(*SessionId).Get();

	RUIRESULT eventResult = RUI_FUNCTION_NOT_AVAIL;
	if (customValue)
	{
		switch (customValue->AttrType)
		{
		case FAnalyticsEventAttribute::AttrTypeEnum::Number:
		{
			eventResult = ruiTrackEventNumeric(RevulyticsInstance,
				eventCategoryCStr, eventNameCStr, customValue->AttrValueNumber, sessionIdCStr);
			break;
		}
		case FAnalyticsEventAttribute::AttrTypeEnum::String:
		{
			const char *customValueCStr = StringCast<char>(*customValue->AttrValueString).Get();
			eventResult = ruiTrackEventText(RevulyticsInstance,
				eventCategoryCStr, eventNameCStr, customValueCStr, sessionIdCStr);
			break;
		}
		default:
			UE_LOG(LogRevulytics, Error, TEXT("Unsupported custom value type for event (%s)!"), *EventName);
			break;
		}
	}
	else
	{
		eventResult = ruiTrackEvent(RevulyticsInstance,
			eventCategoryCStr, eventNameCStr, sessionIdCStr);
	}

	if (eventResult == RUI_OK)
	{
		UE_LOG(LogRevulytics, Display, TEXT("Analytics event (%s) written with (%d) attributes"), *EventName, Attributes.Num());
	}
	else
	{
		UE_LOG(LogRevulytics, Error, TEXT("Failed to track event (%s), error: %d"), *EventName, eventResult);
	}
}

void FAnalyticsProviderRevulytics::RecordItemPurchase(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
	if (bHasSessionStarted)
	{
		

		UE_LOG(LogRevulytics, Display, TEXT("(%d) number of item (%s) purchased with (%s) at a cost of (%d) each"), ItemQuantity, *ItemId, *Currency, PerItemCost);
	}
	else
	{
		UE_LOG(LogRevulytics, Warning, TEXT("FAnalyticsProviderRevulytics::RecordItemPurchase called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderRevulytics::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
	if (bHasSessionStarted)
	{
		

		UE_LOG(LogRevulytics, Display, TEXT("(%d) amount of in game currency (%s) purchased with (%s) at a cost of (%f) each"), GameCurrencyAmount, *GameCurrencyType, *RealCurrencyType, RealMoneyCost);
	}
	else
	{
		UE_LOG(LogRevulytics, Warning, TEXT("FAnalyticsProviderRevulytics::RecordCurrencyPurchase called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderRevulytics::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount)
{
	if (bHasSessionStarted)
	{
		

		UE_LOG(LogRevulytics, Display, TEXT("(%d) amount of in game currency (%s) given to user"), GameCurrencyAmount, *GameCurrencyType);
	}
	else
	{
		UE_LOG(LogRevulytics, Warning, TEXT("FAnalyticsProviderRevulytics::RecordCurrencyGiven called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderRevulytics::RecordError(const FString& Error, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		

		UE_LOG(LogRevulytics, Display, TEXT("Error is (%s) number of attributes is (%d)"), *Error, Attributes.Num());
	}
	else
	{
		UE_LOG(LogRevulytics, Warning, TEXT("FAnalyticsProviderRevulytics::RecordError called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderRevulytics::RecordProgress(const FString& ProgressType, const FString& ProgressName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		

		UE_LOG(LogRevulytics, Display, TEXT("Progress event is type (%s), named (%s), number of attributes is (%d)"), *ProgressType, *ProgressName, Attributes.Num());
	}
	else
	{
		UE_LOG(LogRevulytics, Warning, TEXT("FAnalyticsProviderRevulytics::RecordProgress called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderRevulytics::RecordItemPurchase(const FString& ItemId, int ItemQuantity, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		

		UE_LOG(LogRevulytics, Display, TEXT("Item purchase id (%s), quantity (%d), number of attributes is (%d)"), *ItemId, ItemQuantity, Attributes.Num());
	}
	else
	{
		UE_LOG(LogRevulytics, Warning, TEXT("FAnalyticsProviderRevulytics::RecordItemPurchase called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderRevulytics::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		

		UE_LOG(LogRevulytics, Display, TEXT("Currency purchase type (%s), quantity (%d), number of attributes is (%d)"), *GameCurrencyType, GameCurrencyAmount, Attributes.Num());
	}
	else
	{
		UE_LOG(LogRevulytics, Warning, TEXT("FAnalyticsProviderRevulytics::RecordCurrencyPurchase called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderRevulytics::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		

		UE_LOG(LogRevulytics, Display, TEXT("Currency given type (%s), quantity (%d), number of attributes is (%d)"), *GameCurrencyType, GameCurrencyAmount, Attributes.Num());
	}
	else
	{
		UE_LOG(LogRevulytics, Warning, TEXT("FAnalyticsProviderRevulytics::RecordCurrencyGiven called before StartSession. Ignoring."));
	}
}
