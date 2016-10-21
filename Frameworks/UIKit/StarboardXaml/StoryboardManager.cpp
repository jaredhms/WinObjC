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
#include "StoryboardManager.h"

#include "DisplayProperties.h"
#include "LayerCoordinator.h"

#include <ErrorHandling.h>

using namespace Platform;
using namespace UIKit::Private::CoreAnimation;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml::Media::Imaging;

static const wchar_t* TAG = L"StoryboardManager";

namespace CoreAnimation {

static const double c_hundredNanoSeconds = 10000000.0;

EventedStoryboard::EventedStoryboard(
    double beginTime, double duration, bool autoReverse, float repeatCount, float repeatDuration, float speed, double timeOffset) {
    TimeSpan beginTimeSpan = TimeSpan();
    beginTimeSpan.Duration = (long long)(beginTime * c_hundredNanoSeconds);
    m_container->BeginTime = beginTimeSpan;
    TimeSpan timeSpan = TimeSpan();
    timeSpan.Duration = (long long)(duration * c_hundredNanoSeconds);
    m_container->Duration = Duration(timeSpan);
    if (repeatCount != 0) {
        m_container->RepeatBehavior = RepeatBehavior(repeatCount);
    }
    if (repeatDuration != 0) {
        TimeSpan timeSpan = TimeSpan();
        timeSpan.Duration = (long long)(repeatDuration * c_hundredNanoSeconds);
        m_container->RepeatBehavior = RepeatBehavior(timeSpan);
    }
    m_container->SpeedRatio = speed;
    m_container->AutoReverse = autoReverse;
    m_container->FillBehavior = FillBehavior::HoldEnd;
    m_container->Completed += ref new EventHandler<Object^>([this](Object^ sender, Object^ args) {
        if (Completed) {
            Completed(this);
        }
        m_container->Stop();
    });
}

void EventedStoryboard::Start() {
    m_container->Begin();
}

void EventedStoryboard::Stop() {
    m_container->Stop();
}

void EventedStoryboard::_CreateFlip(Layer^ layer, bool flipRight, bool invert, bool removeFromParent) {
    if (layer->Projection == nullptr) {
        layer->Projection = ref new PlaneProjection();
    }

    DoubleAnimation^ rotateAnim = ref new DoubleAnimation();
    rotateAnim->Duration = m_container->Duration;

    if (!invert) {
        rotateAnim->From = 0.01;
        if (!flipRight) {
            rotateAnim->To = (double)180;
        } else {
            rotateAnim->To = (double)-180;
        }
    } else {
        if (!flipRight) {
            rotateAnim->From = (double)180;
            rotateAnim->To = (double)360;
        } else {
            rotateAnim->From = (double)-180;
            rotateAnim->To = (double)-360;
        }
    }

    ((PlaneProjection^)layer->Projection)->CenterOfRotationX = LayerProperties::GetVisualWidth(layer) / 2;
    ((PlaneProjection^)layer->Projection)->CenterOfRotationY = LayerProperties::GetVisualHeight(layer) / 2;
    Storyboard::SetTargetProperty(rotateAnim, "(UIElement.Projection).(PlaneProjection.RotationY)");
    Storyboard::SetTarget(rotateAnim, layer);
    m_container->Children->Append(rotateAnim);

    DoubleAnimation^ moveAnim = ref new DoubleAnimation();
    moveAnim->Duration = m_container->Duration;
    moveAnim->From = 0.01;
    moveAnim->To = (double)-160;
    moveAnim->SpeedRatio = 2.0;
    moveAnim->AutoReverse = true;

    Storyboard::SetTarget(moveAnim, layer);
    Storyboard::SetTargetProperty(moveAnim, "(UIElement.Projection).(PlaneProjection.GlobalOffsetZ)");
    m_container->Children->Append(moveAnim);

    DoubleAnimation^ fade1 = ref new DoubleAnimation();
    Storyboard::SetTarget(fade1, layer);
    Storyboard::SetTargetProperty(fade1, "(UIElement.Opacity)");

    if (!invert) {
        TimeSpan fade1TimeSpan = TimeSpan();
        fade1TimeSpan.Duration = m_container->Duration.TimeSpan.Duration / 2;
        fade1->Duration = Duration(fade1TimeSpan);
        fade1->From = 1.0;
        fade1->To = 0.5;
        fade1->FillBehavior = FillBehavior::HoldEnd;
    } else {
        TimeSpan fade1TimeSpan = TimeSpan();
        fade1TimeSpan.Duration = m_container->Duration.TimeSpan.Duration / 2;
        fade1->Duration = Duration(fade1TimeSpan);
        TimeSpan fade1BeginTimeSpan = TimeSpan();
        fade1BeginTimeSpan.Duration = m_container->Duration.TimeSpan.Duration / 2;
        fade1->BeginTime = fade1BeginTimeSpan;
        fade1->From = 0.5;
        fade1->To = 1.0;
        fade1->FillBehavior = FillBehavior::HoldEnd;

        fade1->Completed += ref new EventHandler<Object^>([layer](Object^ sender, Object^ args) { layer->Opacity = 1.0; });
    }

    if (removeFromParent) {
        fade1->Completed += ref new EventHandler<Object^>([layer](Object^ sender, Object^ args) {
            VisualTreeHelper::DisconnectChildrenRecursive(layer);
        });
    } else {
        rotateAnim->Completed += ref new EventHandler<Object^>([layer](Object^ sender, Object^ args) {
            // Using Projection transforms (even Identity) causes less-than-pixel-perfect rendering.
            layer->Projection = nullptr;
        });
    }

    m_container->Children->Append(fade1);
}

void EventedStoryboard::_CreateWoosh(Layer^ layer, bool fromRight, bool invert, bool removeFromParent) {
    if (layer->Projection == nullptr) {
        layer->Projection = ref new PlaneProjection();
    }

    DoubleAnimation^ wooshAnim = ref new DoubleAnimation();
    wooshAnim->Duration = m_container->Duration;
    wooshAnim->EasingFunction = ref new PowerEase();
    wooshAnim->EasingFunction->EasingMode = EasingMode::EaseOut;

    if (!invert) {
        if (fromRight) {
            wooshAnim->From = (double)LayerProperties::GetVisualWidth(layer);
            wooshAnim->To = 0.01;
        } else {
            wooshAnim->From = 0.01;
            wooshAnim->To = (double)LayerProperties::GetVisualWidth(layer);
        }
    } else {
        if (fromRight) {
            wooshAnim->From = 0.01;
            wooshAnim->To = (double)(-LayerProperties::GetVisualWidth(layer) / 4);
        } else {
            wooshAnim->From = (double)(-LayerProperties::GetVisualWidth(layer) / 4);
            wooshAnim->To = 0.01;
        }
    }

    Storyboard::SetTargetProperty(wooshAnim, "(UIElement.Projection).(PlaneProjection.LocalOffsetX)");
    Storyboard::SetTarget(wooshAnim, layer);

    if (removeFromParent) {
        wooshAnim->Completed += ref new EventHandler<Object^>([layer](Object^ sender, Object^ args) {
            VisualTreeHelper::DisconnectChildrenRecursive(layer);
        });
    } else {
        wooshAnim->Completed += ref new EventHandler<Object^>([layer](Object^ sender, Object^ args) {
            // Using Projection transforms (even Identity) causes less-than-pixel-perfect rendering.
            layer->Projection = nullptr;
        });
    }

    m_container->Children->Append(wooshAnim);
}

////////////////////////////////////////////////////////
// TODO: MOVE TO STATIC FUNCTION ON LAYERANIMATION?
concurrency::task<Layer^> EventedStoryboard::SnapshotLayer(Layer^ layer) {
    if (((layer->Height == 0) && (layer->Width == 0)) || (layer->Opacity == 0)) {
        return concurrency::task_from_result<Layer^>(nullptr);
    }
    else {
        RenderTargetBitmap^ snapshot = ref new RenderTargetBitmap();
        return concurrency::create_task(
                snapshot->RenderAsync(layer, static_cast<int>(LayerProperties::GetVisualWidth(layer) * DisplayProperties::RawScreenScale()), 0))
            .then([snapshot, layer](concurrency::task<void> result) noexcept {
            try {
                result.get();
            } catch (Platform::InvalidArgumentException^ ex) {
                // Handle exceptions related to invalid layer attribute passed to RenderAsync
                TraceWarning(TAG,
                             L"RenderAsync threw InvalidArgumentException exception - [%ld]%s",
                             ex->HResult,
                             ex->Message);
                return static_cast<Layer^>(nullptr);
            }

            // Return a new 'copy' layer with the rendered content
            Layer^ newLayer = ref new Layer();
            LayerProperties::InitializeFrameworkElement(newLayer);

            // Copy display properties from the old layer to the new layer
            LayerProperties::SetValue(newLayer, "opacity", LayerProperties::GetValue(layer, "opacity"));
            LayerProperties::SetValue(newLayer, "position", LayerProperties::GetValue(layer, "position"));
            LayerProperties::SetValue(newLayer, "size", LayerProperties::GetValue(layer, "size"));
            LayerProperties::SetValue(newLayer, "anchorPoint", LayerProperties::GetValue(layer, "anchorPoint"));

            int width = snapshot->PixelWidth;
            int height = snapshot->PixelHeight;
            auto dispInfo = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();

            // Set the snapshot as the content of the new layer
            LayerProperties::SetContent(
                newLayer, 
                snapshot,
                static_cast<float>(width),
                static_cast<float>(height), 
                static_cast<float>(DisplayProperties::RawScreenScale() * dispInfo->RawPixelsPerViewPixel));

            // There seems to be a bug in Xaml where Render'd layers get sized to their visible content... sort of.
            // If the UIViewController being transitioned away from has transparent content, the height returned is less the
            // navigation bar, as though Xaml sizes the buffer to the largest child Visual, and only expands where needed.
            // Top/bottom switched due to geometric origin of CALayer so read this as UIViewContentModeTopLeft
            LayerProperties::SetContentGravity(newLayer, ContentGravity::BottomLeft);

            return newLayer;
        }, concurrency::task_continuation_context::use_current());
    }
}
////////////////////////////////////////////////////////

void EventedStoryboard::AddTransition(Layer^ realLayer, Layer^ snapshotLayer, String^ type, String^ subtype) {
    if (type == "kCATransitionFlip") {
        TimeSpan timeSpan = TimeSpan();
        timeSpan.Duration = (long long)(0.75 * c_hundredNanoSeconds);
        m_container->Duration = Duration(timeSpan);
        
        auto wtf = VisualTreeHelper::GetParent(realLayer);

        Panel^ parent = (Panel^)VisualTreeHelper::GetParent(realLayer);

        bool flipToLeft = true;
        if (subtype != "kCATransitionFromLeft") {
            flipToLeft = false;
        }

        // We don't need to animate a snapshot if it doesn't exist
        if (snapshotLayer) {
            unsigned int idx;
            parent->Children->IndexOf(realLayer, &idx);
            parent->Children->InsertAt(idx + 1, snapshotLayer);
            parent->InvalidateArrange();
            realLayer->Opacity = 0;
            _CreateFlip(snapshotLayer, flipToLeft, false, true);
        }

        _CreateFlip(realLayer, flipToLeft, true, false);
    }
    else {
        TimeSpan timeSpan = TimeSpan();
        timeSpan.Duration = (long long)(0.5 * c_hundredNanoSeconds);
        m_container->Duration = Duration(timeSpan);
        Panel^ parent = (Panel^)VisualTreeHelper::GetParent(realLayer);

        bool fromRight = true;
        if (subtype == "kCATransitionFromLeft") {
            fromRight = false;
        }

        if (fromRight) {
            // We don't need to animate a snapshot if it doesn't exist
            if (snapshotLayer) {
                unsigned int idx;
                parent->Children->IndexOf(realLayer, &idx);
                parent->Children->InsertAt(idx, snapshotLayer);
                parent->InvalidateArrange();
                _CreateWoosh(snapshotLayer, fromRight, true, true);
            }

            _CreateWoosh(realLayer, fromRight, false, false);
        }
        else {
            // We don't need to animate a snapshot if it doesn't exist
            if (snapshotLayer) {
                unsigned int idx;
                parent->Children->IndexOf(realLayer, &idx);
                parent->Children->InsertAt(idx + 1, snapshotLayer);
                parent->InvalidateArrange();
                _CreateWoosh(snapshotLayer, fromRight, false, true);
            }

            _CreateWoosh(realLayer, fromRight, true, false);
        }
    }
}

void EventedStoryboard::Animate(FrameworkElement^ layer, String^ propertyName, Object^ from, Object^ to) {
    DoubleAnimation^ timeline = ref new DoubleAnimation();
    timeline->Duration = m_container->Duration;
    timeline->EasingFunction = m_animationEase;
    LayerProperties::AnimateValue(layer, m_container, timeline, propertyName, from, to);
}

} /* namespace CoreAnimation */

// clang-format on