#include "AsyncWidgetPoolManager.h"
#include "AsyncWidgetLoaderSubsystem.h"

#include <Blueprint/UserWidget.h>
#include <Blueprint/UserWidgetPool.h>
#include <Engine/World.h>
#include <GameFramework/PlayerController.h>

DEFINE_LOG_CATEGORY_STATIC(LogAsyncWidgetPoolManager, Log, All);

UAsyncWidgetPoolManager::UAsyncWidgetPoolManager()
{
}

void UAsyncWidgetPoolManager::Initialize(UWorld* InWorld, APlayerController* InPlayerController)
{
	World = InWorld;
	PlayerController = InPlayerController;

	// Initialize the subsystem with the same context
	if (UAsyncWidgetLoaderSubsystem* Subsystem = GetAsyncWidgetLoader())
	{
		Subsystem->SetCreationContext(InWorld, InPlayerController);
	}

	// Update all local pools
	for (auto& Pair : LocalPools)
	{
		Pair.Value.SetWorld(InWorld);
		Pair.Value.SetDefaultPlayerController(InPlayerController);
	}
}

void UAsyncWidgetPoolManager::PreallocateWidgets(TSoftClassPtr<UUserWidget> WidgetClass, int32 NumToPreallocate, bool bAddToGlobalPool)
{
	if (!WidgetClass.IsValid())
	{
		UE_LOG(LogAsyncWidgetPoolManager, Error, TEXT("PreallocateWidgets: Invalid widget class"));
		return;
	}

	if (NumToPreallocate <= 0)
	{
		UE_LOG(LogAsyncWidgetPoolManager, Warning, TEXT("PreallocateWidgets: NumToPreallocate must be > 0"));
		return;
	}

	// If the class isn't loaded yet, we can't preallocate
	if (!WidgetClass.Get())
	{
		UE_LOG(LogAsyncWidgetPoolManager, Warning, TEXT("PreallocateWidgets: Class not loaded, can't preallocate: %s"), *WidgetClass.ToString());
		return;
	}

	// Use subsystem pool or local pool
	if (bAddToGlobalPool)
	{
		UAsyncWidgetLoaderSubsystem* Subsystem = GetAsyncWidgetLoader();
		if (!Subsystem)
		{
			UE_LOG(LogAsyncWidgetPoolManager, Error, TEXT("PreallocateWidgets: Failed to get subsystem"));
			return;
		}

		// Preallocate widgets
		TArray<UUserWidget*> PreallocatedWidgets;
		for (int32 i = 0; i < NumToPreallocate; ++i)
		{
			UUserWidget* Widget = Subsystem->GetPooledWidget(WidgetClass.Get(), WidgetClass.ToSoftObjectPath());
			if (Widget)
			{
				PreallocatedWidgets.Add(Widget);
			}
		}

		// Release widgets back to pool
		for (UUserWidget* Widget : PreallocatedWidgets)
		{
			Subsystem->ReleaseWidget(Widget, WidgetClass.ToSoftObjectPath(), true);
		}
	}
	else
	{
		// Use local pool
		FUserWidgetPool& Pool = GetOrCreateLocalPool(WidgetClass.ToSoftObjectPath());
		
		// Preallocate widgets
		TArray<UUserWidget*> PreallocatedWidgets;
		for (int32 i = 0; i < NumToPreallocate; ++i)
		{
			if (UUserWidget* Widget = Pool.GetOrCreateInstance<UUserWidget>(WidgetClass.Get()))
			{
				PreallocatedWidgets.Add(Widget);
			}
		}

		// Release widgets back to pool
		for (UUserWidget* Widget : PreallocatedWidgets)
		{
			Pool.Release(Widget);
		}
	}
}

UUserWidget* UAsyncWidgetPoolManager::GetWidget(TSoftClassPtr<UUserWidget> WidgetClass)
{
	if (!WidgetClass.IsValid())
	{
		UE_LOG(LogAsyncWidgetPoolManager, Error, TEXT("GetWidget: Invalid widget class"));
		return nullptr;
	}

	// If the class isn't loaded yet, we can't get a widget
	if (!WidgetClass.Get())
	{
		UE_LOG(LogAsyncWidgetPoolManager, Warning, TEXT("GetWidget: Class not loaded: %s"), *WidgetClass.ToString());
		return nullptr;
	}

	// Try local pool first
	const FString PathString = WidgetClass.ToSoftObjectPath().ToString();
	if (FUserWidgetPool* LocalPool = LocalPools.Find(PathString))
	{
		return LocalPool->GetOrCreateInstance<UUserWidget>(WidgetClass.Get());
	}

	// Use subsystem
	UAsyncWidgetLoaderSubsystem* Subsystem = GetAsyncWidgetLoader();
	if (!Subsystem)
	{
		UE_LOG(LogAsyncWidgetPoolManager, Error, TEXT("GetWidget: Failed to get subsystem"));
		return nullptr;
	}

	return Subsystem->GetPooledWidget(WidgetClass.Get(), WidgetClass.ToSoftObjectPath());
}

void UAsyncWidgetPoolManager::ReleaseAllWidgets()
{
	// Release all widgets in local pools
	for (auto& Pair : LocalPools)
	{
		Pair.Value.ResetPool();
	}

	// Use subsystem for global pools
	UAsyncWidgetLoaderSubsystem* Subsystem = GetAsyncWidgetLoader();
	if (!Subsystem)
	{
		UE_LOG(LogAsyncWidgetPoolManager, Error, TEXT("ReleaseAllWidgets: Failed to get subsystem"));
		return;
	}

	Subsystem->ReleaseAllWidgets();
}

void UAsyncWidgetPoolManager::SetMaxPoolSize(TSoftClassPtr<UUserWidget> WidgetClass, int32 MaxSize)
{
	if (!WidgetClass.IsValid())
	{
		UE_LOG(LogAsyncWidgetPoolManager, Error, TEXT("SetMaxPoolSize: Invalid widget class"));
		return;
	}

	const FString PathString = WidgetClass.ToSoftObjectPath().ToString();
	MaxPoolSizes.Add(PathString, MaxSize);
}

bool UAsyncWidgetPoolManager::IsClassLoaded(TSoftClassPtr<UUserWidget> WidgetClass) const
{
	if (!WidgetClass.IsValid())
	{
		return false;
	}

	return WidgetClass.Get() != nullptr;
}

UAsyncWidgetLoaderSubsystem* UAsyncWidgetPoolManager::GetAsyncWidgetLoader()
{
	if (AsyncWidgetLoader)
	{
		return AsyncWidgetLoader;
	}

	// Find the game instance
	UGameInstance* GameInstance = nullptr;
	if (World.IsValid())
	{
		GameInstance = World->GetGameInstance();
	}
	else if (PlayerController.IsValid())
	{
		GameInstance = PlayerController->GetGameInstance();
	}
	else
	{
		// Try to get it from current world
		if (const UWorld* CurrentWorld = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World())
		{
			GameInstance = CurrentWorld->GetGameInstance();
		}
	}

	if (!GameInstance)
	{
		return nullptr;
	}

	AsyncWidgetLoader = GameInstance->GetSubsystem<UAsyncWidgetLoaderSubsystem>();
	return AsyncWidgetLoader;
}

FUserWidgetPool& UAsyncWidgetPoolManager::GetOrCreateLocalPool(const FSoftObjectPath& ClassPath)
{
	const FString PathString = ClassPath.ToString();
	if (FUserWidgetPool* ExistingPool = LocalPools.Find(PathString))
	{
		return *ExistingPool;
	}

	// Create a new pool
	FUserWidgetPool& NewPool = LocalPools.Add(PathString);
	if (World.IsValid() && PlayerController.IsValid())
	{
		NewPool.SetWorld(World.Get());
		NewPool.SetDefaultPlayerController(PlayerController.Get());
	}

	return NewPool;
}