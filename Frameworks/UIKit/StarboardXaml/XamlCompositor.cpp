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

#include "winobjc\winobjc.h"
#include "ApplicationCompositor.h"
#include "CompositorInterface.h"
#include <StringHelpers.h>

#include "DisplayProperties.h"
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

void (*RenderCallback)() = nullptr;

ref class XamlRenderingListener sealed {
private:
    bool _isListening = false;
    Windows::Foundation::EventHandler<Platform::Object^>^ listener;
    Windows::Foundation::EventRegistrationToken listenerToken;
internal: 

    void RenderedFrame(Object^ sender, Object^ args) {
        if (RenderCallback) {
            RenderCallback();
        }
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
}

void SetXamlRoot(Windows::UI::Xaml::Controls::Grid^ grid, ActivationType activationType) {
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

    // Walk and process the map of queued properties per ILayerProxy and the list of layer movements as a single distinct task
    s_compositorTransactions = s_compositorTransactions
        .then([movementTransactions = std::move(movementTransactions),
            propertyTransactions = std::move(propertyTransactions)]() noexcept {
        for (auto& layerMovement : movementTransactions) {
            layerMovement->Process();
        }

        for (auto& layerProperties : propertyTransactions) {
            // Walk the map of queued properties for this ILayerProxy
            for (auto& layerProperty : layerProperties.second) {
                layerProperty.second->Process();
            }
        }
    }, concurrency::task_continuation_context::use_current());
}
