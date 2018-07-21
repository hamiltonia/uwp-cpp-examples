// Copyright (c) Microsoft. All rights reserved.

#include "pch.h"
#include "AppPartial.h"
#include "MainPage.xaml.h"
#include <string>
#include <ppltasks.h>

using namespace MultiInstanceUWP;

using namespace Concurrency;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::System;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Interop;

/// <summary>
/// Invoked when application is launched through protocol.
/// Read more - http://msdn.microsoft.com/library/windows/apps/br224742
/// </summary>
/// <param name="args"></param>
bool App::OnAppActivated(IActivatedEventArgs^ args)
{
    Platform::String^ apptype = L"unity";
    if (args->Kind == ActivationKind::Protocol)
    {
        auto eventArgs = safe_cast<ProtocolActivatedEventArgs^>(args);
        auto queryParsed = eventArgs->Uri->QueryParsed;
        DirectXPageComponent::ProtocolArgs args(queryParsed);
        apptype = args.GetParameter(L"apptype", L"unity");

        if (apptype != L"unity")
        {
            InitializePage(DirectXPageComponent::DirectXPage::typeid, eventArgs->Uri);
            return true;
        }
        else
        {
            InitializePage(DirectXPageComponent::WebViewPage::typeid, eventArgs->Uri);
            return true;
        }
    }

    return false;
}

Frame^ App::CreateRootFrame()
{
    auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);

    // Do not repeat app initialization when the Window already has content,
    // just ensure that the window is active
    if (rootFrame == nullptr)
    {
        // Create a Frame to act as the navigation context and navigate to the first page
        rootFrame = ref new Frame();

        // Set the default language
        rootFrame->Language = Windows::Globalization::ApplicationLanguages::Languages->GetAt(0);

        // Place the frame in the current Window
        Window::Current->Content = rootFrame;
    }
    return rootFrame;
}

void App::InitializePage(Platform::Type^ pageType, Uri^ uri)
{
    //ApplicationView::GetForCurrentView()->SuppressSystemOverlays = false;
    Frame^ rootFrame = CreateRootFrame();
    bool result = rootFrame->Navigate(TypeName(pageType));

    if (!result)
    {
        throw ref new Platform::Exception(-1, "Failed to create XAML page");
    }

    Window::Current->Activate();
}

Concurrency::task<bool> App::LaunchXamlApp()
{
    auto uri = ref new Uri("stammen-multi-instance-uwp:?id=1234&apptype=xaml"); // The protocol handled by the launched app
    auto options = ref new LauncherOptions();
    return create_task(Launcher::LaunchUriAsync(uri, options));
}