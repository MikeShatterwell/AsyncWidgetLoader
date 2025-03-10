// Copyright Mike Desrosiers 2025, All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Blueprint/UserWidget.h"
#include "AsyncWidgetLoaderTypes.h"
#include "IAsyncWidgetRequestHandler.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UAsyncWidgetRequestHandler : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for objects that want to handle async widget loading events.
 */
class ASYNCWIDGETLOADER_API IAsyncWidgetRequestHandler
{
	GENERATED_BODY()

public:
	/**
	 * Called when an async widget load request is initiated
	 * @param RequestId Unique identifier for this request
	 * @param WidgetClass The widget class being loaded
	 * @param UserData Additional context data for the request
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void OnAsyncWidgetRequested(int32 RequestId, TSoftClassPtr<UUserWidget> WidgetClass, int32 UserData);

	/**
	 * Called when an async widget load is completed successfully
	 * @param RequestId Unique identifier for this request
	 * @param LoadedWidget The loaded widget instance
	 * @param UserData Additional context data for the request
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void OnAsyncWidgetLoaded(int32 RequestId, UUserWidget* LoadedWidget, int32 UserData);

	/**
	 * Called when an async widget load fails
	 * @param RequestId Unique identifier for this request
	 * @param WidgetClass The widget class that failed to load
	 * @param UserData Additional context data for the request
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void OnAsyncWidgetLoadFailed(int32 RequestId, TSoftClassPtr<UUserWidget> WidgetClass, int32 UserData);

	/**
	 * Called when an async widget load is cancelled
	 * @param RequestId Unique identifier for this request
	 * @param WidgetClass The widget class whose load was cancelled
	 * @param UserData Additional context data for the request
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Async Widget Loader")
	void OnAsyncWidgetLoadCancelled(int32 RequestId, TSoftClassPtr<UUserWidget> WidgetClass, int32 UserData);
};