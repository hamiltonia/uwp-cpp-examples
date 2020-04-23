//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
//
//*********************************************************

using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Navigation;
using UWPSwapChainPanel;
using System;
using Windows.UI.Xaml.Media;

using Microsoft.Gaming.XboxGameBar;


namespace SwapChainPanel
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class Widget : UWPSwapChainPanel.Common.LayoutAwarePage
    {
        // A pointer back to the main page.  This is needed if you want to call methods in MainPage such
        // as NotifyUser()
        MainPage rootPage = MainPage.Current;

        private XboxGameBarWidget widget = null;
        private XboxGameBarWidgetControl widgetControl = null;

        private int currentSizeMode = 0;

        public Widget()
        {
            this.InitializeComponent();
        }

        /// <summary>
        /// Invoked when this page is about to be displayed in a Frame.
        /// </summary>
        /// <param name="e">Event data that describes how this page was reached.  The Parameter
        /// property is typically used to configure the page.</param>
        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            // Start rendering animated DirectX content on a background thread, which reduces load on the UI thread
            // and can help keep the app responsive to user input.
            DirectXPanel1.StartRenderLoop();

            widget = e.Parameter as XboxGameBarWidget;
            if (widget != null)
            {
                widgetControl = new XboxGameBarWidgetControl(widget);
                widget.SettingsClicked += Widget_SettingsClicked;
            }
        }

        private async void Widget_SettingsClicked(XboxGameBarWidget sender, object args)
        {
            switch(currentSizeMode)
            {
                case 0:
                    currentSizeMode++;
                    await widget.TryResizeWindowAsync(new Windows.Foundation.Size(1920, 1080));
                    break;
                case 1:
                    currentSizeMode++;
                    await widget.TryResizeWindowAsync(new Windows.Foundation.Size(640, 480));
                    break;
                case 2:
                    currentSizeMode = 0;
                    await widget.TryResizeWindowAsync(new Windows.Foundation.Size(400, 300));
                    break;

            }
        }

        /// <summary>
        /// Invoked when this page will no longer be displayed in a Frame.
        /// </summary>
        /// <param name="e"></param>
        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            // Stop rendering DirectX content when it will no longer be on screen.
            DirectXPanel1.StopRenderLoop();
        }
    }
}
