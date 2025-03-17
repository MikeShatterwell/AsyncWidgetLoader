// Copyright Mike Desrosiers 2025, All Rights Reserved.
#pragma once

#include <CoreMinimal.h>
#include <UObject/Interface.h>
#include <Blueprint/UserWidget.h>

#include "IAsyncWidgetRequestHandler.generated.h"

UINTERFACE(BlueprintType)
class UAsyncWidgetRequestHandler : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for objects that want to handle async widget loading events.
 */
class IAsyncWidgetRequestHandler
{
	GENERATED_BODY()

public:
	/**
	 * Called when an async widget load request is initiated
	 * @param RequestId Unique identifier for this request
	 * @param WidgetClass The widget class being loaded
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void OnAsyncWidgetRequested(int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass);
	virtual void OnAsyncWidgetRequested_Implementation(int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass) {}

	/**
	 * Called when an async widget load is completed successfully
	 * @param RequestId Unique identifier for this request
	 * @param LoadedWidget The loaded widget instance
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void OnAsyncWidgetLoaded(int32 RequestId, UUserWidget* LoadedWidget);
	virtual void OnAsyncWidgetLoaded_Implementation(int32 RequestId, UUserWidget* LoadedWidget) {}

	/**
	 * Called when an async widget load fails
	 * @param RequestId Unique identifier for this request
	 * @param WidgetClass The widget class that failed to load
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void OnAsyncWidgetLoadFailed(int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass);
	virtual void OnAsyncWidgetLoadFailed_Implementation(int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass) {}

	/**
	 * Called when an async widget load is cancelled
	 * @param RequestId Unique identifier for this request
	 * @param WidgetClass The widget class whose load was cancelled
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void OnAsyncWidgetLoadCancelled(int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass);
	virtual void OnAsyncWidgetLoadCancelled_Implementation(int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass) {}
};