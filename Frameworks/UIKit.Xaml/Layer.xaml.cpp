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
#include "Layer.xaml.h"

#include "ObjCXamlControls.h"

using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

namespace UIKit {
namespace Private {
namespace CoreAnimation {

DependencyProperty^ Layer::s_layerContentProperty = nullptr;
DependencyProperty^ Layer::s_sublayerCanvasProperty = nullptr;
bool Layer::s_dependencyPropertiesRegistered = false;

Layer::Layer() {
    InitializeComponent();

    // No-op if already registered
    _registerDependencyProperties();

    Name = L"Layer";
}

// Accessor for our Layer content
Image^ Layer::LayerContent::get() {
    if (!_content) {
        _content = ref new Image();
        _content->Name = "LayerContent";
        Children->InsertAt(0, _content);
    }

    return _content;
}

// Accessor for our Layer content
bool Layer::HasLayerContent::get() {
    return _content != nullptr;
}

// Accessor for our SublayerCanvas
Canvas^ Layer::SublayerCanvas::get() {
    if (!_sublayerCanvas) {
        _sublayerCanvas = ref new Canvas();
        _sublayerCanvas->Name = "Sublayers";
        Children->Append(_sublayerCanvas);
    }

    return _sublayerCanvas;
}

DependencyProperty^ Layer::LayerContentProperty::get() {
    return s_layerContentProperty;
}

DependencyProperty^ Layer::SublayerCanvasProperty::get() {
    return s_sublayerCanvasProperty;
}

void Layer::_registerDependencyProperties() {
    if (!s_dependencyPropertiesRegistered) {
        s_layerContentProperty = DependencyProperty::RegisterAttached("LayerContent",
            Image::typeid,
            FrameworkElement::typeid,
            nullptr);

        s_sublayerCanvasProperty = DependencyProperty::RegisterAttached("SublayerCanvas",
            Canvas::typeid,
            FrameworkElement::typeid,
            nullptr);

        s_dependencyPropertiesRegistered = true;
    }
}

} /* CoreAnimation */
} /* Private */
} /* UIKit */

////////////////////////////////////////////////////////////////////////////////////
// ObjectiveC Interop
////////////////////////////////////////////////////////////////////////////////////

// Set one or more layer properties for the specified target xaml element
UIKIT_XAML_EXPORT void XamlSetFrameworkElementLayerProperties(
    const Microsoft::WRL::ComPtr<IInspectable>& targetElement,
    const Microsoft::WRL::ComPtr<IInspectable>& layerContentProperty,
    const Microsoft::WRL::ComPtr<IInspectable>& sublayerCanvasProperty) {

    auto frameworkElement = safe_cast<FrameworkElement^>(reinterpret_cast<Object^>(targetElement.Get()));
    if (layerContentProperty) {
        auto contentImage = safe_cast<Image^>(reinterpret_cast<Object^>(layerContentProperty.Get()));
        frameworkElement->SetValue(UIKit::Private::CoreAnimation::Layer::LayerContentProperty, contentImage);
    }

    if (sublayerCanvasProperty) {
        auto sublayerCanvas = safe_cast<Canvas^>(reinterpret_cast<Object^>(sublayerCanvasProperty.Get()));
        frameworkElement->SetValue(UIKit::Private::CoreAnimation::Layer::SublayerCanvasProperty, sublayerCanvas);
    }
}
