#pragma once

#include <CoreMinimal.h>
#include <Subsystems/GameInstanceSubsystem.h>
#include <Engine/StreamableManager.h>
#include <Blueprint/UserWidget.h>

#include "AsyncWidgetLoaderTypes.h"
#include "AsyncWidgetHandle.h"
#include "AsyncWidgetLoaderSubsystem.generated.h"

struct FUserWidgetPool;
class UAsyncWidgetHandle;

/**
 * A subsystem that manages asynchronous loading of widgets
 */
UCLASS(BlueprintType)
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
	 * Request a widget asynchronously
	 * @param WidgetClass The soft class reference to load
	 * @param Requester The object requesting the widget
	 * @param OnLoadCompleted Delegate called when loading completes
	 * @param Priority Loading priority (higher values are loaded first)
	 * @param UserData Additional context data for the request
	 * @param bAddToPool Whether to add the widget to the pool
	 * @return Request ID for tracking or cancellation
	 */
	int32 RequestWidget(
		const TSoftClassPtr<UUserWidget>& WidgetClass,
		UObject* Requester,
		const FOnAsyncWidgetLoaded& OnLoadCompleted,
		float Priority = 0.0f,
		int32 UserData = 0,
		bool bAddToPool = true);

	/**
	 * Request a widget asynchronously (Blueprint friendly version)
	 * @param WidgetClass The soft class reference to load
	 * @param Requester The object requesting the widget
	 * @param OnLoadCompleted Blueprint delegate called when loading completes
	 * @param Priority Loading priority (higher values are loaded first)
	 * @param UserData Additional context data for the request
	 * @return Request ID for tracking or cancellation
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	int32 RequestWidget(
		TSoftClassPtr<UUserWidget> WidgetClass,
		UObject* Requester,
		FOnAsyncWidgetLoadedDynamic OnLoadCompleted,
		float Priority = 0.0f,
		int32 UserData = 0);

	/**
	 * Create a placeholder widget to show during loading
	 * @param WidgetClass The widget class being loaded
	 * @param Requester The object requesting the widget
	 * @param Context Additional context for the placeholder
	 * @return A placeholder widget, or nullptr if none available
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	UUserWidget* CreatePlaceholderWidget(
		TSoftClassPtr<UUserWidget> WidgetClass,
		UObject* Requester,
		UObject* Context = nullptr);

	/**
	 * Set the default placeholder widget class to use when none is specified
	 * @param PlaceholderClass The placeholder widget class
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void SetDefaultPlaceholderClass(TSoftClassPtr<UUserWidget> PlaceholderClass);

	/**
	 * Cancel an in-progress widget loading request
	 * @param RequestId The request ID to cancel
	 * @return True if the request was cancelled
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	bool CancelRequest(int32 RequestId);

	/**
	 * Get the status of an async widget request
	 * @param RequestId The request ID to check
	 * @return The current status of the request
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	EAsyncWidgetLoadStatus GetRequestStatus(int32 RequestId) const;

	/**
	 * Create a handle for a widget
	 * @param Widget The widget to create a handle for
	 * @param ClassPath The class path for the widget
	 * @return A new handle object
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	UAsyncWidgetHandle* CreateWidgetHandle(UUserWidget* Widget, FSoftObjectPath ClassPath);

	/**
	 * Set the world and player controller for widget creation
	 * @param World The world to use
	 * @param PlayerController The player controller to use
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void SetCreationContext(UWorld* World, APlayerController* PlayerController);

	/**
	 * Release a widget back to the pool
	 * @param Widget The widget to release
	 * @param ClassPath The class path for the widget
	 * @param bImmediate Whether to release immediately or after a delay
	 */
	void ReleaseWidget(UUserWidget* Widget, const FSoftObjectPath& ClassPath, bool bImmediate = true);

	/**
	 * Release all widgets in the pool
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void ReleaseAllWidgets();

	/**
	 * Get the current number of active requests
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	int32 GetActiveRequestCount() const { return ActiveRequests.Num(); }

	/**
	 * Get a pooled widget for the specified class
	 * This will either return an inactive widget from the pool or create a new one
	 */
	UUserWidget* GetPooledWidget(const TSubclassOf<UUserWidget>& WidgetClass, const FSoftObjectPath& ClassPath);

protected:
	/** StreamableManager for handling async loading */
	FStreamableManager StreamableManager;

	/** Map of class paths to pools */
	UPROPERTY()
	TMap<FString, FUserWidgetPool> ClassToPoolMap;

	/** Map of request IDs to active requests */
	UPROPERTY()
	TMap<int32, FAsyncWidgetRequest> ActiveRequests;

	/** Delayed widget releases */
	UPROPERTY()
	TArray<FDelayedWidgetRelease> DelayedReleases;

	/** Default world for widget creation */
	UPROPERTY()
	TWeakObjectPtr<UWorld> DefaultWorld;

	/** Default player controller for widget creation */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> DefaultPlayerController;

	/** Default placeholder widget class */
	UPROPERTY()
	TSoftClassPtr<UUserWidget> DefaultPlaceholderClass;

	/** Next request ID */
	int32 NextRequestId;

	/** Delay time for releasing widgets (seconds) */
	float WidgetReleaseDelay;

	/** Process the completion of a widget load */
	void OnWidgetClassLoaded(int32 RequestId);

	/** Remove completed or cancelled requests */
	void CleanupRequests();

	/** Process delayed widget releases */
	void ProcessDelayedReleases();

	/** Get a pool for the specified class path */
	FUserWidgetPool& GetOrCreatePool(const FSoftObjectPath& ClassPath);

	/** Tick function */
	bool Tick(float DeltaTime);

	/** Tick delegate handle */
	FTSTicker::FDelegateHandle TickerHandle;
};