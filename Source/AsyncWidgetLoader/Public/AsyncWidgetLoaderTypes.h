// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Blueprint/UserWidget.h>

#include "Engine/StreamableManager.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAsyncWidgetLoadedDynamic, int32, RequestId, UUserWidget*, LoadedWidget);
DECLARE_DELEGATE_TwoParams(FOnAsyncWidgetLoaded, int32, UUserWidget*);

// Status of an async widget load request
UENUM(BlueprintType)
enum class EAsyncWidgetLoadStatus : uint8
{
	NotStarted,
	Loading,
	Completed,
	Failed,
	Cancelled
};

// Tracks a single widget load request
USTRUCT()
struct ASYNCWIDGETLOADER_API FAsyncWidgetRequest
{
	GENERATED_BODY()

	/** Unique identifier for this request */
	int32 RequestId = INDEX_NONE;

	/** The soft object path for loading */
	FSoftObjectPath ClassPath;

	/** The soft class reference */
	TSoftClassPtr<UUserWidget> WidgetClass;

	/** The object that requested the widget */
	TWeakObjectPtr<UObject> Requester;

	/** Strong reference to the streamable handle */
	TSharedPtr<FStreamableHandle> StreamableHandle;

	/** Callback when loading completes */
	FOnAsyncWidgetLoaded OnLoadCompleted;

	/** Callback for blueprints when loading completes */
	FOnAsyncWidgetLoadedDynamic OnLoadCompletedBP;

	/** Optional placeholder widget shown during loading */
	TWeakObjectPtr<UUserWidget> PlaceholderWidget;

	/** Priority for this request (higher gets loaded sooner) */
	float Priority = 0.0f;

	/** When the request was made */
	double RequestTime = 0.0;

	/** Current status of this request */
	EAsyncWidgetLoadStatus Status = EAsyncWidgetLoadStatus::NotStarted;

	/** Whether to add the loaded widget to the pool when created */
	bool bAddToPool = true;

	/** User context data passed through to callbacks */
	int32 UserData = 0;

	FAsyncWidgetRequest() = default;

	/** Check if this request is valid */
	bool IsValid() const
	{
		return RequestId != INDEX_NONE && !ClassPath.IsNull();
	}

	/** Check if the requester is still valid */
	bool IsRequesterValid() const
	{
		return Requester.IsValid();
	}

	/** Cancel this request */
	void Cancel()
	{
		if (StreamableHandle.IsValid() && !StreamableHandle->HasLoadCompleted())
		{
			StreamableHandle->CancelHandle();
		}
		StreamableHandle.Reset();
		Status = EAsyncWidgetLoadStatus::Cancelled;
	}
};

/**
 * Tracks a widget that is scheduled for delayed release
 */
USTRUCT()
struct ASYNCWIDGETLOADER_API FDelayedWidgetRelease
{
	GENERATED_BODY()

	/** The widget to release */
	UPROPERTY()
	TObjectPtr<UUserWidget> Widget = nullptr;

	/** The class path for pool lookup */
	FSoftObjectPath ClassPath;

	/** When to release the widget */
	double ReleaseTime = 0.0;

	FDelayedWidgetRelease() = default;

	FDelayedWidgetRelease(UUserWidget* InWidget, const FSoftObjectPath& InClassPath, double InReleaseTime)
		: Widget(InWidget), ClassPath(InClassPath), ReleaseTime(InReleaseTime)
	{
	}
};