﻿//******************************************************************************
//
// Copyright (c) Microsoft. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//******************************************************************************
// clang-format does not like C++/CX
// clang-format off
#include "pch.h"
#include "Button.xaml.h"

using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::UI::Xaml::Controls;

namespace UIKit {
namespace Xaml {

// Method to get default text Foreground brush
Brush^ GetDefaultWhiteForegroundBrush() {
    static auto ret = ref new SolidColorBrush(Windows::UI::Colors::White);
    return ret;
}

Button::Button() {
    InitializeComponent();
    
    // Default to a transparent background brush so we can accept pointer input
    static auto transparentBrush = ref new SolidColorBrush(Windows::UI::Colors::Transparent);
    Background = transparentBrush;
}

// Accessor for our Layer content
Image^ Button::LayerContent::get() {
    if (!_content && _contentCanvas) {
        _content = ref new Image();
        _content->Name = "LayerContent";

        // Layer content is behind any sublayers
        _contentCanvas->Children->InsertAt(0, _content);
    }

    return _content;
}

// Accessor for our Layer content
bool Button::HasLayerContent::get() {
    return _content != nullptr;
}

// Accessor for our SublayerCanvas
Canvas^ Button::SublayerCanvas::get() {
    return _contentCanvas;
}

// Accessor for the LayerProperty that manages the BorderBrush of this button
Private::CoreAnimation::LayerProperty^ Button::GetBorderBrushProperty() {
    return ref new Private::CoreAnimation::LayerProperty(_border, _border->BorderBrushProperty);
}

// Accessor for the LayerProperty that manages the BorderThickness of this button
Private::CoreAnimation::LayerProperty^ Button::GetBorderThicknessProperty() {
    return ref new Private::CoreAnimation::LayerProperty(_border, _border->BorderThicknessProperty);
}

void Button::OnPointerPressed(PointerRoutedEventArgs^ e) {
    // Call the pointer hook if it exists
    if (_pointerPressedHook) {
        CapturePointer(e->Pointer);
        _pointerPressedHook->Invoke(
            InspectableFromObject(this).Get(),
            ObjectToType<ABI::Windows::UI::Xaml::Input::IPointerRoutedEventArgs>(e).Get());
    }

    // If the pointer hook didn't handle the event, call into our base class
    if (!e->Handled) {
        __super::OnPointerPressed(e);
    }
}

void Button::OnPointerMoved(PointerRoutedEventArgs^ e) {
    // Call the pointer hook if it exists
    if (_pointerMovedHook) {
        _pointerMovedHook->Invoke(
            InspectableFromObject(this).Get(),
            ObjectToType<ABI::Windows::UI::Xaml::Input::IPointerRoutedEventArgs>(e).Get());
    }

    // If the pointer hook didn't handle the event, call into our base class
    if (!e->Handled) {
        __super::OnPointerMoved(e);
    }
}

void Button::OnPointerReleased(PointerRoutedEventArgs^ e) {
    // Call the pointer hook if it exists
    if (_pointerReleasedHook) {
        _pointerReleasedHook->Invoke(
            InspectableFromObject(this).Get(),
            ObjectToType<ABI::Windows::UI::Xaml::Input::IPointerRoutedEventArgs>(e).Get());
    }

    // If the pointer hook didn't handle the event, call into our base class
    if (!e->Handled) {
        __super::OnPointerReleased(e);
    }
}

void Button::OnPointerCanceled(PointerRoutedEventArgs^ e) {
    // Call the pointer hook if it exists
    if (_pointerCanceledHook) {
        _pointerCanceledHook->Invoke(
            InspectableFromObject(this).Get(),
            ObjectToType<ABI::Windows::UI::Xaml::Input::IPointerRoutedEventArgs>(e).Get());
    }

    // Call into our base class regardless of whether or not the UIKit subclass, etc. 'handled' the event
    __super::OnPointerCanceled(e);
}

void Button::OnPointerCaptureLost(PointerRoutedEventArgs^ e) {
    // Call the pointer hook if it exists
    if (_pointerCaptureLostHook) {
        _pointerCaptureLostHook->Invoke(
            InspectableFromObject(this).Get(),
            ObjectToType<ABI::Windows::UI::Xaml::Input::IPointerRoutedEventArgs>(e).Get());
    }

    // Call into our base class regardless of whether or not the UIKit subclass, etc. 'handled' the event
    __super::OnPointerCaptureLost(e);
}

// Hook other events
void Button::HookLayoutEvent(
    const Microsoft::WRL::ComPtr<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>& layoutHook) {
    _layoutHook = std::move(layoutHook);
}

// Hook pointer events
void Button::HookPointerEvents(
    const Microsoft::WRL::ComPtr<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>& pointerPressedHook,
    const Microsoft::WRL::ComPtr<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>& pointerMovedHook,
    const Microsoft::WRL::ComPtr<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>& pointerReleasedHook,
    const Microsoft::WRL::ComPtr<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>& pointerCanceledHook,
    const Microsoft::WRL::ComPtr<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>& pointerCaptureLostHook) {
    _pointerPressedHook = std::move(pointerPressedHook);
    _pointerMovedHook = std::move(pointerMovedHook);
    _pointerReleasedHook = std::move(pointerReleasedHook);
    _pointerCanceledHook = std::move(pointerCanceledHook);
    _pointerCaptureLostHook = std::move(pointerCaptureLostHook);
}

void Button::RemovePointerEvents() {
    _pointerPressedHook = nullptr;
    _pointerMovedHook = nullptr;
    _pointerReleasedHook = nullptr;
    _pointerCanceledHook = nullptr;
    _pointerCaptureLostHook = nullptr;
}

void Button::RemoveLayoutEvent() {
    _layoutHook = nullptr;
}

// This method is called multiple times by XAML, and we call back to UIButton to layout the views.
// This will be used by autolayout to get the intrinsic content size when XAML has done calculating the elements final desired size.
Windows::Foundation::Size Button::ArrangeOverride(Windows::Foundation::Size finalSize) {
    __super::ArrangeOverride(finalSize);
    if (_layoutHook) {
        _layoutHook->Invoke(nullptr, nullptr);
    }

    return finalSize;
}

void Button::OnApplyTemplate() {
    // Call GetTemplateChild to grab references to UIElements in our custom control template
    _image = safe_cast<Image^>(GetTemplateChild("buttonImage"));
    _border = safe_cast<Border^>(GetTemplateChild("buttonBorder"));
    _contentCanvas = safe_cast<Canvas^>(GetTemplateChild(L"contentCanvas"));
}

} /* Xaml*/
} /* UIKit*/

////////////////////////////////////////////////////////////////////////////////////
// ObjectiveC Interop
////////////////////////////////////////////////////////////////////////////////////
UIKIT_XAML_EXPORT void XamlCreateButton(IInspectable** created) {
    ComPtr<IInspectable> inspectable = InspectableFromObject(ref new UIKit::Xaml::Button());
    *created = inspectable.Detach();
}

UIKIT_XAML_EXPORT void XamlRemovePointerEvents(const ComPtr<IInspectable>& inspectableButton) {
    auto button = safe_cast<UIKit::Xaml::Button^>(reinterpret_cast<Platform::Object^>(inspectableButton.Get()));
    button->RemovePointerEvents();
}

UIKIT_XAML_EXPORT void XamlRemoveLayoutEvent(const ComPtr<IInspectable>& inspectableButton) {
    auto button = safe_cast<UIKit::Xaml::Button^>(reinterpret_cast<Platform::Object^>(inspectableButton.Get()));
    button->RemoveLayoutEvent();
}

UIKIT_XAML_EXPORT void XamlButtonApplyVisuals(
    const ComPtr<IInspectable>& inspectableButton,
    const ComPtr<IInspectable>& inspectableButtonImage,
    const ComPtr<IInspectable>& inspectableBorderBackgroundBrush) {
    auto button = safe_cast<UIKit::Xaml::Button^>(reinterpret_cast<Platform::Object^>(inspectableButton.Get()));

    // Set the Button's Image
    auto image = safe_cast<Brush^>(reinterpret_cast<Platform::Object^>(inspectableButtonImage.Get()));
    if (image) {
        ImageBrush^ imageBrush = safe_cast<ImageBrush^>(image);
        button->_image->Source = safe_cast<BitmapSource^>(imageBrush->ImageSource);
    }

    // Set the border background brush (if any)
    button->_border->Background = safe_cast<Brush^>(reinterpret_cast<Platform::Object^>(inspectableBorderBackgroundBrush.Get()));
}

UIKIT_XAML_EXPORT void XamlHookButtonPointerEvents(
    const ComPtr<IInspectable>& inspectableButton,
    const ComPtr<IInspectable>& pointerPressedHook,
    const ComPtr<IInspectable>& pointerMovedHook,
    const ComPtr<IInspectable>& pointerReleasedHook,
    const ComPtr<IInspectable>& pointerCanceledHook,
    const ComPtr<IInspectable>& pointerCaptureLostHook) {

    // Now subscribe to the events on the actual button object
    auto button = safe_cast<UIKit::Xaml::Button^>(reinterpret_cast<Platform::Object^>(inspectableButton.Get()));
    button->HookPointerEvents(
        InspectableToType<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>(pointerPressedHook),
        InspectableToType<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>(pointerMovedHook),
        InspectableToType<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>(pointerReleasedHook),
        InspectableToType<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>(pointerCanceledHook),
        InspectableToType<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>(pointerCaptureLostHook));
}

UIKIT_XAML_EXPORT void XamlHookLayoutEvent(const ComPtr<IInspectable>& inspectableButton,
                                                   const ComPtr<IInspectable>&  layoutHook) {
    auto button = safe_cast<UIKit::Xaml::Button^>(reinterpret_cast<Platform::Object^>(inspectableButton.Get()));
    button->HookLayoutEvent(
        InspectableToType<ABI::Windows::UI::Xaml::Input::IPointerEventHandler>(layoutHook));
    
}

// clang-format on