// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class ASYNCWIDGETLOADER_API FAsyncWidgetLoaderModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** Gets the module instance */
	static inline FAsyncWidgetLoaderModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FAsyncWidgetLoaderModule>("AsyncWidgetLoader");
	}
};
