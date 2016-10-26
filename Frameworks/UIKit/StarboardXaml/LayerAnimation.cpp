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

#include "LayerAnimation.h"
#include "LayerProxy.h"
#include "StoryboardManager.h"

#include <ErrorHandling.h>

using namespace Microsoft::WRL;
using namespace UIKit::Private::CoreAnimation;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;

static const wchar_t* TAG = L"LayerAnimation";

__inline CoreAnimation::EventedStoryboard^ _GetStoryboard(const Microsoft::WRL::ComPtr<IInspectable>& xamlAnimation) {
    return dynamic_cast<CoreAnimation::EventedStoryboard^>(reinterpret_cast<Platform::Object^>(xamlAnimation.Get()));
}

__inline FrameworkElement^ _GetXamlElement(ILayerProxy& layer) {
    return dynamic_cast<FrameworkElement^>(reinterpret_cast<Platform::Object^>(reinterpret_cast<LayerProxy*>(&layer)->GetXamlElement().Get()));
};

LayerAnimation::LayerAnimation() {
    easingFunction = EaseInEaseOut;
}

void LayerAnimation::_CreateXamlAnimation() {
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

    _xamlAnimation = reinterpret_cast<IInspectable*>(xamlAnimation);
}

void LayerAnimation::_Start() {
    CoreAnimation::EventedStoryboard^ xamlAnimation = _GetStoryboard(_xamlAnimation);

    std::weak_ptr<LayerAnimation> weakThis = shared_from_this();
    xamlAnimation->Completed = ref new CoreAnimation::AnimationMethod([weakThis](Platform::Object^ sender) {
        std::shared_ptr<LayerAnimation> strongThis = weakThis.lock();
        if (strongThis) {
            strongThis->_Completed();
            CoreAnimation::EventedStoryboard^ xamlAnimation = _GetStoryboard(strongThis->_xamlAnimation);
            xamlAnimation->Completed = nullptr;
        }
    });

    xamlAnimation->Start();
}

void LayerAnimation::Stop() {
    CoreAnimation::EventedStoryboard^ xamlAnimation = _GetStoryboard(_xamlAnimation);
    xamlAnimation->Stop();
    xamlAnimation->Completed = nullptr;
    _xamlAnimation = nullptr;
}

concurrency::task<void> LayerAnimation::_AddAnimation(ILayerProxy& layer, const wchar_t* propertyName, bool fromValid, float from, bool toValid, float to) {
    auto xamlLayer = _GetXamlElement(layer);
    auto xamlAnimation = _GetStoryboard(_xamlAnimation);

    xamlAnimation->Animate(xamlLayer,
        ref new Platform::String(propertyName),
        fromValid ? (Platform::Object^)(double)from : nullptr,
        toValid ? (Platform::Object^)(double)to : nullptr);

    return concurrency::task_from_result();
}

concurrency::task<void> LayerAnimation::_AddTransitionAnimation(ILayerProxy& layer, const char* type, const char* subtype) {
    auto xamlLayer = _GetXamlElement(layer);
    auto xamlAnimation = _GetStoryboard(_xamlAnimation);

    std::string stype(type);
    std::wstring wtype(stype.begin(), stype.end());
    std::string ssubtype(subtype);
    std::wstring wsubtype(ssubtype.begin(), ssubtype.end());

    // We currently only support layer snapshots/transitions on our default Layer implementation
    auto coreAnimationLayer = dynamic_cast<Layer^>(xamlLayer);
    if (!coreAnimationLayer) {
        UNIMPLEMENTED_WITH_MSG(
            "Layer transition animations not supported on this Xaml element: [%ws].",
            xamlLayer->GetType()->FullName->Data());
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

        _Start();
    }, concurrency::task_continuation_context::use_current());
}