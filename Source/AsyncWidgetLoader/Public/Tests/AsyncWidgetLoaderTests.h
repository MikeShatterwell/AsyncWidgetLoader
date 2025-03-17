// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

#include <Blueprint/UserWidget.h>
#include <Misc/AutomationTest.h>

#include "Interfaces/IAsyncWidgetRequestHandler.h"

#include "AsyncWidgetLoaderTests.generated.h"

// Mock widget for testing
UCLASS()
class UMockUserWidget : public UUserWidget
{
	GENERATED_BODY()
};

// Mock object that implements IAsyncWidgetRequestHandler
UCLASS()
class UMockWidgetRequestHandler : public UObject, public IAsyncWidgetRequestHandler
{
	GENERATED_BODY()

public:
	int32 RequestedCount = 0;
	int32 LoadedCount = 0;
	int32 FailedCount = 0;
	int32 CancelledCount = 0;
    
	int32 LastRequestId = INDEX_NONE;
	TSoftClassPtr<UUserWidget> LastWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> LastLoadedWidget = nullptr;

	// For callback testing

	UPROPERTY()
	TObjectPtr<UUserWidget> CallbackWidget = nullptr;
	int32 CallbackRequestId = INDEX_NONE;
	bool CallbackExecuted = false;

	UFUNCTION()
	void OnWidgetLoaded(const int32 RequestId, UUserWidget* LoadedWidget)
	{
		CallbackWidget = LoadedWidget;
		CallbackRequestId = RequestId;
		CallbackExecuted = true;
	}

	// IAsyncWidgetRequestHandler interface
	virtual void OnAsyncWidgetRequested_Implementation(int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass) override;
	virtual void OnAsyncWidgetLoaded_Implementation(int32 RequestId, UUserWidget* LoadedWidget) override;
	virtual void OnAsyncWidgetLoadFailed_Implementation(int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass) override;
	virtual void OnAsyncWidgetLoadCancelled_Implementation(int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass) override;
};

/**
 * Helper class for tests that need to wait for async loading
 */
class FAsyncWidgetLoaderTestHelper
{
public:
	explicit FAsyncWidgetLoaderTestHelper(float TimeoutSeconds = 10.0f);
	bool IsTimedOut() const;

	template<typename PredicateType>
	bool WaitUntil(PredicateType Predicate)
	{
		while (!IsTimedOut())
		{
			if (Predicate())
			{
				return true;
			}

			// Process pending game thread tasks
			FTSTicker::GetCoreTicker().Tick(0.01f);
			FPlatformProcess::Sleep(0.01f);
		}
        
		return false;
	}

private:
	double TimeoutTime;
};