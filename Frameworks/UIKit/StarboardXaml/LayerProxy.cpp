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

#include <ppltasks.h>
#include <robuffer.h>
#include <collection.h>
#include <assert.h>
#include "CALayerXaml.h"

#include "winobjc\winobjc.h"
#include "ApplicationCompositor.h"
#include "CompositorInterface.h"
#include <StringHelpers.h>

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
extern Canvas^ windowCollection;
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
    for (auto curNode : _subnodes) {
        curNode->_parent = NULL;
    }
}

Microsoft::WRL::ComPtr<IInspectable> LayerProxy::GetXamlElement() {
    return _xamlElement;
}

///////////////////////////////////////////////////////////////////////////////////////
// TODO: This should just happen in UIWindow.mm and should get deleted from here
void LayerProxy::SetNodeZIndex(int zIndex) {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    xamlNode->SetValue(Canvas::ZIndexProperty, zIndex);
}
///////////////////////////////////////////////////////////////////////////////////////

void LayerProxy::SetProperty(const wchar_t* name, float value) {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    CoreAnimation::LayerProperties::SetValue(xamlNode, ref new Platform::String(name), (double)value);
}

void LayerProxy::SetPropertyInt(const wchar_t* name, int value) {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    CoreAnimation::LayerProperties::SetValue(xamlNode, ref new Platform::String(name), (int)value);
}

void LayerProxy::SetHidden(bool hidden) {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    xamlNode->Visibility = (hidden ? Visibility::Collapsed : Visibility::Visible);
}

void LayerProxy::SetMasksToBounds(bool masksToBounds) {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    CoreAnimation::LayerProperties::SetValue(xamlNode, "masksToBounds", masksToBounds);
}

float LayerProxy::_GetPresentationPropertyValue(const char* name) {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    std::string str(name);
    std::wstring wstr(str.begin(), str.end());
    return (float)(double)CoreAnimation::LayerProperties::GetValue(xamlNode, ref new Platform::String(wstr.data()));
}

void LayerProxy::SetContentsCenter(float x, float y, float width, float height) {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    CoreAnimation::LayerProperties::SetContentCenter(xamlNode, Rect(x, y, width, height));
}

/////////////////////////////////////////////////////////////
// TODO: FIND A WAY TO REMOVE THIS?
void LayerProxy::SetTopMost() {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    _topMost = true;
    // xamlNode->SetTopMost();
    // Worst case, this becomes:
    // xamlNode -> _SetContent(nullptr);
    // xamlNode -> __super::Background = nullptr;
}

void LayerProxy::SetBackgroundColor(float r, float g, float b, float a) {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);

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
    if (Panel^ panel = dynamic_cast<Panel^>(xamlNode)) {
        panel->Background = backgroundBrush;
    } else if (Control^ control = dynamic_cast<Control^>(xamlNode)) {
        control->Background = backgroundBrush;
    } else {
        UNIMPLEMENTED_WITH_MSG(
            "SetBackgroundColor not supported on this Xaml element: [%ws].",
            xamlNode->GetType()->FullName->Data());
    }
}

void LayerProxy::SetShouldRasterize(bool rasterize) {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    if (rasterize) {
        xamlNode->CacheMode = ref new Media::BitmapCache();
    } else if (xamlNode->CacheMode) {
        xamlNode->CacheMode = nullptr;
    }
}

void LayerProxy::SetContents(const Microsoft::WRL::ComPtr<IInspectable>& bitmap, float width, float height, float scale) {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    if (bitmap) {
        auto content = dynamic_cast<Media::ImageSource^>(reinterpret_cast<Platform::Object^>(bitmap.Get()));
        CoreAnimation::LayerProperties::SetContent(xamlNode, content, width, height, scale);
    } else {
        CoreAnimation::LayerProperties::SetContent(xamlNode, nullptr, width, height, scale);
    }
}

void LayerProxy::AddToRoot() {
    FrameworkElement^ xamlNode = _FrameworkElementFromInspectable(_xamlElement);
    windowCollection->Children->Append(xamlNode);
    SetMasksToBounds(true);
    _isRoot = true;
}

void LayerProxy::AddSubnode(const std::shared_ptr<LayerProxy>& subNode, const std::shared_ptr<LayerProxy>& before, const std::shared_ptr<LayerProxy>& after) {
    assert(subNode->_parent == NULL);
    subNode->_parent = this;
    _subnodes.insert(subNode);

    FrameworkElement^ xamlElementForSubNode = _FrameworkElementFromInspectable(subNode->_xamlElement);
    Panel^ subLayerPanelForThisNode = _SubLayerPanelFromInspectable(_xamlElement);
    if (!subLayerPanelForThisNode) {
        UNIMPLEMENTED_WITH_MSG(
            "AddSubNode not supported on this Xaml element: [%ws].", 
            _FrameworkElementFromInspectable(_xamlElement)->GetType()->FullName->Data());
        return;
    }

    if (before == NULL && after == NULL) {
        subLayerPanelForThisNode->Children->Append(xamlElementForSubNode);
    } else if (before != NULL) {
        FrameworkElement^ xamlBeforeNode = _FrameworkElementFromInspectable(before->_xamlElement);
        unsigned int idx = 0;
        if (subLayerPanelForThisNode->Children->IndexOf(xamlBeforeNode, &idx) == true) {
            subLayerPanelForThisNode->Children->InsertAt(idx, xamlElementForSubNode);
        } else {
            FAIL_FAST();
        }
    } else if (after != NULL) {
        FrameworkElement^ xamlAfterNode = _FrameworkElementFromInspectable(after->_xamlElement);
        unsigned int idx = 0;
        if (subLayerPanelForThisNode->Children->IndexOf(xamlAfterNode, &idx) == true) {
            subLayerPanelForThisNode->Children->InsertAt(idx + 1, xamlElementForSubNode);
        } else {
            FAIL_FAST();
        }
    }

    subLayerPanelForThisNode->InvalidateArrange();
}

void LayerProxy::MoveNode(const std::shared_ptr<LayerProxy>& before, const std::shared_ptr<LayerProxy>& after) {
    assert(_parent != NULL);

    FrameworkElement^ xamlElementForThisNode = _FrameworkElementFromInspectable(_xamlElement);
    Panel^ subLayerPanelForParentNode = _SubLayerPanelFromInspectable(_parent->_xamlElement);
    if (!subLayerPanelForParentNode) {
        FrameworkElement^ xamlElementForParentNode = _FrameworkElementFromInspectable(_parent->_xamlElement);
        UNIMPLEMENTED_WITH_MSG(
            "MoveNode for [%ws] not supported on parent [%ws].",
            xamlElementForThisNode->GetType()->FullName->Data(),
            xamlElementForParentNode->GetType()->FullName->Data());
        return;
    }

    if (before != NULL) {
        FrameworkElement^ xamlBeforeNode = _FrameworkElementFromInspectable(before->_xamlElement);

        unsigned int srcIdx = 0;
        if (subLayerPanelForParentNode->Children->IndexOf(xamlElementForThisNode, &srcIdx) == true) {
            unsigned int destIdx = 0;
            if (subLayerPanelForParentNode->Children->IndexOf(xamlBeforeNode, &destIdx) == true) {
                if (srcIdx == destIdx)
                    return;

                if (srcIdx < destIdx)
                    destIdx--;

                subLayerPanelForParentNode->Children->RemoveAt(srcIdx);
                subLayerPanelForParentNode->Children->InsertAt(destIdx, xamlElementForThisNode);
            } else {
                FAIL_FAST();
            }
        } else {
            FAIL_FAST();
        }
    } else {
        assert(after != NULL);

        FrameworkElement^ xamlAfterNode = _FrameworkElementFromInspectable(after->_xamlElement);
        unsigned int srcIdx = 0;
        if (subLayerPanelForParentNode->Children->IndexOf(xamlElementForThisNode, &srcIdx) == true) {
            unsigned int destIdx = 0;
            if (subLayerPanelForParentNode->Children->IndexOf(xamlAfterNode, &destIdx) == true) {
                if (srcIdx == destIdx)
                    return;

                if (srcIdx < destIdx)
                    destIdx--;

                subLayerPanelForParentNode->Children->RemoveAt(srcIdx);
                subLayerPanelForParentNode->Children->InsertAt(destIdx + 1, xamlElementForThisNode);
            } else {
                FAIL_FAST();
            }
        } else {
            FAIL_FAST();
        }
    }
}

void LayerProxy::RemoveFromSupernode() {
    FrameworkElement^ xamlElementForThisNode = _FrameworkElementFromInspectable(_xamlElement);
    Panel^ subLayerPanelForParentNode;
    if (_isRoot) {
        subLayerPanelForParentNode = (Panel^)(Platform::Object^)windowCollection;
    } else {
        if (!_parent) {
            return;
        }

        subLayerPanelForParentNode = _SubLayerPanelFromInspectable(_parent->_xamlElement);
        _parent->_subnodes.erase(shared_from_this());
    }

    if (!subLayerPanelForParentNode) {
        FrameworkElement^ xamlElementForParentNode = _FrameworkElementFromInspectable(_parent->_xamlElement);
        UNIMPLEMENTED_WITH_MSG(
            "RemoveFromSupernode for [%ws] not supported on parent [%ws].",
            xamlElementForThisNode->GetType()->FullName->Data(),
            xamlElementForParentNode->GetType()->FullName->Data());

        return;
    }

    _parent = nullptr;

    unsigned int idx = 0;
    if (subLayerPanelForParentNode->Children->IndexOf(xamlElementForThisNode, &idx) == true) {
        subLayerPanelForParentNode->Children->RemoveAt(idx);
    } else {
        FAIL_FAST();
    }
}
