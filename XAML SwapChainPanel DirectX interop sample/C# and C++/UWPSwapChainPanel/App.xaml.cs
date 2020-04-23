//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
//
//*********************************************************

using System;
using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Navigation;
using UWPSwapChainPanel;
using Microsoft.Gaming.XboxGameBar;
using SwapChainPanel;

namespace UWPSwapChainPanel
{

    /// <summary>
    /// Provides application-specific behavior to supplement the default Application class.
    /// </summary>
    sealed partial class App : Application
    {
        private XboxGameBarWidget widget1 = null;

        /// <summary>
        /// Initializes the singleton Application object.  This is the first line of authored code
        /// executed, and as such is the logical equivalent of main() or WinMain().
        /// </summary>
        public App()
        {
            // Initialize dependency properties for the DrawingPanel class used in Scenario2.
            DirectXPanels.DrawingPanel.RegisterDependencyProperties();

            this.InitializeComponent();
            this.Suspending += OnSuspending;
        }

        /// <summary>
        /// Invoked when the application is launched normally by the end user.  Other entry points
        /// will be used when the application is launched to open a specific file, to display
        /// search results, and so forth.
        /// </summary>
        /// <param name="args">Details about the launch request and process.</param>
        protected override async void OnLaunched(LaunchActivatedEventArgs args)
        {
            Frame rootFrame = Window.Current.Content as Frame;

            // Do not repeat app initialization when the Window already has content,
            // just ensure that the window is active
            
            if (rootFrame == null)
            {
                // Create a Frame to act as the navigation context and navigate to the first page
                rootFrame = new Frame();
                // Associate the frame with a SuspensionManager key                                
                SuspensionManager.RegisterFrame(rootFrame, "AppFrame");

                if (args.PreviousExecutionState == ApplicationExecutionState.Terminated)
                {
                    // Restore the saved session state only when appropriate
                    try
                    {
                        await SuspensionManager.RestoreAsync();
                    }
                    catch (SuspensionManagerException)
                    {
                        //Something went wrong restoring state.
                        //Assume there is no state and continue
                    }
                }

                // Place the frame in the current Window
                Window.Current.Content = rootFrame;
            }
            if (rootFrame.Content == null || !String.IsNullOrEmpty(args.Arguments))
            {
                // When the navigation stack isn't restored or there are launch arguments
                // indicating an alternate launch (e.g.: via toast or secondary tile), 
                // navigate to the appropriate page, configuring the new page by passing required 
                // information as a navigation parameter
                //if (!rootFrame.Navigate(typeof(MainPage), args.Arguments))
                if (!rootFrame.Navigate(typeof(Widget), args.Arguments))
                {
                    throw new Exception("Failed to create initial page");
                }
            }
            // Ensure the current window is active
            Window.Current.Activate();
        }


        protected override void OnActivated(IActivatedEventArgs args)
        {
            XboxGameBarWidgetActivatedEventArgs widgetArgs = null;
            if (args.Kind == ActivationKind.Protocol)
            {
                var protocolArgs = args as IProtocolActivatedEventArgs;
                string scheme = protocolArgs.Uri.Scheme;
                if (scheme.Equals("ms-gamebarwidget"))
                {
                    widgetArgs = args as XboxGameBarWidgetActivatedEventArgs;
                }
            }
            if (widgetArgs != null)
            {
                //
                // Activation Notes:
                //
                //    If IsLaunchActivation is true, this is Game Bar launching a new instance
                // of our widget. This means we have a NEW CoreWindow with corresponding UI
                // dispatcher, and we MUST create and hold onto a new XboxGameBarWidget.
                //
                // Otherwise this is a subsequent activation coming from Game Bar. We MUST
                // continue to hold the XboxGameBarWidget created during initial activation
                // and ignore this repeat activation, or just observe the URI command here and act 
                // accordingly.  It is ok to perform a navigate on the root frame to switch 
                // views/pages if needed.  Game Bar lets us control the URI for sending widget to
                // widget commands or receiving a command from another non-widget process. 
                //
                // Important Cleanup Notes:
                //    When our widget is closed--by Game Bar or us calling XboxGameBarWidget.Close()-,
                // the CoreWindow will get a closed event.  We can register for Window.Closed
                // event to know when our partucular widget has shutdown, and cleanup accordingly.
                //
                // NOTE: If a widget's CoreWindow is the LAST CoreWindow being closed for the process
                // then we won't get the Window.Closed event.  However, we will get the OnSuspending
                // call and can use that for cleanup.
                //
                if (widgetArgs.IsLaunchActivation)
                {
                    var rootFrame = new Frame();
                    rootFrame.NavigationFailed += OnNavigationFailed;
                    Window.Current.Content = rootFrame;

                    // Navigate to correct view
                    if (widgetArgs.AppExtensionId == "Widget1")
                    {
                        widget1 = new XboxGameBarWidget(
                            widgetArgs,
                            Window.Current.CoreWindow,
                            rootFrame);
                        rootFrame.Navigate(typeof(Widget), widget1);

                        Window.Current.Closed += Widget1Window_Closed;
                    }
                    //else if (widgetArgs.AppExtensionId == "Widget1Settings")
                    //{
                    //    widget1Settings = new XboxGameBarWidget(
                    //        widgetArgs,
                    //        Window.Current.CoreWindow,
                    //        rootFrame);
                    //    rootFrame.Navigate(typeof(Widget1Settings));

                    //    Window.Current.Closed += Widget1SettingsWindow_Closed;
                    //}
                    //else if (widgetArgs.AppExtensionId == "Widget2")
                    //{
                    //    widget2 = new XboxGameBarWidget(
                    //        widgetArgs,
                    //        Window.Current.CoreWindow,
                    //        rootFrame);
                    //    rootFrame.Navigate(typeof(Widget2), widgetArgs.Uri);

                    //    Window.Current.Closed += Widget2Window_Closed;
                    //}
                    else
                    {
                        // Unknown - Game Bar should never send you an unknown App Extension Id
                        return;
                    }

                    Window.Current.Activate();
                }
                //else if (widgetArgs.AppExtensionId == "Widget2")
                //{
                //    // You can perform whatever behavior you need based on the URI payload. In our case
                //    // we're simply renavigating to Widget2 and displaying the absolute URI.  You
                //    // define your URI schema (subpath + query + fragment). 
                //    Frame rootFrame = null;
                //    var content = Window.Current.Content;
                //    if (content != null)
                //    {
                //        rootFrame = content as Frame;
                //    }
                //    rootFrame.NavigationFailed += OnNavigationFailed;
                //    rootFrame.Navigate(typeof(Widget2), widgetArgs.Uri);
                //}
            }
        }

        private void Widget1Window_Closed(object sender, Windows.UI.Core.CoreWindowEventArgs e)
        {
            widget1 = null;
            Window.Current.Closed -= Widget1Window_Closed;
        }

        void OnNavigationFailed(object sender, NavigationFailedEventArgs e)
        {
            throw new Exception("Failed to load Page " + e.SourcePageType.FullName);
        }

        /// <summary>
        /// Invoked when application execution is being suspended.  Application state is saved
        /// without knowing whether the application will be terminated or resumed with the contents
        /// of memory still intact.
        /// </summary>
        /// <param name="sender">The source of the suspend request.</param>
        /// <param name="e">Details about the suspend request.</param>
        private async void OnSuspending(object sender, SuspendingEventArgs e)
        {
            var deferral = e.SuspendingOperation.GetDeferral();
            await SuspensionManager.SaveAsync();
            deferral.Complete();
        }
    }
}
