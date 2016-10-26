//******************************************************************************
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
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

#include "LayerProxy.h"
#include "LayerCoordinator.h"

#include <assert.h>
#include <ErrorHandling.h>

using namespace Microsoft::WRL;
using namespace UIKit::Private::CoreAnimation;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;

static const wchar_t* TAG = L"LayerProxy";

//////////////////////////////////////////////////////////////////
// TODO: WE SHOULD GET RID OF THIS OR MORE FORMALLY EXPOSE IT
extern Canvas^ s_windowCollection;
//////////////////////////////////////////////////////////////////

__inline FrameworkElement^ _FrameworkElementFromInspectable(const ComPtr<IInspectable>& element) {
    return dynamic_cast<FrameworkElement^>(reinterpret_cast<Platform::Object^>(element.Get()));
};

__inline Panel^ _SubLayerPanelFromInspectable(const ComPtr<IInspectable>& element) {
    FrameworkElement^ xamlElement = _FrameworkElementFromInspectable(element);

    // First check if the element implements ILayer
    ILayer^ layerElement = dynamic_cast<ILayer^>(xamlElement);
    if (layerElement) {
        return layerElement->SublayerCanvas;
    }

    // Not an ILayer, so default to grabbing the xamlElement's SublayerCanvasProperty (if it exists)
    return dynamic_cast<Canvas^>(_FrameworkElementFromInspectable(element)->GetValue(Layer::SublayerCanvasProperty));
};

LayerProxy::LayerProxy(IInspectable* xamlElement) :
    _xamlElement(nullptr),
    _isRoot(false),
    _parent(nullptr),
    _currentTexture(nullptr),
    _topMost(false) {

    // If we weren't passed a xaml element, default to our Xaml CALayer representation
    if (!xamlElement) {
        _xamlElement = reinterpret_cast<IInspectable*>(ref new Layer());
    } else {
        _xamlElement = xamlElement;
    }

    // Initialize the UIElement with CoreAnimation
    CoreAnimation::LayerProperties::InitializeFrameworkElement(_FrameworkElementFromInspectable(_xamlElement));
}

LayerProxy::~LayerProxy() {
    for (auto layer : _subLayers) {
        layer->_parent = nullptr;
    }
}

Microsoft::WRL::ComPtr<IInspectable> LayerProxy::GetXamlElement() {
    return _xamlElement;
}

///////////////////////////////////////////////////////////////////////////////////////
// TODO: This should just happen in UIWindow.mm and should get deleted from here
void LayerProxy::SetZIndex(int zIndex) {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    xamlLayer->SetValue(Canvas::ZIndexProperty, zIndex);
}
///////////////////////////////////////////////////////////////////////////////////////

void LayerProxy::SetProperty(const wchar_t* name, float value) {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    CoreAnimation::LayerProperties::SetValue(xamlLayer, ref new Platform::String(name), (double)value);
}

void LayerProxy::SetPropertyInt(const wchar_t* name, int value) {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    CoreAnimation::LayerProperties::SetValue(xamlLayer, ref new Platform::String(name), (int)value);
}

void LayerProxy::SetHidden(bool hidden) {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    xamlLayer->Visibility = (hidden ? Visibility::Collapsed : Visibility::Visible);
}

void LayerProxy::SetMasksToBounds(bool masksToBounds) {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    CoreAnimation::LayerProperties::SetValue(xamlLayer, "masksToBounds", masksToBounds);
}

float LayerProxy::_GetPresentationPropertyValue(const char* name) {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    std::string str(name);
    std::wstring wstr(str.begin(), str.end());
    return (float)(double)CoreAnimation::LayerProperties::GetValue(xamlLayer, ref new Platform::String(wstr.data()));
}

void LayerProxy::SetContentsCenter(float x, float y, float width, float height) {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    CoreAnimation::LayerProperties::SetContentCenter(xamlLayer, Rect(x, y, width, height));
}

/////////////////////////////////////////////////////////////
// TODO: FIND A WAY TO REMOVE THIS?
void LayerProxy::SetTopMost() {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    _topMost = true;
    // xamlLayer->SetTopMost();
    // Worst case, this becomes:
    // xamlLayer -> _SetContent(nullptr);
    // xamlLayer -> __super::Background = nullptr;
}

void LayerProxy::SetBackgroundColor(float r, float g, float b, float a) {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);

    SolidColorBrush^ backgroundBrush = nullptr; // A null brush is transparent and not hit-testable
    if (!_isRoot && !_topMost && (a != 0.0)) {
        Windows::UI::Color backgroundColor;
        backgroundColor.R = static_cast<unsigned char>(r * 255.0);
        backgroundColor.G = static_cast<unsigned char>(g * 255.0);
        backgroundColor.B = static_cast<unsigned char>(b * 255.0);
        backgroundColor.A = static_cast<unsigned char>(a * 255.0);
        backgroundBrush = ref new SolidColorBrush(backgroundColor);
    }

    // Panel and Control each have a Background property that we can set
    if (Panel^ panel = dynamic_cast<Panel^>(xamlLayer)) {
        panel->Background = backgroundBrush;
    } else if (Control^ control = dynamic_cast<Control^>(xamlLayer)) {
        control->Background = backgroundBrush;
    } else {
        UNIMPLEMENTED_WITH_MSG(
            "SetBackgroundColor not supported on this Xaml element: [%ws].",
            xamlLayer->GetType()->FullName->Data());
    }
}

void LayerProxy::SetShouldRasterize(bool rasterize) {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    if (rasterize) {
        xamlLayer->CacheMode = ref new Media::BitmapCache();
    } else if (xamlLayer->CacheMode) {
        xamlLayer->CacheMode = nullptr;
    }
}

void LayerProxy::SetContents(const Microsoft::WRL::ComPtr<IInspectable>& bitmap, float width, float height, float scale) {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    if (bitmap) {
        auto content = dynamic_cast<Media::ImageSource^>(reinterpret_cast<Platform::Object^>(bitmap.Get()));
        CoreAnimation::LayerProperties::SetContent(xamlLayer, content, width, height, scale);
    } else {
        CoreAnimation::LayerProperties::SetContent(xamlLayer, nullptr, width, height, scale);
    }
}

void LayerProxy::AddToRoot() {
    FrameworkElement^ xamlLayer = _FrameworkElementFromInspectable(_xamlElement);
    s_windowCollection->Children->Append(xamlLayer);
    SetMasksToBounds(true);
    _isRoot = true;
}

void LayerProxy::AddSubLayer(const std::shared_ptr<LayerProxy>& subLayer, const std::shared_ptr<LayerProxy>& before, const std::shared_ptr<LayerProxy>& after) {
    assert(subLayer->_parent == NULL);
    subLayer->_parent = this;
    _subLayers.insert(subLayer);

    FrameworkElement^ xamlElementForSubLayer = _FrameworkElementFromInspectable(subLayer->_xamlElement);
    Panel^ subLayerPanelForThisLayer = _SubLayerPanelFromInspectable(_xamlElement);
    if (!subLayerPanelForThisLayer) {
        UNIMPLEMENTED_WITH_MSG(
            "AddSubLayer not supported on this Xaml element: [%ws].", 
            _FrameworkElementFromInspectable(_xamlElement)->GetType()->FullName->Data());
        return;
    }

    if (before == NULL && after == NULL) {
        subLayerPanelForThisLayer->Children->Append(xamlElementForSubLayer);
    } else if (before != NULL) {
        FrameworkElement^ xamlBeforeLayer = _FrameworkElementFromInspectable(before->_xamlElement);
        unsigned int idx = 0;
        if (subLayerPanelForThisLayer->Children->IndexOf(xamlBeforeLayer, &idx) == true) {
            subLayerPanelForThisLayer->Children->InsertAt(idx, xamlElementForSubLayer);
        } else {
            FAIL_FAST();
        }
    } else if (after != NULL) {
        FrameworkElement^ xamlAfterLayer = _FrameworkElementFromInspectable(after->_xamlElement);
        unsigned int idx = 0;
        if (subLayerPanelForThisLayer->Children->IndexOf(xamlAfterLayer, &idx) == true) {
            subLayerPanelForThisLayer->Children->InsertAt(idx + 1, xamlElementForSubLayer);
        } else {
            FAIL_FAST();
        }
    }

    subLayerPanelForThisLayer->InvalidateArrange();
}

void LayerProxy::MoveLayer(const std::shared_ptr<LayerProxy>& before, const std::shared_ptr<LayerProxy>& after) {
    assert(_parent != NULL);

    FrameworkElement^ xamlElementForThisLayer = _FrameworkElementFromInspectable(_xamlElement);
    Panel^ subLayerPanelForParentLayer = _SubLayerPanelFromInspectable(_parent->_xamlElement);
    if (!subLayerPanelForParentLayer) {
        FrameworkElement^ xamlElementForParentLayer = _FrameworkElementFromInspectable(_parent->_xamlElement);
        UNIMPLEMENTED_WITH_MSG(
            "MoveLayer for [%ws] not supported on parent [%ws].",
            xamlElementForThisLayer->GetType()->FullName->Data(),
            xamlElementForParentLayer->GetType()->FullName->Data());
        return;
    }

    if (before != NULL) {
        FrameworkElement^ xamlBeforeLayer = _FrameworkElementFromInspectable(before->_xamlElement);

        unsigned int srcIdx = 0;
        if (subLayerPanelForParentLayer->Children->IndexOf(xamlElementForThisLayer, &srcIdx) == true) {
            unsigned int destIdx = 0;
            if (subLayerPanelForParentLayer->Children->IndexOf(xamlBeforeLayer, &destIdx) == true) {
                if (srcIdx == destIdx)
                    return;

                if (srcIdx < destIdx)
                    destIdx--;

                subLayerPanelForParentLayer->Children->RemoveAt(srcIdx);
                subLayerPanelForParentLayer->Children->InsertAt(destIdx, xamlElementForThisLayer);
            } else {
                FAIL_FAST();
            }
        } else {
            FAIL_FAST();
        }
    } else {
        assert(after != NULL);

        FrameworkElement^ xamlAfterLayer = _FrameworkElementFromInspectable(after->_xamlElement);
        unsigned int srcIdx = 0;
        if (subLayerPanelForParentLayer->Children->IndexOf(xamlElementForThisLayer, &srcIdx) == true) {
            unsigned int destIdx = 0;
            if (subLayerPanelForParentLayer->Children->IndexOf(xamlAfterLayer, &destIdx) == true) {
                if (srcIdx == destIdx)
                    return;

                if (srcIdx < destIdx)
                    destIdx--;

                subLayerPanelForParentLayer->Children->RemoveAt(srcIdx);
                subLayerPanelForParentLayer->Children->InsertAt(destIdx + 1, xamlElementForThisLayer);
            } else {
                FAIL_FAST();
            }
        } else {
            FAIL_FAST();
        }
    }
}

void LayerProxy::RemoveFromSuperLayer() {
    FrameworkElement^ xamlElementForThisLayer = _FrameworkElementFromInspectable(_xamlElement);
    Panel^ subLayerPanelForParentLayer;
    if (_isRoot) {
        subLayerPanelForParentLayer = s_windowCollection;
    } else {
        if (!_parent) {
            return;
        }

        subLayerPanelForParentLayer = _SubLayerPanelFromInspectable(_parent->_xamlElement);
        _parent->_subLayers.erase(shared_from_this());
    }

    if (!subLayerPanelForParentLayer) {
        FrameworkElement^ xamlElementForParentLayer = _FrameworkElementFromInspectable(_parent->_xamlElement);
        UNIMPLEMENTED_WITH_MSG(
            "RemoveFromSuperLayer for [%ws] not supported on parent [%ws].",
            xamlElementForThisLayer->GetType()->FullName->Data(),
            xamlElementForParentLayer->GetType()->FullName->Data());

        return;
    }

    _parent = nullptr;

    unsigned int idx = 0;
    if (subLayerPanelForParentLayer->Children->IndexOf(xamlElementForThisLayer, &idx) == true) {
        subLayerPanelForParentLayer->Children->RemoveAt(idx);
    } else {
        FAIL_FAST();
    }
}
