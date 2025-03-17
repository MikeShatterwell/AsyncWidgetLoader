#include "AsyncWidgetLoaderSubsystem.h"

#include <Blueprint/UserWidget.h>
#include <Blueprint/UserWidgetPool.h>
#include <Containers/Ticker.h>
#include <Engine/World.h>
#include <GameFramework/PlayerController.h>
#include <Misc/ScopeLock.h>
#include <Stats/Stats.h>
#include <TimerManager.h>

#include "LogAsyncWidgetLoader.h"
#include "Interfaces/IAsyncWidgetRequestHandler.h"

UAsyncWidgetLoaderSubsystem::UAsyncWidgetLoaderSubsystem()
	: NextRequestId(1)
{
}

void UAsyncWidgetLoaderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	TimerManager.SetTimer(
		CleanupTimerHandle,
		this,
		&ThisClass::CleanupRequests,
		CleanupInterval,
		true
	);
}

void UAsyncWidgetLoaderSubsystem::Deinitialize()
{
	// Cancel all pending requests
	TArray<int32> RequestIds;
	ActiveRequests.GetKeys(RequestIds);
	for (const int32 RequestId : RequestIds)
	{
		CancelRequest(RequestId);
	}

	ResetWidgetPools();

	Super::Deinitialize();
}

bool UAsyncWidgetLoaderSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create on game instance
	return Cast<UGameInstance>(Outer) != nullptr;
}

int32 UAsyncWidgetLoaderSubsystem::RequestWidgetAsync(
	const TSoftClassPtr<UUserWidget>& WidgetClass,
	UObject* Requester,
	const FOnAsyncWidgetLoadedDynamic& OnLoadCompleted,
	const float Priority)
{
	if (!Requester)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("%hs: Invalid requester"), __FUNCTION__);
		return INDEX_NONE;
	}

	if (!WidgetClass.IsValid())
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("%hs: Invalid widget class"), __FUNCTION__);
		return INDEX_NONE;
	}
	
	const int32 CurrentRequestId = NextRequestId++;

	// Check if the class is already loaded
	/*if (UClass* LoadedClass = WidgetClass.Get())
	{
		// Create the widget immediately
		if (UUserWidget* Widget = GetOrCreatePooledWidget(LoadedClass))
		{
			// Call the completion callback right away
			OnLoadCompleted.ExecuteIfBound(CurrentRequestId, Widget);

			// Notify via interface if implemented
			if (Requester && Requester->Implements<UAsyncWidgetRequestHandler>())
			{
				IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoaded(Requester, CurrentRequestId, Widget);
			}

			return CurrentRequestId;
		}
	}*/

	// Widget class not already loaded, start async loading

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

	// Notify via interface if implemented
	if (Requester->Implements<UAsyncWidgetRequestHandler>())
	{
		IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetRequested(Requester, CurrentRequestId, WidgetClass);
	}

	// Start async loading
	const TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
		Request.ClassPath,
		[this, RequestId = CurrentRequestId]()
		{
			OnWidgetClassLoaded(RequestId);
		},
		Priority);

	Request.StreamableHandle = Handle;

	return CurrentRequestId;
}

bool UAsyncWidgetLoaderSubsystem::CancelRequest(const int32 RequestId)
{
	FAsyncWidgetRequest* Request = ActiveRequests.Find(RequestId);
	if (!Request)
	{
		// If we can't find it in ActiveRequests, it might be an immediate request that already completed
		// In which case, there's nothing to cancel - just return success
		if (RequestId > 0 && RequestId < NextRequestId)
		{
			return true;
		}

		UE_LOG(LogAsyncWidgetLoader, Warning, TEXT("CancelRequest: Request %d not found"), RequestId);
		return false;
	}

	// Cancel the streamable handle
	Request->Cancel();

	// Notify the requester if it implements the interface
	if (Request->IsRequesterValid() && Request->Requester->Implements<UAsyncWidgetRequestHandler>())
	{
		IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoadCancelled(Request->Requester.Get(), RequestId, Request->WidgetClass);
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

EAsyncWidgetLoadStatus UAsyncWidgetLoaderSubsystem::GetRequestStatus(const int32 RequestId) const
{
	const FAsyncWidgetRequest* Request = ActiveRequests.Find(RequestId);
	if (!Request)
	{
		// If the request ID is valid but not in ActiveRequests, it might be an immediate load that completed
		// We can check if the ID is within our range of generated IDs
		if (RequestId > 0 && RequestId < NextRequestId)
		{
			return EAsyncWidgetLoadStatus::Completed;
		}
		return EAsyncWidgetLoadStatus::NotStarted;
	}

	return Request->Status;
}

void UAsyncWidgetLoaderSubsystem::SetWidgetCreationContext(UWorld* World, APlayerController* PlayerController)
{
	DefaultWorld = World;
	DefaultPlayerController = PlayerController;

	// Update all pools with the new context
	for (auto& Pair : ClassPathToPoolMap)
	{
		Pair.Value.SetWorld(World);
		Pair.Value.SetDefaultPlayerController(PlayerController);
	}
}

void UAsyncWidgetLoaderSubsystem::ResetWidgetPools()
{
	// Clear all pools
	for (auto& Pair : ClassPathToPoolMap)
	{
		Pair.Value.ResetPool();
	}
}

UUserWidget* UAsyncWidgetLoaderSubsystem::GetOrCreatePooledWidget(const TSubclassOf<UUserWidget>& LoadedWidgetClass)
{
	if (!LoadedWidgetClass)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("GetPooledWidget: Invalid widget class"));
		return nullptr;
	}

	// Get a widget from the pool
	return GetOrCreatePool(LoadedWidgetClass.Get()).GetOrCreateInstance(LoadedWidgetClass);
}

void UAsyncWidgetLoaderSubsystem::OnPreallocatedWidgetLoaded(int32 RequestId, UUserWidget* LoadedWidget)
{
	if (LoadedWidget)
	{
		ReleaseWidgetToPool(LoadedWidget);
	}
}

void UAsyncWidgetLoaderSubsystem::PreallocateWidgets(const TSoftClassPtr<UUserWidget>& WidgetClass,
	const int32 NumToPreallocate, UObject* Requester, const float Priority)
{
	if (!WidgetClass.IsValid())
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("%hs: Invalid widget class"), __FUNCTION__);
		return;
	}

	if (NumToPreallocate <= 0)
	{
		UE_LOG(LogAsyncWidgetLoader, Warning, TEXT("PreallocateWidgets: NumToPreallocate must be > 0"));
		return;
	}

	FOnAsyncWidgetLoadedDynamic OnLoadCompleted;
	OnLoadCompleted.BindDynamic(this, &ThisClass::OnPreallocatedWidgetLoaded);

	for (int32 i = 0; i < NumToPreallocate; ++i)
	{
		RequestWidgetAsync(WidgetClass, Requester, FOnAsyncWidgetLoadedDynamic(), Priority);
	}
}

void UAsyncWidgetLoaderSubsystem::ReleaseWidgetToPool(UUserWidget* Widget)
{
	if (!Widget)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("%hs: Invalid widget"), __FUNCTION__);
		return;
	}

	const FSoftClassPath ClassPath = Widget->GetClass()->GetPathName();
	if (FUserWidgetPool* Pool = ClassPathToPoolMap.Find(ClassPath.ToString()))
	{
		Pool->Release(Widget);
	}
	else
	{
		UE_LOG(LogAsyncWidgetLoader, Warning, TEXT("%hs: Pool not found for widget class %s -- to use this function, ensure the widget you are passing in was initially created via the UAsyncWidgetLoaderSubsystem"), __FUNCTION__, *ClassPath.ToString());
	}
}

void UAsyncWidgetLoaderSubsystem::OnWidgetClassLoaded(const int32 RequestId)
{
	FAsyncWidgetRequest* Request = ActiveRequests.Find(RequestId);
	if (!Request)
	{
		UE_LOG(LogAsyncWidgetLoader, Warning, TEXT("%hs: Request %d not found"), __FUNCTION__, RequestId);
		return;
	}

	// Mark as completed
	Request->Status = EAsyncWidgetLoadStatus::Completed;

	// Check if the requester is still valid
	if (!Request->IsRequesterValid())
	{
		UE_LOG(LogAsyncWidgetLoader, Warning, TEXT("%hs: Requester for request %d is no longer valid"), __FUNCTION__, RequestId);
		ActiveRequests.Remove(RequestId);
		return;
	}

	// Get the loaded class
	UClass* LoadedClass = Cast<UClass>(Request->StreamableHandle->GetLoadedAsset());
	if (!LoadedClass)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("%hs: Failed to load class for request %d"), __FUNCTION__, RequestId);
		Request->Status = EAsyncWidgetLoadStatus::Failed;

		// Notify failure
		if (Request->Requester->Implements<UAsyncWidgetRequestHandler>())
		{
			IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoadFailed(Request->Requester.Get(), RequestId, Request->WidgetClass);
		}

		ActiveRequests.Remove(RequestId);
		return;
	}

	// Create the widget
	UUserWidget* Widget = GetOrCreatePooledWidget(LoadedClass);
	if (!Widget)
	{
		UE_LOG(LogAsyncWidgetLoader, Error, TEXT("%hs: Failed to create widget for request %d"), __FUNCTION__, RequestId);
		Request->Status = EAsyncWidgetLoadStatus::Failed;

		// Notify failure
		if (Request->Requester->Implements<UAsyncWidgetRequestHandler>())
		{
			IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoadFailed(Request->Requester.Get(), RequestId, Request->WidgetClass);
		}

		ActiveRequests.Remove(RequestId);
		return;
	}

	// Call the completion callback
	if (Request->OnLoadCompleted.IsBound())
	{
		Request->OnLoadCompleted.Execute(RequestId, Widget);
	}
	else if (Request->OnLoadCompleted.IsBound())
	{
		Request->OnLoadCompleted.Execute(RequestId, Widget);
	}

	// Notify the requester if it implements the interface
	if (Request->Requester->Implements<UAsyncWidgetRequestHandler>())
	{
		IAsyncWidgetRequestHandler::Execute_OnAsyncWidgetLoaded(Request->Requester.Get(), RequestId, Widget);
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
	for (const int32 RequestId : RequestsToRemove)
	{
		if (FAsyncWidgetRequest* Request = ActiveRequests.Find(RequestId))
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

FUserWidgetPool& UAsyncWidgetLoaderSubsystem::GetOrCreatePool(const TSoftClassPtr<UUserWidget>& ClassPath)
{
	const FString PathString = ClassPath.ToString();
	if (FUserWidgetPool* ExistingPool = ClassPathToPoolMap.Find(PathString))
	{
		return *ExistingPool;
	}

	// Create a new pool
	FUserWidgetPool& NewPool = ClassPathToPoolMap.Add(PathString);
	if (DefaultWorld.IsValid() && DefaultPlayerController.IsValid())
	{
		NewPool.SetWorld(DefaultWorld.Get());
		NewPool.SetDefaultPlayerController(DefaultPlayerController.Get());
	}

	return NewPool;
}