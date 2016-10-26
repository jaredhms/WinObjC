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
#pragma once

#include <ppltasks.h>

namespace CoreAnimation {

delegate void AnimationMethod(Platform::Object^ sender);

[Windows::Foundation::Metadata::WebHostHidden]
ref class EventedStoryboard sealed {
    ref class Animation {
    internal:
        Platform::String^ propertyName = nullptr;
        Platform::Object^ toValue = nullptr;
    };

    enum class EasingFunction { EastInEaseIn, Linear };

public:
    property AnimationMethod^ Completed {
        AnimationMethod^ get() {
            return m_completed;
        }
        void set(AnimationMethod^ value) {
            m_completed = value;
        }
    };

    property Windows::UI::Xaml::Media::Animation::EasingFunctionBase^ AnimationEase {
        Windows::UI::Xaml::Media::Animation::EasingFunctionBase^ get() {
            return m_animationEase;
        }
        void set(Windows::UI::Xaml::Media::Animation::EasingFunctionBase^ value) {
            m_animationEase = value;
        }
    };

    EventedStoryboard(
        double beginTime, double duration, bool autoReverse, float repeatCount, float repeatDuration, float speed, double timeOffset);
    void Start();
    void Stop();
    void Animate(Windows::UI::Xaml::FrameworkElement^ layer, Platform::String^ propertyName, Platform::Object^ from, Platform::Object^ to);

internal:
    AnimationMethod^ m_completed;
    AnimationMethod^ m_started;

    concurrency::task<UIKit::Private::CoreAnimation::Layer^> SnapshotLayer(UIKit::Private::CoreAnimation::Layer^ layer);
    void AddTransition(
        UIKit::Private::CoreAnimation::Layer^ realLayer,
        UIKit::Private::CoreAnimation::Layer^ snapshotLayer,
        Platform::String^ type, 
        Platform::String^ subtype);

private:
    void _CreateFlip(UIKit::Private::CoreAnimation::Layer^ layer, bool flipRight, bool invert, bool removeFromParent);
    void _CreateWoosh(UIKit::Private::CoreAnimation::Layer^ layer, bool fromRight, bool invert, bool removeFromParent);

    Windows::UI::Xaml::Media::Animation::Storyboard^ m_container = ref new Windows::UI::Xaml::Media::Animation::Storyboard();
    Windows::UI::Xaml::Media::Animation::EasingFunctionBase^ m_animationEase;
};

} /* namespace CoreAnimation */

// clang-format on