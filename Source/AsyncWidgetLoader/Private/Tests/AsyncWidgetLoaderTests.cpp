// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/AsyncWidgetLoaderTests.h"

#include <Blueprint/UserWidget.h>
#include <Engine/Engine.h>
#include <Engine/World.h>
#include <GameFramework/PlayerController.h>
#include <Kismet/GameplayStatics.h>
#include <TimerManager.h>
#include <UObject/SoftObjectPath.h>

#include "AsyncWidgetLoaderSubsystem.h"
#include "Interfaces/IAsyncWidgetRequestHandler.h"

// Implementation of UMockWidgetRequestHandler interface methods
void UMockWidgetRequestHandler::OnAsyncWidgetRequested_Implementation(const int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	RequestedCount++;
	LastRequestId = RequestId;
	LastWidgetClass = WidgetClass;
}

void UMockWidgetRequestHandler::OnAsyncWidgetLoaded_Implementation(const int32 RequestId, UUserWidget* LoadedWidget)
{
	LoadedCount++;
	LastRequestId = RequestId;
	LastLoadedWidget = LoadedWidget;
}

void UMockWidgetRequestHandler::OnAsyncWidgetLoadFailed_Implementation(const int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	FailedCount++;
	LastRequestId = RequestId;
	LastWidgetClass = WidgetClass;
}

void UMockWidgetRequestHandler::OnAsyncWidgetLoadCancelled_Implementation(const int32 RequestId, const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	CancelledCount++;
	LastRequestId = RequestId;
	LastWidgetClass = WidgetClass;
}

// FAsyncWidgetLoaderTestHelper implementation
FAsyncWidgetLoaderTestHelper::FAsyncWidgetLoaderTestHelper(const float TimeoutSeconds)
	: TimeoutTime(FPlatformTime::Seconds() + TimeoutSeconds)
{
}

bool FAsyncWidgetLoaderTestHelper::IsTimedOut() const
{
	return FPlatformTime::Seconds() >= TimeoutTime;
}

/**
 * Test case for AsyncWidgetLoader subsystem functionality
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncWidgetLoaderSubsystemTest, "Game.UI.AsyncWidgetLoader.Subsystem", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAsyncWidgetLoaderSubsystemTest::RunTest(const FString& Parameters)
{
	// Get the game instance
	const UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	if (!TestNotNull("GameInstance should exist", GameInstance))
	{
		return false;
	}

	// Get the subsystem
	UAsyncWidgetLoaderSubsystem* WidgetLoader = GameInstance->GetSubsystem<UAsyncWidgetLoaderSubsystem>();
	if (!TestNotNull("AsyncWidgetLoaderSubsystem should exist", WidgetLoader))
	{
		return false;
	}

	// Test: Subsystem should be initialized
	TestTrue("Subsystem should be initialized", WidgetLoader != nullptr);

	// Set context for widget creation
	UWorld* World = GameInstance->GetWorld();
	APlayerController* PC = nullptr;
	
	// Get the primary player controller associated with the local player
	if (const ULocalPlayer* LocalPlayer = GameInstance->GetFirstLocalPlayerController()->GetLocalPlayer())
	{
		PC = LocalPlayer->GetPlayerController(World);
	}
	WidgetLoader->SetWidgetCreationContext(World, PC);

	return true;
}

/**
 * Test case for widget request functionality
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncWidgetLoaderRequestTest, "Game.UI.AsyncWidgetLoader.Requests", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAsyncWidgetLoaderRequestTest::RunTest(const FString& Parameters)
{
	// Get the game instance
	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	if (!TestNotNull("GameInstance should exist", GameInstance))
	{
		return false;
	}

	// Get the subsystem
	UAsyncWidgetLoaderSubsystem* WidgetLoader = GameInstance->GetSubsystem<UAsyncWidgetLoaderSubsystem>();
	if (!TestNotNull("AsyncWidgetLoaderSubsystem should exist", WidgetLoader))
	{
		return false;
	}

	// Create a mock handler
	UMockWidgetRequestHandler* Handler = NewObject<UMockWidgetRequestHandler>(GameInstance);
	
	// Use a widget class we know exists
	const TSubclassOf<UUserWidget> HardWidgetClass = UMockUserWidget::StaticClass();
	const TSoftClassPtr<UUserWidget> WidgetClass(HardWidgetClass);
	
	// Make the request
	const int32 RequestId = WidgetLoader->RequestWidgetAsync(
		WidgetClass,
		Handler,
		FOnAsyncWidgetLoadedDynamic(),
		1.0f
	);
	
	// Verify request was made
	TestTrue("RequestId should be valid", RequestId != INDEX_NONE);
	TestEqual("Request status should be Loading", WidgetLoader->GetRequestStatus(RequestId), EAsyncWidgetLoadStatus::Loading);
	
	// Cancel the request
	bool CancelResult = WidgetLoader->CancelRequest(RequestId);
	TestTrue("Cancel should succeed", CancelResult);
	
	// Test with invalid widget class
	const TSoftClassPtr<UUserWidget> InvalidWidgetClass;
	const int32 InvalidRequestId = WidgetLoader->RequestWidgetAsync(
		InvalidWidgetClass,
		Handler,
		FOnAsyncWidgetLoadedDynamic(),
		1.0f
	);
	
	// Verify request was rejected
	TestEqual("Invalid request should return INDEX_NONE", InvalidRequestId, INDEX_NONE);
	
	// Test with invalid requester
	const int32 InvalidRequesterRequest = WidgetLoader->RequestWidgetAsync(
		WidgetClass,
		nullptr,
		FOnAsyncWidgetLoadedDynamic(),
		1.0f
	);
	
	// Verify request was rejected
	TestEqual("Invalid requester should return INDEX_NONE", InvalidRequesterRequest, INDEX_NONE);
	
	return true;
}

/**
 * Test case for widget loading callbacks
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncWidgetLoaderCallbackTest, "Game.UI.AsyncWidgetLoader.Callbacks", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAsyncWidgetLoaderCallbackTest::RunTest(const FString& Parameters)
{
	// Get the game instance
	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	if (!TestNotNull("GameInstance should exist", GameInstance))
	{
		return false;
	}

	// Get the subsystem
	UAsyncWidgetLoaderSubsystem* WidgetLoader = GameInstance->GetSubsystem<UAsyncWidgetLoaderSubsystem>();
	if (!TestNotNull("AsyncWidgetLoaderSubsystem should exist", WidgetLoader))
	{
		return false;
	}

	// Set context for widget creation
	UWorld* World = GameInstance->GetWorld();
	APlayerController* PC = nullptr;
	
	// Get the primary player controller associated with the local player
	if (const ULocalPlayer* LocalPlayer = GameInstance->GetFirstLocalPlayerController()->GetLocalPlayer())
	{
		PC = LocalPlayer->GetPlayerController(World);
	}
	WidgetLoader->SetWidgetCreationContext(World, PC);

	// Create a mock handler
	UMockWidgetRequestHandler* Handler = NewObject<UMockWidgetRequestHandler>(GameInstance);
	
	// Mock widget class that already exists
	const TSubclassOf<UUserWidget> LoadedWidgetClass = UMockUserWidget::StaticClass();
	const TSoftClassPtr<UUserWidget> WidgetClassRef(LoadedWidgetClass);
	
	// Track callbacks
	bool CallbackExecuted = false;
	const UUserWidget* CallbackWidget = nullptr;
	int32 CallbackRequestId = INDEX_NONE;
	
	FOnAsyncWidgetLoadedDynamic OnLoadCompleted;
	OnLoadCompleted.BindDynamic(Handler, &UMockWidgetRequestHandler::OnWidgetLoaded);
	
	// Make the request with already loaded class
	const int32 RequestId = WidgetLoader->RequestWidgetAsync(
		WidgetClassRef,
		Handler,
		OnLoadCompleted,
		1.0f
	);
	
	// Verify request was made
	TestTrue("RequestId should be valid", RequestId != INDEX_NONE);
	
	// Since class is already loaded, callback should execute immediately
	TestTrue("Callback should have executed", CallbackExecuted);
	TestNotNull("Callback widget should not be null", CallbackWidget);
	TestEqual("Callback RequestId should match", CallbackRequestId, RequestId);
	
	// Verify interface callbacks were called
	TestEqual("OnAsyncWidgetRequested should be called", Handler->RequestedCount, 0); // Not called for already loaded widgets
	TestEqual("OnAsyncWidgetLoaded should be called", Handler->LoadedCount, 1);
	TestEqual("LastRequestId should match", Handler->LastRequestId, RequestId);
	TestNotNull("LastLoadedWidget should not be null", Handler->LastLoadedWidget.Get());
	
	// Reset tracking
	CallbackExecuted = false;
	CallbackWidget = nullptr;
	CallbackRequestId = INDEX_NONE;
	Handler->RequestedCount = 0;
	Handler->LoadedCount = 0;
	
	// Make the async request
	const int32 AsyncRequestId = WidgetLoader->RequestWidgetAsync(
		WidgetClassRef,
		Handler,
		OnLoadCompleted,
		1.0f
	);
	
	// Verify request was made
	TestTrue("AsyncRequestId should be valid", AsyncRequestId != INDEX_NONE);
	TestEqual("AsyncRequest status should be Loading", WidgetLoader->GetRequestStatus(AsyncRequestId), EAsyncWidgetLoadStatus::Loading);
	
	// Verify interface callbacks for request start
	TestEqual("OnAsyncWidgetRequested should be called", Handler->RequestedCount, 1);
	TestEqual("OnAsyncWidgetLoaded should not be called yet", Handler->LoadedCount, 0);
	
	// Wait for async loading to complete
	FAsyncWidgetLoaderTestHelper Helper;
	const bool LoadCompleted = Helper.WaitUntil([&]() {
		return CallbackExecuted || 
			   WidgetLoader->GetRequestStatus(AsyncRequestId) == EAsyncWidgetLoadStatus::Completed ||
			   WidgetLoader->GetRequestStatus(AsyncRequestId) == EAsyncWidgetLoadStatus::Failed;
	});

	return LoadCompleted && CallbackExecuted && 
		   CallbackRequestId == AsyncRequestId && 
		   Handler->LoadedCount == 1 &&
		   Handler->LastLoadedWidget.Get() != nullptr;
}

/**
 * Test case for widget pooling functionality
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncWidgetLoaderPoolingTest, "Game.UI.AsyncWidgetLoader.Pooling", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAsyncWidgetLoaderPoolingTest::RunTest(const FString& Parameters)
{
	// Get the game instance
	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	if (!TestNotNull("GameInstance should exist", GameInstance))
	{
		return false;
	}

	// Get the subsystem
	UAsyncWidgetLoaderSubsystem* WidgetLoader = GameInstance->GetSubsystem<UAsyncWidgetLoaderSubsystem>();
	if (!TestNotNull("AsyncWidgetLoaderSubsystem should exist", WidgetLoader))
	{
		return false;
	}

	// Set context for widget creation
	APlayerController* PC = UGameplayStatics::GetPlayerController(GameInstance->GetWorld(), 0);
	WidgetLoader->SetWidgetCreationContext(GameInstance->GetWorld(), PC);

	// Mock widget class that exists
	TSubclassOf<UUserWidget> WidgetClass = UMockUserWidget::StaticClass();
	
	// Get a widget from the pool
	UUserWidget* Widget1 = WidgetLoader->GetOrCreatePooledWidget(WidgetClass);
	TestNotNull("Widget1 should not be null", Widget1);
	
	// Return to pool
	WidgetLoader->ReleaseWidgetToPool(Widget1);
	
	// Get another widget, should be the same instance
	UUserWidget* Widget2 = WidgetLoader->GetOrCreatePooledWidget(WidgetClass);
	TestNotNull("Widget2 should not be null", Widget2);
	TestEqual("Widget2 should be same as Widget1", Widget2, Widget1);
	
	// Get another widget without releasing, should be different
	UUserWidget* Widget3 = WidgetLoader->GetOrCreatePooledWidget(WidgetClass);
	TestNotNull("Widget3 should not be null", Widget3);
	TestNotEqual("Widget3 should be different from Widget1", Widget3, Widget1);
	
	// Preallocate some widgets
	int32 NumToPreallocate = 5;
	UMockWidgetRequestHandler* Handler = NewObject<UMockWidgetRequestHandler>(GameInstance);
	TSoftClassPtr<UUserWidget> WidgetClassRef(WidgetClass);
	
	WidgetLoader->PreallocateWidgets(WidgetClassRef, NumToPreallocate, Handler, 1.0f);
	
	// Wait a bit for preallocations
	FAsyncWidgetLoaderTestHelper Helper;
	Helper.WaitUntil([&]() {
		return FPlatformTime::Seconds() > FPlatformTime::Seconds() + 1.0f;
	});
	
	// Reset the pools
	WidgetLoader->ResetWidgetPools();
	
	// Widgets should be released
	UUserWidget* Widget4 = WidgetLoader->GetOrCreatePooledWidget(WidgetClass);
	TestNotNull("Widget4 should not be null", Widget4);
	TestNotEqual("Widget4 should be different from previous widgets after reset", Widget4, Widget1);
	TestNotEqual("Widget4 should be different from previous widgets after reset", Widget4, Widget3);
	
	return true;
}

/**
 * Test case for request cancellation and cleanup
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncWidgetLoaderCleanupTest, "Game.UI.AsyncWidgetLoader.Cleanup", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAsyncWidgetLoaderCleanupTest::RunTest(const FString& Parameters)
{
	// Get the game instance
	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	if (!TestNotNull("GameInstance should exist", GameInstance))
	{
		return false;
	}

	// Get the subsystem
	UAsyncWidgetLoaderSubsystem* WidgetLoader = GameInstance->GetSubsystem<UAsyncWidgetLoaderSubsystem>();
	if (!TestNotNull("AsyncWidgetLoaderSubsystem should exist", WidgetLoader))
	{
		return false;
	}

	// Create a handler in a local scope so it gets destroyed
	{
		UMockWidgetRequestHandler* TempHandler = NewObject<UMockWidgetRequestHandler>(GameInstance);
		
		// Make a request with a handler that will go away
		const TSubclassOf<UUserWidget> HardWidgetClass = UMockUserWidget::StaticClass();
		const TSoftClassPtr<UUserWidget> WidgetClass(HardWidgetClass);
		
		int32 RequestId = WidgetLoader->RequestWidgetAsync(
			WidgetClass,
			TempHandler,
			FOnAsyncWidgetLoadedDynamic(),
			1.0f
		);
		
		// Verify request was made
		TestTrue("RequestId should be valid", RequestId != INDEX_NONE);
		TestEqual("Request status should be Loading", WidgetLoader->GetRequestStatus(RequestId), EAsyncWidgetLoadStatus::Loading);
		
		// TempHandler is about to be destroyed when we exit this scope
	}
	
	// Manually trigger cleanup
	// This is normally called on a timer, but we'll call it directly for testing
	WidgetLoader->CleanupRequests();
	
	// Create a persistent handler
	UMockWidgetRequestHandler* Handler = NewObject<UMockWidgetRequestHandler>(GameInstance);
	
	// Create a request
	const TSubclassOf<UUserWidget> HardWidgetClass = UMockUserWidget::StaticClass();
	const TSoftClassPtr<UUserWidget> WidgetClass(HardWidgetClass);

	const int32 RequestId = WidgetLoader->RequestWidgetAsync(
		WidgetClass,
		Handler,
		FOnAsyncWidgetLoadedDynamic(),
		1.0f
	);
	
	// Cancel the request
	bool CancelResult = WidgetLoader->CancelRequest(RequestId);
	TestTrue("Cancel should succeed", CancelResult);
	TestEqual("Request status after cancel should be NotStarted", WidgetLoader->GetRequestStatus(RequestId), EAsyncWidgetLoadStatus::NotStarted);
	
	// Verify interface callback
	TestEqual("OnAsyncWidgetCancelled should be called", Handler->CancelledCount, 1);
	TestEqual("LastRequestId should match", Handler->LastRequestId, RequestId);
	TestEqual("LastWidgetClass should match", Handler->LastWidgetClass, WidgetClass);
	
	return true;
}