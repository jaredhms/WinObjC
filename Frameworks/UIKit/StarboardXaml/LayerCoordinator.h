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

namespace CoreAnimation {

public enum class ContentGravity {
    Resize = 0,
    Center,
    Top,
    ResizeAspect,
    TopLeft,
    BottomLeft,
    Left,
    ResizeAspectFill,
    Bottom,
    TopRight,
    Right,
    BottomRight
};

/////////////////////////////////////////////
// Layer property management
[Windows::Foundation::Metadata::WebHostHidden]
public ref class LayerProperties : public Windows::UI::Xaml::DependencyObject {
internal:
    // Initialize a FrameworkElement for CALayer support
    static void InitializeFrameworkElement(Windows::UI::Xaml::FrameworkElement^ element);

    // CALayer property managment support
    static void SetValue(Windows::UI::Xaml::FrameworkElement^ element, Platform::String^ propertyName, Platform::Object^ value);
    static Platform::Object^ GetValue(Windows::UI::Xaml::FrameworkElement^ element, Platform::String^ propertyName);
    static void AnimateValue(
        Windows::UI::Xaml::FrameworkElement^ element, 
        Windows::UI::Xaml::Media::Animation::Storyboard^ storyboard,
        Windows::UI::Xaml::Media::Animation::DoubleAnimation^ properties,
        Platform::String^ propertyName,
        Platform::Object^ fromValue,
        Platform::Object^ toValue);

    // CALayer content support
    static void SetContent(
        Windows::UI::Xaml::FrameworkElement^ element, 
        Windows::UI::Xaml::Media::ImageSource^ source, 
        float width, 
        float height, 
        float scale);
    static void SetContentCenter(Windows::UI::Xaml::FrameworkElement^ element, Windows::Foundation::Rect rect);

    // CALayer border support
    ////////////////////////////////////
    // TODO: Add border support

///////////////////////////////////////////////////////////////////////////////////
// TODO: EVERYTHING BELOW SHOULD/CAN PROB GO INTO A .CPP FILE AS A HELPER REF CLASS

    // AnchorPoint
    static property Windows::UI::Xaml::DependencyProperty^ AnchorPointProperty {
        Windows::UI::Xaml::DependencyProperty^ get();
    }
    static float GetAnchorPointX(Windows::UI::Xaml::FrameworkElement^ element);
    static void SetAnchorPointX(Windows::UI::Xaml::FrameworkElement^ element, float value);
    static float GetAnchorPointY(Windows::UI::Xaml::FrameworkElement^ element);
    static void SetAnchorPointY(Windows::UI::Xaml::FrameworkElement^ element, float value);
    static Windows::UI::Xaml::Media::TranslateTransform^ GetAnchorTransform(Windows::UI::Xaml::FrameworkElement^ element);
    static Platform::String^ GetAnchorXTransformPath();
    static Platform::String^ GetAnchorYTransformPath();

    // Origin
    static property Windows::UI::Xaml::DependencyProperty^ OriginProperty {
        Windows::UI::Xaml::DependencyProperty^ get();
    }
    static float GetOriginX(Windows::UI::Xaml::FrameworkElement^ element);
    static void SetOriginX(Windows::UI::Xaml::FrameworkElement^ element, float value);
    static float GetOriginY(Windows::UI::Xaml::FrameworkElement^ element);
    static void SetOriginY(Windows::UI::Xaml::FrameworkElement^ element, float value);
    static Windows::UI::Xaml::Media::TranslateTransform^ GetOriginTransform(Windows::UI::Xaml::FrameworkElement^ element);
    static Platform::String^ GetOriginXTransformPath();
    static Platform::String^ GetOriginYTransformPath();

    // Position
    static property Windows::UI::Xaml::DependencyProperty^ PositionProperty {
        Windows::UI::Xaml::DependencyProperty^ get();
    }
    static float GetPositionX(Windows::UI::Xaml::FrameworkElement^ element);
    static void SetPositionX(Windows::UI::Xaml::FrameworkElement^ element, float value);
    static float GetPositionY(Windows::UI::Xaml::FrameworkElement^ element);
    static void SetPositionY(Windows::UI::Xaml::FrameworkElement^ element, float value);
    static Windows::UI::Xaml::Media::TranslateTransform^ GetPositionTransform(Windows::UI::Xaml::FrameworkElement^ element);
    static Platform::String^ GetPositionXTransformPath();
    static Platform::String^ GetPositionYTransformPath();

    // Rotation
    static Windows::UI::Xaml::Media::RotateTransform^ GetRotationTransform(Windows::UI::Xaml::FrameworkElement^ element);
    static Platform::String^ GetRotationTransformPath();

    // Scale
    static Windows::UI::Xaml::Media::ScaleTransform^ GetScaleTransform(Windows::UI::Xaml::FrameworkElement^ element);
    static Platform::String^ GetScaleXTransformPath();
    static Platform::String^ GetScaleYTransformPath();

    // Translation
    static Windows::UI::Xaml::Media::TranslateTransform^ GetTranslationTransform(Windows::UI::Xaml::FrameworkElement^ element);
    static Platform::String^ GetTranslationXTransformPath();
    static Platform::String^ GetTranslationYTransformPath();

    // VisualWidth
    static property Windows::UI::Xaml::DependencyProperty^ VisualWidthProperty {
        Windows::UI::Xaml::DependencyProperty^ get();
    }
    static double GetVisualWidth(Windows::UI::Xaml::FrameworkElement^ element);
    static void SetVisualWidth(Windows::UI::Xaml::FrameworkElement^ element, double value);

    // VisualHeight
    static property Windows::UI::Xaml::DependencyProperty^ VisualHeightProperty {
        Windows::UI::Xaml::DependencyProperty^ get();
    }
    static double GetVisualHeight(Windows::UI::Xaml::FrameworkElement^ element);
    static void SetVisualHeight(Windows::UI::Xaml::FrameworkElement^ element, double value);

    // ContentGravity
    static property Windows::UI::Xaml::DependencyProperty^ ContentGravityProperty {
        Windows::UI::Xaml::DependencyProperty^ get();
    }
    static ContentGravity GetContentGravity(Windows::UI::Xaml::FrameworkElement^ element);
    static void SetContentGravity(Windows::UI::Xaml::FrameworkElement^ element, ContentGravity value);

    // ContentSize
    static property Windows::UI::Xaml::DependencyProperty^ ContentSizeProperty {
        Windows::UI::Xaml::DependencyProperty^ get();
    }

internal:
    // Animation Support
    static void AddAnimation(Platform::String^ propertyName,
                             Windows::UI::Xaml::FrameworkElement^ target,
                             Windows::UI::Xaml::Media::Animation::Storyboard^ storyboard,
                             Windows::UI::Xaml::Media::Animation::DoubleAnimation^ copyProperties,
                             Platform::Object^ fromValue,
                             Platform::Object^ toValue,
                             bool dependent = false);

private:
    static void _registerDependencyProperties();
    static bool s_dependencyPropertiesRegistered;

    static Windows::UI::Xaml::DependencyProperty^ s_anchorPointProperty;
    static Windows::UI::Xaml::DependencyProperty^ s_originProperty;
    static Windows::UI::Xaml::DependencyProperty^ s_positionProperty;
    static Windows::UI::Xaml::DependencyProperty^ s_visualWidthProperty;
    static Windows::UI::Xaml::DependencyProperty^ s_visualHeightProperty;
    static Windows::UI::Xaml::DependencyProperty^ s_contentGravityProperty;
    static Windows::UI::Xaml::DependencyProperty^ s_contentCenterProperty;
    static Windows::UI::Xaml::DependencyProperty^ s_contentSizeProperty;

    // Content helpers
    static Windows::UI::Xaml::Controls::Image^ _GetContentImage(Windows::UI::Xaml::FrameworkElement^ element, bool createIfPossible = false);
    static Windows::Foundation::Rect _GetContentCenter(Windows::UI::Xaml::FrameworkElement^ element);
    static void _ApplyContentCenter(Windows::UI::Xaml::FrameworkElement^ element, Windows::Foundation::Rect contentCenter);
    static void _ApplyContentGravity(Windows::UI::Xaml::FrameworkElement^ element, ContentGravity gravity);
    static Windows::Foundation::Size _GetContentSize(Windows::UI::Xaml::FrameworkElement^ element);
    static void _SetContentSize(Windows::UI::Xaml::FrameworkElement^ element, Windows::Foundation::Size value);

    // TODO: Revisit whether or not we actually need this - it can't be very performant...?
    static void _sizeChangedCallback(Windows::UI::Xaml::DependencyObject^ sender, Windows::UI::Xaml::DependencyPropertyChangedEventArgs^ args);
};

} /* namespace CoreAnimation */

// clang-format on