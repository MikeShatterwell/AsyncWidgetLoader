// Copyright Mike Desrosiers 2025, All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

class UAsyncWidgetLoaderSubsystem;

/**
 * A handle to an asynchronously loaded widget
 * Manages widget lifecycle and provides release functionality
 */
UCLASS(BlueprintType)
class ASYNCWIDGETLOADER_API UAsyncWidgetHandle : public UObject
{
	GENERATED_BODY()

public:
	UAsyncWidgetHandle();
	virtual ~UAsyncWidgetHandle() override;

	virtual void BeginDestroy() override;

	/** Initialize this handle with a widget and owner */
	void Initialize(UUserWidget* InWidget, UAsyncWidgetLoaderSubsystem* InOwner, const FSoftObjectPath& InClassPath);

	/** Get the widget managed by this handle */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	UUserWidget* GetWidget() const { return Widget; }

	/** Release the widget back to the pool */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	void Release();

	/** Get the class path of the original widget class */
	const FSoftObjectPath& GetClassPath() const { return OriginalClassPath; }

	/** Check if this handle has a valid widget */
	UFUNCTION(BlueprintCallable, Category = "Async Widget Loader")
	bool IsValid() const;

protected:
	/** The widget this handle manages */
	UPROPERTY()
	TObjectPtr<UUserWidget> Widget;

	/** The subsystem that owns this handle */
	UPROPERTY()
	TObjectPtr<UAsyncWidgetLoaderSubsystem> OwningSubsystem;

	/** The original class path for pooling */
	FSoftObjectPath OriginalClassPath;

	/** Whether we've been released already */
	bool bReleased;
};