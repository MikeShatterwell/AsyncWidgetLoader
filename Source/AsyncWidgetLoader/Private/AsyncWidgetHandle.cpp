// Copyright 2025, All Rights Reserved.

#include "AsyncWidgetHandle.h"
#include "AsyncWidgetLoaderSubsystem.h"

UAsyncWidgetHandle::UAsyncWidgetHandle()
	: Widget(nullptr)
	, OwningSubsystem(nullptr)
	, bReleased(false)
{
}

UAsyncWidgetHandle::~UAsyncWidgetHandle()
{
	// Make sure we release our widget
	if (!bReleased && IsValid())
	{
		Release();
	}
}

void UAsyncWidgetHandle::BeginDestroy()
{
	// Auto-release when destroyed
	if (!bReleased && IsValid())
	{
		Release();
	}
    
	Super::BeginDestroy();
}

void UAsyncWidgetHandle::Initialize(UUserWidget* InWidget, UAsyncWidgetLoaderSubsystem* InOwner, const FSoftObjectPath& InClassPath)
{
	Widget = InWidget;
	OwningSubsystem = InOwner;
	OriginalClassPath = InClassPath;
	bReleased = false;
}

void UAsyncWidgetHandle::Release()
{
	if (bReleased || !IsValid())
	{
		return;
	}

	bReleased = true;

	if (OwningSubsystem && Widget)
	{
		OwningSubsystem->ReleaseWidget(Widget, OriginalClassPath);
		Widget = nullptr;
	}
}

bool UAsyncWidgetHandle::IsValid() const
{
	return Widget != nullptr && OwningSubsystem != nullptr && !OriginalClassPath.IsNull();
}