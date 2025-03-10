// Copyright 2025, All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Blueprint/UserWidget.h>
#include "AsyncWidgetLoaderTypes.h"
#include "AsyncWidgetPoolManager.generated.h"

struct FUserWidgetPool;
class UAsyncWidgetLoaderSubsystem;

/**
 * Manager class for advanced widget pooling capabilities
 * Can be used to configure and manage pools separately from the subsystem
 */
UCLASS(BlueprintType)
class ASYNCWIDGETLOADER_API UAsyncWidgetPoolManager : public UObject
{
	GENERATED_BODY()

public:
	UAsyncWidgetPoolManager();

	/**
	 * Initialize the pool manager with context information
	 * @param InWorld World for widget creation
	 * @param InPlayerController Player controller for widget creation
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void Initialize(UWorld* InWorld, APlayerController* InPlayerController);

	/**
	 * Pre-create a pool of widgets
	 * @param WidgetClass The widget class to pool
	 * @param NumToPreallocate How many widgets to create initially
	 * @param bAddToGlobalPool Whether to add this pool to the global subsystem
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void PreallocateWidgets(TSoftClassPtr<UUserWidget> WidgetClass, int32 NumToPreallocate, bool bAddToGlobalPool = true);

	/**
	 * Get a widget from the pool
	 * @param WidgetClass The widget class to get
	 * @return A pooled widget instance
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	UUserWidget* GetWidget(TSoftClassPtr<UUserWidget> WidgetClass);

	/**
	 * Release a widget back to the pool
	 * @param Widget The widget to release
	 * @param bImmediate Whether to release immediately or after a delay
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void ReleaseWidget(UUserWidget* Widget, bool bImmediate = true);

	/**
	 * Release all widgets in the pool
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void ReleaseAllWidgets();

	/**
	 * Set maximum pool size
	 * @param WidgetClass The widget class to configure
	 * @param MaxSize The maximum number of pooled instances to keep
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void SetMaxPoolSize(TSoftClassPtr<UUserWidget> WidgetClass, int32 MaxSize);

	/**
	 * Check if a class is already loaded
	 * @param WidgetClass The widget class to check
	 * @return True if the class is already loaded
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	bool IsClassLoaded(TSoftClassPtr<UUserWidget> WidgetClass) const;

	/**
	 * Get stats about a specific pool
	 * @param WidgetClass The widget class to get stats for
	 * @param OutActiveCount Number of active widgets
	 * @param OutInactiveCount Number of inactive (pooled) widgets
	 */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void GetPoolStats(TSoftClassPtr<UUserWidget> WidgetClass, int32& OutActiveCount, int32& OutInactiveCount);

protected:
	/** World for widget creation */
	UPROPERTY()
	TWeakObjectPtr<UWorld> World;

	/** Player controller for widget creation */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> PlayerController;

	/** Max pool sizes for each class */
	UPROPERTY()
	TMap<FString, int32> MaxPoolSizes;

	/** Local pools if not using global system */
	UPROPERTY()
	TMap<FString, FUserWidgetPool> LocalPools;

	/** Reference to the subsystem */
	UPROPERTY()
	TObjectPtr<UAsyncWidgetLoaderSubsystem> AsyncWidgetLoader;

	/** Get the async widget loader subsystem */
	UAsyncWidgetLoaderSubsystem* GetAsyncWidgetLoader();

	/** Get a pool for the specified class path */
	FUserWidgetPool& GetOrCreateLocalPool(const FSoftObjectPath& ClassPath);
};