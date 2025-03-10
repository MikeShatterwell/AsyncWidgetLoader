#include "AsyncWidgetLoaderSubsystem.h"
#include "AsyncWidgetHandle.h"
#include "Interfaces/IAsyncWidgetLoadingPlaceholder.h"
#include "Interfaces/IAsyncWidgetRequestHandler.h"
#include <Engine/World.h>
#include <GameFramework/PlayerController.h>
#include <Blueprint/UserWidget.h>
#include <Misc/ScopeLock.h>
#include <Containers/Ticker.h>
#include <TimerManager.h>
#include <Blueprint/UserWidgetPool.h>
#include <Stats/Stats.h>

DEFINE_LOG_CATEGORY_STATIC(LogAsyncWidgetLoader, Log, All);

UAsyncWidgetLoaderSubsystem::UAsyncWidgetLoaderSubsystem()
	: NextRequestId(1)
	, WidgetReleaseDelay(0.5f)
{
}

void UAsyncWidgetLoaderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Register ticker for cleanup and processing
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UAsyncWidgetLoaderSubsystem::Tick),
		0.1f);
}

void UAsyncWidgetLoaderSubsystem::Deinitialize()
{
	// Clean up ticker
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

	// Cancel all pending requests
	TArray<int32> RequestIds;
	ActiveRequests.GetKeys(RequestIds);
	for (int32 RequestId : RequestIds)
	{
		CancelRequest(RequestId);
	}

	// Release all widgets
	ReleaseAllWidgets();

	Super::Deinitialize();
}

bool UAsyncWidgetLoaderSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create on game instance
	return Cast<UGameInstance>(Outer) != nullptr;
}

int32 UAsyncWidgetLoaderSubsystem::RequestWidget(
	const TSoftClassPtr<UUserWidget>& WidgetClass,
	UObject* Requester,
	const FOnAsyncWidgetLoaded& OnLoadCompleted,
	const float Priority,
	const int32 UserData,
	const bool bAddToPool)
{
	if (!Requester)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("RequestWidget: Invalid requester"));
		return INDEX_NONE;
	}

	if (!WidgetClass.IsValid())
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("RequestWidget: Invalid widget class"));
		return INDEX_NONE;
	}

	// Check if the class is already loaded
	if (WidgetClass.Get())
	{
		// Create the widget immediately
		UUserWidget* Widget = nullptr;
		if (bAddToPool)
		{
			Widget = GetPooledWidget(WidgetClass.Get(), WidgetClass.ToSoftObjectPath());
		}
		else
		{
			// Create a new instance
			Widget = CreateWidget<UUserWidget>(DefaultPlayerController.Get(), WidgetClass.Get());
		}

		if (Widget)
		{
			// Call the completion callback
			OnLoadCompleted.ExecuteIfBound(NextRequestId, Widget);

			// Notify via interface if implemented
			if (Requester->Implements<UAsyncWidgetRequestHandler>())
			{
				IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoaded(Requester, NextRequestId, Widget, UserData);
			}

			// Increment request ID for next request
			return NextRequestId++;
		}
	}

	// Create a new request
	FAsyncWidgetRequest& Request = ActiveRequests.Add(NextRequestId);
	Request.RequestId = NextRequestId;
	Request.ClassPath = WidgetClass.ToSoftObjectPath();
	Request.WidgetClass = WidgetClass;
	Request.Requester = Requester;
	Request.OnLoadCompleted = OnLoadCompleted;
	Request.Priority = Priority;
	Request.RequestTime = FPlatformTime::Seconds();
	Request.Status = EAsyncWidgetLoadStatus::Loading;
	Request.bAddToPool = bAddToPool;
	Request.UserData = UserData;

	// Notify via interface if implemented
	if (Requester->Implements<UAsyncWidgetRequestHandler>())
	{
		IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetRequested(Requester, NextRequestId, WidgetClass, UserData);
	}

	// Start async loading
	TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
		Request.ClassPath,
		[this, RequestId = NextRequestId]() { OnWidgetClassLoaded(RequestId); },
		Priority);

	Request.StreamableHandle = Handle;

	// Increment request ID for next request
	return NextRequestId++;
}

int32 UAsyncWidgetLoaderSubsystem::RequestWidget(
	const TSoftClassPtr<UUserWidget> WidgetClass,
	UObject* Requester,
	const FOnAsyncWidgetLoadedDynamic OnLoadCompleted,
	const float Priority,
	const int32 UserData)
{
	if (!Requester)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("RequestWidgetBP: Invalid requester"));
		return INDEX_NONE;
	}

	if (!WidgetClass.IsValid())
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("RequestWidgetBP: Invalid widget class"));
		return INDEX_NONE;
	}

	// Check if the class is already loaded
	if (WidgetClass.Get())
	{
		// Create the widget immediately
		if (UUserWidget* Widget = GetPooledWidget(WidgetClass.Get(), WidgetClass.ToSoftObjectPath()))
		{
			// Call the completion callback
			OnLoadCompleted.ExecuteIfBound(NextRequestId, Widget);

			// Notify via interface if implemented
			if (Requester->Implements<UAsyncWidgetRequestHandler>())
			{
				IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoaded(Requester, NextRequestId, Widget, UserData);
			}

			// Increment request ID for next request
			return NextRequestId++;
		}
	}

	// Create a new request
	FAsyncWidgetRequest& Request = ActiveRequests.Add(NextRequestId);
	Request.RequestId = NextRequestId;
	Request.ClassPath = WidgetClass.ToSoftObjectPath();
	Request.WidgetClass = WidgetClass;
	Request.Requester = Requester;
	Request.OnLoadCompletedBP = OnLoadCompleted;
	Request.Priority = Priority;
	Request.RequestTime = FPlatformTime::Seconds();
	Request.Status = EAsyncWidgetLoadStatus::Loading;
	Request.UserData = UserData;

	// Notify via interface if implemented
	if (Requester->Implements<UAsyncWidgetRequestHandler>())
	{
		IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetRequested(Requester, NextRequestId, WidgetClass, UserData);
	}

	// Start async loading
	TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
		Request.ClassPath,
		[this, RequestId = NextRequestId]() { OnWidgetClassLoaded(RequestId); },
		Priority);

	Request.StreamableHandle = Handle;

	// Increment request ID for next request
	return NextRequestId++;
}

UUserWidget* UAsyncWidgetLoaderSubsystem::CreatePlaceholderWidget(
	TSoftClassPtr<UUserWidget> WidgetClass,
	UObject* Requester,
	UObject* Context)
{
	if (!DefaultPlaceholderClass.IsValid() && !DefaultPlaceholderClass.Get())
	{
		return nullptr;
	}

	// Create placeholder widget
	UUserWidget* PlaceholderWidget = CreateWidget<UUserWidget>(DefaultPlayerController.Get(), DefaultPlaceholderClass.Get());
	if (!PlaceholderWidget)
	{
		return nullptr;
	}

	// Initialize if it implements the placeholder interface
	if (PlaceholderWidget->Implements<UAsyncWidgetLoadingPlaceholder>())
	{
		IAsyncWidgetLoadingPlaceholder::Execute_InitializePlaceholder(PlaceholderWidget, WidgetClass, Context);
	}

	return PlaceholderWidget;
}

void UAsyncWidgetLoaderSubsystem::SetDefaultPlaceholderClass(TSoftClassPtr<UUserWidget> PlaceholderClass)
{
	DefaultPlaceholderClass = PlaceholderClass;
}

bool UAsyncWidgetLoaderSubsystem::CancelRequest(int32 RequestId)
{
	FAsyncWidgetRequest* Request = ActiveRequests.Find(RequestId);
	if (!Request)
	{
		UE_LOG(LogAsyncWidgetLoader, Warning, TEXT("CancelRequest: Request %d not found"), RequestId);
		return false;
	}

	// Cancel the streamable handle
	Request->Cancel();

	// Notify the requester if it implements the interface
	if (Request->IsRequesterValid() && Request->Requester->Implements<UAsyncWidgetRequestHandler>())
	{
		IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoadCancelled(
			Request->Requester.Get(), RequestId, Request->WidgetClass, Request->UserData);
	}

	// Release placeholder widget if any
	if (Request->PlaceholderWidget.IsValid())
	{
		Request->PlaceholderWidget.Reset();
	}

	// Remove from active requests
	ActiveRequests.Remove(RequestId);

	return true;
}

EAsyncWidgetLoadStatus UAsyncWidgetLoaderSubsystem::GetRequestStatus(int32 RequestId) const
{
	const FAsyncWidgetRequest* Request = ActiveRequests.Find(RequestId);
	if (!Request)
	{
		return EAsyncWidgetLoadStatus::NotStarted;
	}

	return Request->Status;
}

UAsyncWidgetHandle* UAsyncWidgetLoaderSubsystem::CreateWidgetHandle(UUserWidget* Widget, FSoftObjectPath ClassPath)
{
	if (!Widget)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("CreateWidgetHandle: Invalid widget"));
		return nullptr;
	}

	UAsyncWidgetHandle* Handle = NewObject<UAsyncWidgetHandle>(this);
	Handle->Initialize(Widget, this, ClassPath);
	return Handle;
}

void UAsyncWidgetLoaderSubsystem::SetCreationContext(UWorld* World, APlayerController* PlayerController)
{
	DefaultWorld = World;
	DefaultPlayerController = PlayerController;

	// Update all pools with the new context
	for (auto& Pair : ClassToPoolMap)
	{
		Pair.Value.SetWorld(World);
		Pair.Value.SetDefaultPlayerController(PlayerController);
	}
}

void UAsyncWidgetLoaderSubsystem::ReleaseWidget(UUserWidget* Widget, const FSoftObjectPath& ClassPath, bool bImmediate)
{
	if (!Widget)
	{
		return;
	}

	if (bImmediate)
	{
		// Get the pool and release immediately
		FUserWidgetPool& Pool = GetOrCreatePool(ClassPath);
		Pool.Release(Widget);
	}
	else
	{
		// Schedule for delayed release
		FDelayedWidgetRelease DelayedRelease(Widget, ClassPath, FPlatformTime::Seconds() + WidgetReleaseDelay);
		DelayedReleases.Add(DelayedRelease);
	}
}

void UAsyncWidgetLoaderSubsystem::ReleaseAllWidgets()
{
	// Clear all pools
	for (auto& Pair : ClassToPoolMap)
	{
		Pair.Value.ResetPool();
	}

	// Clear delayed releases
	DelayedReleases.Empty();
}

UUserWidget* UAsyncWidgetLoaderSubsystem::GetPooledWidget(const TSubclassOf<UUserWidget>& WidgetClass, const FSoftObjectPath& ClassPath)
{
	if (!WidgetClass)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("GetPooledWidget: Invalid widget class"));
		return nullptr;
	}

	// Get or create the pool
	FUserWidgetPool& Pool = GetOrCreatePool(ClassPath);

	// Initialize pool if needed
	if (!Pool.IsInitialized())
	{
		if (!DefaultWorld.IsValid() || !DefaultPlayerController.IsValid())
		{
			UE_LOG(LogAsyncWidgetLoader, Error, TEXT("GetPooledWidget: Pool not initialized and no default context available"));
			return nullptr;
		}

		Pool.SetWorld(DefaultWorld.Get());
		Pool.SetDefaultPlayerController(DefaultPlayerController.Get());
	}

	// Get a widget from the pool
	return Pool.GetOrCreateInstance(WidgetClass);
}

void UAsyncWidgetLoaderSubsystem::OnWidgetClassLoaded(int32 RequestId)
{
	FAsyncWidgetRequest* Request = ActiveRequests.Find(RequestId);
	if (!Request)
	{
		UE_LOG(LogAsyncWidgetLoader, Warning, TEXT("OnWidgetClassLoaded: Request %d not found"), RequestId);
		return;
	}

	// Mark as completed
	Request->Status = EAsyncWidgetLoadStatus::Completed;

	// Check if the requester is still valid
	if (!Request->IsRequesterValid())
	{
		UE_LOG(LogAsyncWidgetLoader, Warning, TEXT("OnWidgetClassLoaded: Requester for request %d is no longer valid"), RequestId);
		ActiveRequests.Remove(RequestId);
		return;
	}

	// Get the loaded class
	UClass* LoadedClass = Cast<UClass>(Request->StreamableHandle->GetLoadedAsset());
	if (!LoadedClass)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("OnWidgetClassLoaded: Failed to load class for request %d"), RequestId);
		Request->Status = EAsyncWidgetLoadStatus::Failed;

		// Notify failure
		if (Request->Requester->Implements<UAsyncWidgetRequestHandler>())
		{
			IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoadFailed(
				Request->Requester.Get(), RequestId, Request->WidgetClass, Request->UserData);
		}

		ActiveRequests.Remove(RequestId);
		return;
	}

	// Create the widget
	UUserWidget* Widget = nullptr;
	if (Request->bAddToPool)
	{
		Widget = GetPooledWidget(LoadedClass, Request->ClassPath);
	}
	else
	{
		// Create a new instance
		Widget = CreateWidget<UUserWidget>(DefaultPlayerController.Get(), LoadedClass);
	}

	if (!Widget)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("OnWidgetClassLoaded: Failed to create widget for request %d"), RequestId);
		Request->Status = EAsyncWidgetLoadStatus::Failed;

		// Notify failure
		if (Request->Requester->Implements<UAsyncWidgetRequestHandler>())
		{
			IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoadFailed(
				Request->Requester.Get(), RequestId, Request->WidgetClass, Request->UserData);
		}

		ActiveRequests.Remove(RequestId);
		return;
	}

	// Prepare the placeholder for replacement if any
	if (Request->PlaceholderWidget.IsValid())
	{
		if (Request->PlaceholderWidget->Implements<UAsyncWidgetLoadingPlaceholder>())
		{
			IAsyncWidgetLoadingPlaceholder::Execute_PrepareForReplacement(Request->PlaceholderWidget.Get());
		}
	}

	// Call the completion callback
	if (Request->OnLoadCompleted.IsBound())
	{
		Request->OnLoadCompleted.Execute(RequestId, Widget);
	}
	else if (Request->OnLoadCompletedBP.IsBound())
	{
		Request->OnLoadCompletedBP.Execute(RequestId, Widget);
	}

	// Notify the requester if it implements the interface
	if (Request->Requester->Implements<UAsyncWidgetRequestHandler>())
	{
		IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoaded(
			Request->Requester.Get(), RequestId, Widget, Request->UserData);
	}

	// Remove from active requests
	ActiveRequests.Remove(RequestId);
}

void UAsyncWidgetLoaderSubsystem::CleanupRequests()
{
	// Find requests with invalid requesters or cancelled handles
	TArray<int32> RequestsToRemove;
	for (auto& Pair : ActiveRequests)
	{
		FAsyncWidgetRequest& Request = Pair.Value;

		// Check if requester is still valid
		if (!Request.IsRequesterValid())
		{
			UE_LOG(LogAsyncWidgetLoader, Verbose, TEXT("%hs: Removing request %d with invalid requester"), __FUNCTION__, Request.RequestId);
			RequestsToRemove.Add(Request.RequestId);
			continue;
		}

		// Check if the streamable handle is valid and loading
		if (Request.StreamableHandle.IsValid())
		{
			if (Request.Status == EAsyncWidgetLoadStatus::Loading &&
				Request.StreamableHandle->HasLoadCompleted())
			{
				// Loading completed through the streamable manager, but our callback hasn't been called yet
				// This can happen if the handle completed on a background thread
				UE_LOG(LogAsyncWidgetLoader, Verbose, TEXT("%hs: Handle for request %d completed, but callback not yet called"), __FUNCTION__, Request.RequestId);
				continue;
			}

			if (Request.Status == EAsyncWidgetLoadStatus::Cancelled ||
				Request.StreamableHandle->WasCanceled())
			{
				UE_LOG(LogAsyncWidgetLoader, Verbose, TEXT("%hs: Removing cancelled request %d"), __FUNCTION__, Request.RequestId);
				RequestsToRemove.Add(Request.RequestId);
				continue;
			}
		}
	}

	// Remove invalid requests
	for (int32 RequestId : RequestsToRemove)
	{
		FAsyncWidgetRequest* Request = ActiveRequests.Find(RequestId);
		if (Request)
		{
			// Cancel the request
			Request->Cancel();

			// Release placeholder widget if any
			if (Request->PlaceholderWidget.IsValid())
			{
				Request->PlaceholderWidget.Reset();
			}

			ActiveRequests.Remove(RequestId);
		}
	}
}

void UAsyncWidgetLoaderSubsystem::ProcessDelayedReleases()
{
	if (DelayedReleases.Num() == 0)
	{
		return;
	}

	// Get current time
	const double CurrentTime = FPlatformTime::Seconds();

	// Find releases to process
	TArray<int32> ReleasesToProcess;
	for (int32 i = 0; i < DelayedReleases.Num(); ++i)
	{
		const FDelayedWidgetRelease& DelayedRelease = DelayedReleases[i];
		if (CurrentTime >= DelayedRelease.ReleaseTime)
		{
			ReleasesToProcess.Add(i);
		}
	}

	// Process releases
	for (int32 i = ReleasesToProcess.Num() - 1; i >= 0; --i)
	{
		const int32 Index = ReleasesToProcess[i];
		const FDelayedWidgetRelease& DelayedRelease = DelayedReleases[Index];

		// Release the widget
		if (DelayedRelease.Widget)
		{
			ReleaseWidget(DelayedRelease.Widget, DelayedRelease.ClassPath, true);
		}

		// Remove from array
		DelayedReleases.RemoveAt(Index);
	}
}

FUserWidgetPool& UAsyncWidgetLoaderSubsystem::GetOrCreatePool(const FSoftObjectPath& ClassPath)
{
	const FString PathString = ClassPath.ToString();
	FUserWidgetPool* ExistingPool = ClassToPoolMap.Find(PathString);
	if (ExistingPool)
	{
		return *ExistingPool;
	}

	// Create a new pool
	FUserWidgetPool& NewPool = ClassToPoolMap.Add(PathString);
	if (DefaultWorld.IsValid() && DefaultPlayerController.IsValid())
	{
		NewPool.SetWorld(DefaultWorld.Get());
		NewPool.SetDefaultPlayerController(DefaultPlayerController.Get());
	}

	return NewPool;
}

bool UAsyncWidgetLoaderSubsystem::Tick(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AsyncWidgetLoaderSubsystem_Tick);

	// Process delayed releases
	ProcessDelayedReleases();

	// Clean up invalid requests
	CleanupRequests();

	return true;
}