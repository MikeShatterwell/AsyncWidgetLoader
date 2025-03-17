// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Blueprint/UserWidget.h>

#include "Engine/StreamableManager.h"

#include "AsyncWidgetLoaderTypes.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnAsyncWidgetLoadedDynamic, int32, RequestId, UUserWidget*, LoadedWidget);

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

	/** Callback for blueprints when loading completes */
	FOnAsyncWidgetLoadedDynamic OnLoadCompleted;

	/** Optional placeholder widget shown during loading */
	TWeakObjectPtr<UUserWidget> PlaceholderWidget;

	/** Priority for this request (higher gets loaded sooner) */
	float Priority = 0.0f;

	/** When the request was made */
	double RequestTime = 0.0;

	/** Current status of this request */
	EAsyncWidgetLoadStatus Status = EAsyncWidgetLoadStatus::NotStarted;

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