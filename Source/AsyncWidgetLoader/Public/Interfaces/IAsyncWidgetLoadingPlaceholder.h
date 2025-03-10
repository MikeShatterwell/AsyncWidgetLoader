// Copyright Mike Desrosiers 2025, All Rights Reserved.
#pragma once

#include <CoreMinimal.h>
#include <UObject/Interface.h>
#include <Blueprint/UserWidget.h>

#include "IAsyncWidgetLoadingPlaceholder.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UAsyncWidgetLoadingPlaceholder : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for placeholder widgets shown during async loading
 */
class ASYNCWIDGETLOADER_API IAsyncWidgetLoadingPlaceholder
{
	GENERATED_BODY()

public:
	/**
	 * Initialize the placeholder with context information
	 * @param WidgetClass The class being loaded
	 * @param LoadingContext Additional context to customize placeholder appearance
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void InitializePlaceholder(TSoftClassPtr<UUserWidget> WidgetClass, UObject* LoadingContext);

	/**
	 * Update loading progress
	 * @param Progress Progress value between 0 and 1
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void OnLoadingProgress(float Progress);

	/**
	 * Called before the placeholder is replaced with the actual widget
	 * Useful for fade-out animations
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void PrepareForReplacement();
    
	/**
	 * Get the desired size for the placeholder
	 * Default implementation returns (256, 256)
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	FVector2D GetPlaceholderDesiredSize();
};