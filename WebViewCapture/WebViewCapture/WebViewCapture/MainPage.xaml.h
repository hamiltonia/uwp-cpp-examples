﻿//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"
#include "StepTimer.h"

namespace WebViewCapture
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	public ref class MainPage sealed
	{
	public:
		MainPage();
    private:
        void TimerTick(Platform::Object^ sender, Platform::Object^ e);
        void Update();
        Concurrency::task<void> DisplayScaledBitmap(unsigned int width, unsigned int height);

        Windows::UI::Xaml::DispatcherTimer^ m_dispatcherTimer;
        DX::StepTimer m_timer;
	};
}