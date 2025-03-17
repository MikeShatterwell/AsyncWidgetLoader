#pragma once

#include <CoreMinimal.h>
#include <Subsystems/GameInstanceSubsystem.h>
#include <Engine/StreamableManager.h>
#include <Blueprint/UserWidget.h>
#include <Blueprint/UserWidgetPool.h>

#include "AsyncWidgetLoaderTypes.h"
#include "AsyncWidgetLoaderSubsystem.generated.h"


/**
 * A subsystem that manages asynchronous loading of widgets and pooling
 * 
 * Key features:
 * - Asynchronously load widget classes
 * - Widget pooling to avoid constant recreation
 * - Placeholder widgets during loading
 * - Handles for easy lifetime management
 */
UCLASS(BlueprintType, DisplayName = "Async Widget Loader")
class ASYNCWIDGETLOADER_API UAsyncWidgetLoaderSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UAsyncWidgetLoaderSubsystem();

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~ End USubsystem Interface
	
	/**
	 * Load a widget class asynchronously and create an instance
	 * 
	 * @param WidgetClass The widget class to load
	 * @param Requester The object requesting the widget, will receive callbacks
	 * @param OnLoadCompleted Callback when loading completes
	 * @param Priority Loading priority (higher values are loaded first)
	 * @return Request ID that can be used to cancel or track the request
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	int32 RequestWidgetAsync(
		const TSoftClassPtr<UUserWidget>& WidgetClass,
		UObject* Requester,
		const FOnAsyncWidgetLoadedDynamic& OnLoadCompleted,
		float Priority = 0.0f);

	/**
	 * Set the creation context for widgets
	 * 
	 * @param World The world to use for widget creation
	 * @param PlayerController The player controller for widget creation
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void SetWidgetCreationContext(UWorld* World, APlayerController* PlayerController);

	/**
	 * Release all widgets in all pools
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void ResetWidgetPools();

	/**
	 * Get a pooled widget for the specified class
	 * Creates a new one if none available in pool
	 * 
	 * @param LoadedWidgetClass The class of widget to get
	 * @return A widget instance from the pool or newly created
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	UUserWidget* GetOrCreatePooledWidget(const TSubclassOf<UUserWidget>& LoadedWidgetClass);

	UFUNCTION()
	void OnPreallocatedWidgetLoaded(int32 RequestId, UUserWidget* LoadedWidget);

	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void PreallocateWidgets(const TSoftClassPtr<UUserWidget>& WidgetClass, int32 NumToPreallocate, UObject* Requester, const float Priority);

	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void ReleaseWidgetToPool(UUserWidget* Widget);

	/**
	 * Cancel an in-progress widget loading request
	 * 
	 * @param RequestId The request ID to cancel
	 * @return True if the request was cancelled
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	bool CancelRequest(int32 RequestId);

	/**
	 * Get the status of an async widget request
	 * 
	 * @param RequestId The request ID to check
	 * @return The current status of the request
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	EAsyncWidgetLoadStatus GetRequestStatus(int32 RequestId) const;

	/** Remove completed or cancelled requests */
	void CleanupRequests();
protected:
	/** StreamableManager for handling async loading */
	FStreamableManager StreamableManager;

	/** Map of class paths to pools */
	UPROPERTY()
	TMap<FString, FUserWidgetPool> ClassPathToPoolMap;

	/** Map of request IDs to active requests */
	UPROPERTY()
	TMap<int32, FAsyncWidgetRequest> ActiveRequests;

	/** Default world for widget creation */
	UPROPERTY()
	TWeakObjectPtr<UWorld> DefaultWorld;

	/** Default player controller for widget creation */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> DefaultPlayerController;

	/** Next request ID */
	UPROPERTY()
	int32 NextRequestId;
	
	/** Process when a widget class finishes loading */
	void OnWidgetClassLoaded(int32 RequestId);
	

	UPROPERTY()
	float CleanupInterval = 5.0f;

	FTimerHandle CleanupTimerHandle;

	/** Get a pool for the specified class path */
	FUserWidgetPool& GetOrCreatePool(const TSoftClassPtr<UUserWidget>& ClassPath);
};