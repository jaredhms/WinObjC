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

#include "LayerProxy.h"

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

__inline FrameworkElement^ _GetXamlElement(ILayerProxy& node) {
    return dynamic_cast<FrameworkElement^>(reinterpret_cast<Platform::Object^>(reinterpret_cast<LayerProxy*>(&node)->GetXamlElement().Get()));
};

concurrency::task<void> DisplayAnimation::AddAnimation(ILayerProxy& node, const wchar_t* propertyName, bool fromValid, float from, bool toValid, float to) {
    auto xamlNode = _GetXamlElement(node);
    auto xamlAnimation = GetStoryboard(this);

    xamlAnimation->Animate(xamlNode,
                           ref new Platform::String(propertyName),
                           fromValid ? (Platform::Object^)(double)from : nullptr,
                           toValid ? (Platform::Object^)(double)to : nullptr);

    return concurrency::task_from_result();
}

concurrency::task<void> DisplayAnimation::AddTransitionAnimation(ILayerProxy& node, const char* type, const char* subtype) {
    auto xamlNode = _GetXamlElement(node);
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
    std::map<std::shared_ptr<ILayerProxy>, std::map<std::string, std::shared_ptr<ICompositorTransaction>>>&& propertyTransactions,
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

    // Walk and process the map of queued properties per ILayerProxy and the list of node movements as a single distinct task
    s_compositorTransactions = s_compositorTransactions
        .then([movementTransactions = std::move(movementTransactions),
            propertyTransactions = std::move(propertyTransactions)]() noexcept {
        for (auto& nodeMovement : movementTransactions) {
            nodeMovement->Process();
        }

        for (auto& nodeProperties : propertyTransactions) {
            // Walk the map of queued properties for this ILayerProxy
            for (auto& nodeProperty : nodeProperties.second) {
                nodeProperty.second->Process();
            }
        }
    }, concurrency::task_continuation_context::use_current());
}
