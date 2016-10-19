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

#include <wrl/client.h>
#include <memory>
#include <agile.h>
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

Grid^ rootNode;
Canvas^ windowCollection;
extern float screenWidth, screenHeight;
void GridSizeChanged(float newWidth, float newHeight);

void OnGridSizeChanged(Platform::Object^ sender, SizeChangedEventArgs^ e) {
    Windows::Foundation::Size newSize = e->NewSize;
    GridSizeChanged(newSize.Width, newSize.Height);
}

void SetRootGrid(winobjc::Id& root) {
    rootNode = (Grid^)(Platform::Object^)root;

    // canvas serves as the container for UI windows, thus named as windowCollection
    windowCollection = ref new Canvas();
    windowCollection->HorizontalAlignment = HorizontalAlignment::Center;
    windowCollection->VerticalAlignment = VerticalAlignment::Center;

    // For the canvas servering window collection, we give it a special name
    // useful for later when we try to locate this canvas
    windowCollection->Name = L"windowCollection";

    rootNode->Children->Append(windowCollection);
    rootNode->InvalidateArrange();

    rootNode->SizeChanged += ref new SizeChangedEventHandler(&OnGridSizeChanged);
}

void (*RenderCallback)();

ref class XamlRenderingListener sealed {
private:
    bool _isListening = false;
    Windows::Foundation::EventHandler<Platform::Object^>^ listener;
    Windows::Foundation::EventRegistrationToken listenerToken;
    internal : void RenderedFrame(Object^ sender, Object^ args) {
        if (RenderCallback)
            RenderCallback();
    }

    XamlRenderingListener() {
        listener = ref new Windows::Foundation::EventHandler<Platform::Object^>(this, &XamlRenderingListener::RenderedFrame);
    }

    void Start() {
        if (!_isListening) {
            _isListening = true;
            listenerToken = Media::CompositionTarget::Rendering += listener;
        }
    }

    void Stop() {
        if (!_isListening) {
            _isListening = false;
            Media::CompositionTarget::Rendering -= listenerToken;
        }
    }
};

XamlRenderingListener^ renderListener;

void EnableRenderingListener(void (*callback)()) {
    RenderCallback = callback;
    if (renderListener == nullptr) {
        renderListener = ref new XamlRenderingListener();
    }
    renderListener->Start();
}

void DisableRenderingListener() {
    renderListener->Stop();
}

__inline FrameworkElement^ GetXamlElement(DisplayNode* node) {
    return dynamic_cast<FrameworkElement^>(reinterpret_cast<Platform::Object^>(node->_xamlElement.Get()));
};

__inline Panel^ GetSubLayerPanel(DisplayNode* node) {
    FrameworkElement^ xamlElement = GetXamlElement(node);

    // First check if the element implements ILayer
    ILayer^ layerElement = dynamic_cast<ILayer^>(xamlElement);
    if (layerElement) {
        return layerElement->SublayerCanvas;
    }

    // Not an ILayer, so default to grabbing the xamlElement's SublayerCanvasProperty (if it exists)
    return dynamic_cast<Canvas^>(GetXamlElement(node)->GetValue(Layer::SublayerCanvasProperty));
};

__inline CoreAnimation::EventedStoryboard^ GetStoryboard(DisplayAnimation* anim) {
    return (CoreAnimation::EventedStoryboard^)(Platform::Object^)anim->_xamlAnimation;
}

DisplayAnimation::DisplayAnimation() {
    easingFunction = EaseInEaseOut;
}

void DisplayAnimation::CreateXamlAnimation() {
    auto xamlAnimation = ref new CoreAnimation::EventedStoryboard(
        beginTime, duration, autoReverse, repeatCount, repeatDuration, speed, timeOffset);

    switch (easingFunction) {
        case Easing::EaseInEaseOut: {
            auto easing = ref new Media::Animation::QuadraticEase();
            easing->EasingMode = Media::Animation::EasingMode::EaseInOut;
            xamlAnimation->AnimationEase = easing;
        } break;

        case Easing::EaseIn: {
            auto easing = ref new Media::Animation::QuadraticEase();
            easing->EasingMode = Media::Animation::EasingMode::EaseIn;
            xamlAnimation->AnimationEase = easing;
        } break;

        case Easing::EaseOut: {
            auto easing = ref new Media::Animation::QuadraticEase();
            easing->EasingMode = Media::Animation::EasingMode::EaseOut;
            xamlAnimation->AnimationEase = easing;
        } break;

        case Easing::Default: {
            auto easing = ref new Media::Animation::QuadraticEase();
            easing->EasingMode = Media::Animation::EasingMode::EaseInOut;
            xamlAnimation->AnimationEase = easing;
        } break;

        case Easing::Linear: {
            xamlAnimation->AnimationEase = nullptr;
        } break;
    }

    _xamlAnimation = (Platform::Object^)xamlAnimation;
}

void DisplayAnimation::Start() {
    auto xamlAnimation = (CoreAnimation::EventedStoryboard^)(Platform::Object^)_xamlAnimation;

    std::weak_ptr<DisplayAnimation> weakThis = shared_from_this();
    xamlAnimation->Completed = ref new CoreAnimation::AnimationMethod([weakThis](Platform::Object^ sender) {
        std::shared_ptr<DisplayAnimation> strongThis = weakThis.lock();
        if (strongThis) {
            strongThis->Completed();
            auto xamlAnimation = (CoreAnimation::EventedStoryboard^)(Platform::Object^)strongThis->_xamlAnimation;
            xamlAnimation->Completed = nullptr;
        }
    });
    xamlAnimation->Start();
}

void DisplayAnimation::Stop() {
    auto xamlAnimation = (CoreAnimation::EventedStoryboard^)(Platform::Object^)_xamlAnimation;
    auto storyboard = (Media::Animation::Storyboard^)(Platform::Object^)xamlAnimation->GetStoryboard();
    storyboard->Stop();
    xamlAnimation->Completed = nullptr;
    _xamlAnimation = nullptr;
}

concurrency::task<void> DisplayAnimation::AddAnimation(DisplayNode& node, const wchar_t* propertyName, bool fromValid, float from, bool toValid, float to) {
    auto xamlNode = GetXamlElement(&node);
    auto xamlAnimation = GetStoryboard(this);

    xamlAnimation->Animate(xamlNode,
                           ref new Platform::String(propertyName),
                           fromValid ? (Platform::Object^)(double)from : nullptr,
                           toValid ? (Platform::Object^)(double)to : nullptr);

    return concurrency::task_from_result();
}

concurrency::task<void> DisplayAnimation::AddTransitionAnimation(DisplayNode& node, const char* type, const char* subtype) {
    auto xamlNode = GetXamlElement(&node);
    auto xamlAnimation = GetStoryboard(this);

    std::string stype(type);
    std::wstring wtype(stype.begin(), stype.end());
    std::string ssubtype(subtype);
    std::wstring wsubtype(ssubtype.begin(), ssubtype.end());

    // We currently only support layer snapshots/transitions on our default Layer implementation
    auto coreAnimationLayer = dynamic_cast<Layer^>(xamlNode);
    if (!coreAnimationLayer) {
        UNIMPLEMENTED_WITH_MSG(
            "Layer transition animations not supported on this Xaml element: [%ws].",
            xamlNode->GetType()->FullName->Data());
        return concurrency::task_from_result();
    }

    // Render a layer snapshot, then kick off the animation
    return xamlAnimation->SnapshotLayer(coreAnimationLayer)
        .then([this, xamlAnimation, coreAnimationLayer, wtype, wsubtype](Layer^ snapshotLayer) noexcept {

        xamlAnimation->AddTransition(
            coreAnimationLayer,
            snapshotLayer,
            ref new Platform::String(wtype.data()),
            ref new Platform::String(wsubtype.data()));

        Start();
    } , concurrency::task_continuation_context::use_current());
}

DisplayNode::DisplayNode(IInspectable* xamlElement) :
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
    CoreAnimation::LayerProperties::InitializeFrameworkElement(GetXamlElement(this));
}

DisplayNode::~DisplayNode() {
    for (auto curNode : _subnodes) {
        curNode->_parent = NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
// TODO: This should just happen in UIWindow.mm and should get deleted from here
void DisplayNode::SetNodeZIndex(int zIndex) {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    xamlNode->SetValue(Canvas::ZIndexProperty, zIndex);
}
///////////////////////////////////////////////////////////////////////////////////////

void DisplayNode::SetProperty(const wchar_t* name, float value) {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    CoreAnimation::LayerProperties::SetValue(xamlNode, ref new Platform::String(name), (double)value);
}

void DisplayNode::SetPropertyInt(const wchar_t* name, int value) {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    CoreAnimation::LayerProperties::SetValue(xamlNode, ref new Platform::String(name), (int)value);
}

void DisplayNode::SetHidden(bool hidden) {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    xamlNode->Visibility = (hidden ? Visibility::Collapsed : Visibility::Visible);
}

void DisplayNode::SetMasksToBounds(bool masksToBounds) {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    CoreAnimation::LayerProperties::SetValue(xamlNode, "masksToBounds", masksToBounds);
}

float DisplayNode::_GetPresentationPropertyValue(const char* name) {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    std::string str(name);
    std::wstring wstr(str.begin(), str.end());
    return (float)(double)CoreAnimation::LayerProperties::GetValue(xamlNode, ref new Platform::String(wstr.data()));
}

void DisplayNode::SetContentsCenter(float x, float y, float width, float height) {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    CoreAnimation::LayerProperties::SetContentCenter(xamlNode, Rect(x, y, width, height));
}

/////////////////////////////////////////////////////////////
// TODO: FIND A WAY TO REMOVE THIS?
void DisplayNode::SetTopMost() {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    _topMost = true;
    // xamlNode->SetTopMost();
    // Worst case, this becomes:
    // xamlNode -> _SetContent(nullptr);
    // xamlNode -> __super::Background = nullptr;
}

void DisplayNode::SetBackgroundColor(float r, float g, float b, float a) {
    FrameworkElement^ xamlNode = GetXamlElement(this);

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

void DisplayNode::SetShouldRasterize(bool rasterize) {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    if (rasterize) {
        xamlNode->CacheMode = ref new Media::BitmapCache();
    } else if (xamlNode->CacheMode) {
        xamlNode->CacheMode = nullptr;
    }
}

void DisplayNode::SetContents(const Microsoft::WRL::ComPtr<IInspectable>& bitmap, float width, float height, float scale) {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    if (bitmap) {
        auto content = dynamic_cast<Media::ImageSource^>(reinterpret_cast<Platform::Object^>(bitmap.Get()));
        CoreAnimation::LayerProperties::SetContent(xamlNode, content, width, height, scale);
    } else {
        CoreAnimation::LayerProperties::SetContent(xamlNode, nullptr, width, height, scale);
    }
}

void DisplayNode::AddToRoot() {
    FrameworkElement^ xamlNode = GetXamlElement(this);
    windowCollection->Children->Append(xamlNode);
    SetMasksToBounds(true);
    _isRoot = true;
}

void DisplayNode::AddSubnode(const std::shared_ptr<DisplayNode>& subNode, const std::shared_ptr<DisplayNode>& before, const std::shared_ptr<DisplayNode>& after) {
    assert(subNode->_parent == NULL);
    subNode->_parent = this;
    _subnodes.insert(subNode);

    FrameworkElement^ xamlElementForSubNode = GetXamlElement(subNode.get());
    Panel^ subLayerPanelForThisNode = GetSubLayerPanel(this);
    if (!subLayerPanelForThisNode) {
        UNIMPLEMENTED_WITH_MSG(
            "AddSubNode not supported on this Xaml element: [%ws].", 
            GetXamlElement(this)->GetType()->FullName->Data());
        return;
    }

    if (before == NULL && after == NULL) {
        subLayerPanelForThisNode->Children->Append(xamlElementForSubNode);
    } else if (before != NULL) {
        FrameworkElement^ xamlBeforeNode = GetXamlElement(before.get());
        unsigned int idx = 0;
        if (subLayerPanelForThisNode->Children->IndexOf(xamlBeforeNode, &idx) == true) {
            subLayerPanelForThisNode->Children->InsertAt(idx, xamlElementForSubNode);
        } else {
            FAIL_FAST();
        }
    } else if (after != NULL) {
        FrameworkElement^ xamlAfterNode = GetXamlElement(after.get());
        unsigned int idx = 0;
        if (subLayerPanelForThisNode->Children->IndexOf(xamlAfterNode, &idx) == true) {
            subLayerPanelForThisNode->Children->InsertAt(idx + 1, xamlElementForSubNode);
        } else {
            FAIL_FAST();
        }
    }

    subLayerPanelForThisNode->InvalidateArrange();
}

void DisplayNode::MoveNode(const std::shared_ptr<DisplayNode>& before, const std::shared_ptr<DisplayNode>& after) {
    assert(_parent != NULL);

    FrameworkElement^ xamlElementForThisNode = GetXamlElement(this);
    Panel^ subLayerPanelForParentNode = GetSubLayerPanel(_parent);
    if (!subLayerPanelForParentNode) {
        FrameworkElement^ xamlElementForParentNode = GetXamlElement(_parent);
        UNIMPLEMENTED_WITH_MSG(
            "MoveNode for [%ws] not supported on parent [%ws].",
            xamlElementForThisNode->GetType()->FullName->Data(),
            xamlElementForParentNode->GetType()->FullName->Data());
        return;
    }

    if (before != NULL) {
        FrameworkElement^ xamlBeforeNode = GetXamlElement(before.get());

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

        FrameworkElement^ xamlAfterNode = GetXamlElement(after.get());
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

void DisplayNode::RemoveFromSupernode() {
    FrameworkElement^ xamlElementForThisNode = GetXamlElement(this);
    Panel^ subLayerPanelForParentNode;
    if (_isRoot) {
        subLayerPanelForParentNode = (Panel^)(Platform::Object^)windowCollection;
    } else {
        if (!_parent) {
            return;
        }

        subLayerPanelForParentNode = GetSubLayerPanel(_parent);
        _parent->_subnodes.erase(shared_from_this());
    }

    if (!subLayerPanelForParentNode) {
        FrameworkElement^ xamlElementForParentNode = GetXamlElement(_parent);
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

ComPtr<IInspectable> CreateBitmapFromImageData(const void* ptr, int len) {
    auto bitmap = ref new Media::Imaging::BitmapImage;
    auto stream = ref new Windows::Storage::Streams::InMemoryRandomAccessStream();
    auto rw = ref new Windows::Storage::Streams::DataWriter(stream->GetOutputStreamAt(0));
    auto var = ref new Platform::Array<unsigned char, 1>(len);

    memcpy(var->Data, ptr, len);
    rw->WriteBytes(var);
    rw->StoreAsync();

    auto loadImage = bitmap->SetSourceAsync(stream);

    return reinterpret_cast<IInspectable*>(bitmap);
}

ComPtr<IInspectable> CreateBitmapFromBits(void* ptr, int width, int height, int stride) {
    auto bitmap = ref new Media::Imaging::WriteableBitmap(width, height);
    Windows::Storage::Streams::IBuffer^ pixelData = bitmap->PixelBuffer;

    ComPtr<IBufferByteAccess> bufferByteAccess;
    reinterpret_cast<IInspectable*>(pixelData)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));

    // Retrieve the buffer data.
    byte* pixels = nullptr;
    bufferByteAccess->Buffer(&pixels);

    byte* in = (byte*)ptr;
    for (int y = 0; y < height; y++) {
        memcpy(pixels, in, stride);
        pixels += width * 4;
        in += stride;
    }

    return reinterpret_cast<IInspectable*>(bitmap);
}

ComPtr<IInspectable> CreateWritableBitmap(int width, int height) {
    auto bitmap = ref new Media::Imaging::WriteableBitmap(width, height);
    return reinterpret_cast<IInspectable*>(bitmap);
}

ComPtr<IInspectable> LockWritableBitmap(const ComPtr<IInspectable>& bitmap, void** ptr, int* stride) {
    auto writableBitmap = dynamic_cast<Media::Imaging::WriteableBitmap^>(reinterpret_cast<Platform::Object^>(bitmap.Get()));

    Windows::Storage::Streams::IBuffer^ pixelData = writableBitmap->PixelBuffer;

    *stride = writableBitmap->PixelWidth * 4;

    ComPtr<IBufferByteAccess> bufferByteAccess;
    reinterpret_cast<IInspectable*>(pixelData)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));
    byte* pixels = nullptr;
    bufferByteAccess->Buffer(&pixels);
    *ptr = pixels;

    // Return IInspectable back to the .mm side
    ComPtr<IInspectable> inspectableByteAccess;
    bufferByteAccess.As(&inspectableByteAccess);
    return inspectableByteAccess;
}

void SetScreenParameters(float width, float height, float magnification, float rotation) {
    windowCollection->Width = width;
    windowCollection->Height = height;
    windowCollection->InvalidateArrange();
    windowCollection->InvalidateMeasure();
    rootNode->InvalidateArrange();
    rootNode->InvalidateMeasure();

    TransformGroup^ globalTransform = ref new TransformGroup();
    ScaleTransform^ windowScale = ref new ScaleTransform();
    RotateTransform^ orientation = ref new RotateTransform();

    windowScale->ScaleX = magnification;
    windowScale->ScaleY = magnification;
    windowScale->CenterX = windowCollection->Width / 2.0;
    windowScale->CenterY = windowCollection->Height / 2.0;

    globalTransform->Children->Append(windowScale);
    if (rotation != 0.0) {
        orientation->Angle = rotation;
        orientation->CenterX = windowCollection->Width / 2.0;
        orientation->CenterY = windowCollection->Height / 2.0;

        globalTransform->Children->Append(orientation);
    }

    windowCollection->RenderTransform = globalTransform;
    CoreAnimation::DisplayProperties::s_screenScale = static_cast<double>(magnification);
}

extern "C" void SetXamlRoot(Windows::UI::Xaml::Controls::Grid^ grid, ActivationType activationType) {
    winobjc::Id gridObj((Platform::Object^)grid);
    CreateXamlCompositor(gridObj, ((activationType == ActivationTypeLibrary) ? CompositionModeLibrary : CompositionModeDefault));
}

void DispatchCompositorTransactions(
    std::deque<std::shared_ptr<ICompositorTransaction>>&& subTransactions,
    std::deque<std::shared_ptr<ICompositorAnimationTransaction>>&& animationTransactions,
    std::map<std::shared_ptr<DisplayNode>, std::map<std::string, std::shared_ptr<ICompositorTransaction>>>&& propertyTransactions,
    std::deque<std::shared_ptr<ICompositorTransaction>>&& movementTransactions) {

    // Walk and process the list of subtransactions
    for (auto& transaction : subTransactions) {
        transaction->Process();
    }

    // Static composition transaction task chain
    static concurrency::task<void> s_compositorTransactions = concurrency::task_from_result();

    // Walk the list of animations and chain each animation task together
    for (auto& animation : animationTransactions) {
        s_compositorTransactions = s_compositorTransactions.then([animation]() noexcept {
            return animation->Process();
        }, concurrency::task_continuation_context::use_current());
    }

    // Walk and process the map of queued properties per DisplayNode and the list of node movements as a single distinct task
    s_compositorTransactions = s_compositorTransactions
        .then([movementTransactions = std::move(movementTransactions),
            propertyTransactions = std::move(propertyTransactions)]() noexcept {
        for (auto& nodeMovement : movementTransactions) {
            nodeMovement->Process();
        }

        for (auto& nodeProperties : propertyTransactions) {
            // Walk the map of queued properties for this DisplayNode
            for (auto& nodeProperty : nodeProperties.second) {
                nodeProperty.second->Process();
            }
        }
    }, concurrency::task_continuation_context::use_current());
}
