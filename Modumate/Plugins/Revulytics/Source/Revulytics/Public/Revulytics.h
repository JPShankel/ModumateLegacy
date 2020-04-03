// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IAnalyticsProviderModule.h"
#include "Modules/ModuleManager.h"

class IAnalyticsProvider;

// Forward declare some types that are stored, that should require including the whole SDK includes.
extern "C" {
	struct __declspec(dllimport) RUIInstance;
}

/**
 * The public interface to this module
 */
class FAnalyticsRevulytics :
	public IAnalyticsProviderModule
{
	/** Singleton for analytics */
	static TSharedPtr<IAnalyticsProvider> RevulyticsProvider;

	/** Handle to the test dll we will load */
	void* RevulyticsLibraryHandle = nullptr;

	RUIInstance* RevulyticsInstance = nullptr;

	/** Whether the SDK has been configured and product data successfully set */
	bool bConfiguredSDK = false;

	/** Whether the SDK has actually been started */
	bool bStartedSDK = false;

	//--------------------------------------------------------------------------
	// Module functionality
	//--------------------------------------------------------------------------
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FAnalyticsRevulytics& Get()
	{
		return FModuleManager::LoadModuleChecked< FAnalyticsRevulytics >( "Revulytics" );
	}

	RUIInstance* GetRUIInstance() { return RevulyticsInstance; }

	//--------------------------------------------------------------------------
	// provider factory functions
	//--------------------------------------------------------------------------
public:
	/**
	 * IAnalyticsProviderModule interface.
	 * Creates the analytics provider given a configuration delegate.
	 * The keys required exactly match the field names in the Config object. 
	 */
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const override;
	
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool SetConfiguration();
	bool SetProductData();
	bool StartSDK();
	bool StopSDK();
};

