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

#include "LayerCoordinator.h"
#include "LayerProxy.h"

#include <ErrorHandling.h>

#include <map>

using namespace Platform;
using namespace UIKit::Private::CoreAnimation;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml::Media::Imaging;

static const wchar_t* TAG = L"LayerCoordinator";

static const bool DEBUG_ALL = true;
static const bool DEBUG_POSITION = DEBUG_ALL || false;
static const bool DEBUG_ANCHORPOINT = DEBUG_ALL || false;
static const bool DEBUG_SIZE = DEBUG_ALL || false;

namespace CoreAnimation {

// Provides support for setting, animating and retrieving values.
delegate void ApplyAnimationFunction(FrameworkElement^ target,
    Storyboard^ storyboard,
    DoubleAnimation^ properties,
    Object^ fromValue,
    Object^ toValue);
delegate void ApplyTransformFunction(FrameworkElement^ target, Object^ toValue);
delegate Object^ GetCurrentValueFunction(FrameworkElement^ target);

/////////////////////////////////////////////////////////
// TODO: SWITCH TO STD::FUNCTIONS
/////////////////////////////////////////////////////////
ref class AnimatableProperty sealed {
public:
    property ApplyAnimationFunction^ AnimateValue;
    property ApplyTransformFunction^ SetValue;
    property GetCurrentValueFunction^ GetValue;

    AnimatableProperty(ApplyAnimationFunction^ animate, ApplyTransformFunction^ set, GetCurrentValueFunction^ get) {
        AnimateValue = animate;
        SetValue = set;
        GetValue = get;
    };
};

std::map<String^, AnimatableProperty^> s_animatableProperties = {
    { "position.x",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              // Animate the PositionTransform
              LayerProperties::AddAnimation(
                  LayerProperties::GetPositionXTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from,
                  to);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              auto positionX = static_cast<double>(toValue);

              // Store PositionX
              LayerProperties::SetPositionX(target, static_cast<float>(positionX));

              // Update the PositionTransform
              LayerProperties::GetPositionTransform(target)->X = positionX;
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return LayerProperties::GetPositionTransform(target)->X;
          })) },
    { "position.y",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              // Animate the PositionTransform
              LayerProperties::AddAnimation(
                  LayerProperties::GetPositionYTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from,
                  to);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              auto positionY = static_cast<double>(toValue);

              // Store PositionY
              LayerProperties::SetPositionY(target, static_cast<float>(positionY));

              // Update the PositionTransform
              LayerProperties::GetPositionTransform(target)->Y = positionY;
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return LayerProperties::GetPositionTransform(target)->Y;
          })) },
    { "position",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              s_animatableProperties["position.x"]->AnimateValue(target,
                                                            storyboard,
                                                            properties,
                                                            from ? (Object^)((Point)from).X : nullptr,
                                                            (double)((Point)to).X);
              s_animatableProperties["position.y"]->AnimateValue(target,
                                                            storyboard,
                                                            properties,
                                                            from ? (Object^)((Point)from).Y : nullptr,
                                                            (double)((Point)to).Y);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              s_animatableProperties["position.x"]->SetValue(target, (double)((Point)toValue).X);
              s_animatableProperties["position.y"]->SetValue(target, (double)((Point)toValue).Y);
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              // Unbox x and y values as doubles, before casting them to floats
              return Point((float)(double)LayerProperties::GetValue(target, "position.x"),
                           (float)(double)LayerProperties::GetValue(target, "position.y"));
          })) },
    { "origin.x",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              // Zero out negative and null values
              auto fromValue = from ? std::max<double>(0.0, static_cast<double>(from)) : 0.0;
              auto toValue = to ? std::max<double>(0.0, static_cast<double>(to)) : 0.0;

              LayerProperties::AddAnimation(
                  LayerProperties::GetOriginXTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from ? static_cast<Object^>(-fromValue) : nullptr,
                  -toValue);

              // Animate the Clip transform
              if (target->Clip) {
                  LayerProperties::AddAnimation(
                      "(UIElement.Clip).(Transform).(TranslateTransform.X)",
                      target,
                      storyboard,
                      properties,
                      from ? static_cast<Object^>(fromValue) : nullptr,
                      toValue);
              }
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              auto targetValue = static_cast<double>(toValue);

              // Store the new OriginX value
              LayerProperties::SetOriginX(target, static_cast<float>(targetValue));

              // Update the OriginTransform
              LayerProperties::GetOriginTransform(target)->X = -targetValue;

              // Update the Clip transform
              if (target->Clip) {
                  ((TranslateTransform^)target->Clip->Transform)->X = targetValue;
              }
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return -(LayerProperties::GetOriginTransform(target)->X);
          })) },
    { "origin.y",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              // Zero out negative and null values
              auto fromValue = from ? std::max<double>(0.0, static_cast<double>(from)) : 0.0;
              auto toValue = to ? std::max<double>(0.0, static_cast<double>(to)) : 0.0;

              LayerProperties::AddAnimation(
                  LayerProperties::GetOriginYTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from ? static_cast<Object^>(-fromValue) : nullptr,
                  -toValue);

              // Animate the Clip transform
              if (target->Clip) {
                  LayerProperties::AddAnimation(
                      "(UIElement.Clip).(Transform).(TranslateTransform.Y)",
                      target,
                      storyboard,
                      properties,
                      from ? static_cast<Object^>(fromValue) : nullptr,
                      toValue);
              }
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              auto targetValue = static_cast<double>(toValue);

              // Store the new OriginY value
              LayerProperties::SetOriginY(target, static_cast<float>(targetValue));

              // Update the OriginTransform
              LayerProperties::GetOriginTransform(target)->Y = -targetValue;

              // Update the Clip transform
              if (target->Clip) {
                  ((TranslateTransform^)target->Clip->Transform)->Y = targetValue;
              }
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return -(LayerProperties::GetOriginTransform(target)->Y);
          })) },
    { "origin",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              s_animatableProperties["origin.x"]->AnimateValue(target,
                                                          storyboard,
                                                          properties,
                                                          (from != nullptr) ? (Object^)((Point)from).X : nullptr,
                                                          (double)((Point)to).X);
              s_animatableProperties["origin.y"]->AnimateValue(target,
                                                          storyboard,
                                                          properties,
                                                          (from != nullptr) ? (Object^)((Point)from).Y : nullptr,
                                                          (double)((Point)to).Y);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              s_animatableProperties["origin.x"]->SetValue(target, (double)((Point)toValue).X);
              s_animatableProperties["origin.y"]->SetValue(target, (double)((Point)toValue).Y);
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              // Unbox x and y values as doubles, before casting them to floats
              return Point((float)(double)LayerProperties::GetValue(target, "origin.x"),
                           (float)(double)LayerProperties::GetValue(target, "origin.y"));
          })) },
    { "anchorPoint.x",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              // Zero out negative and null values
              auto fromValue = from ? std::max<double>(0.0, static_cast<double>(from)) : 0.0;
              auto toValue = to ? std::max<double>(0.0, static_cast<double>(to)) : 0.0;

              // Calculate values to animate on the AnchorPoint transform
              fromValue = -target->Width * fromValue;
              toValue = -target->Width * toValue;
              LayerProperties::AddAnimation(
                  LayerProperties::GetAnchorXTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from ? static_cast<Object^>(fromValue) : nullptr,
                  toValue);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              // Store the new anchorPointX value
              auto anchorPointX = static_cast<double>(toValue);
              LayerProperties::SetAnchorPointX(target, static_cast<float>(anchorPointX));

              // Calculate and update the AnchorPoint transform
              double destX = -target->Width * anchorPointX;
              LayerProperties::GetAnchorTransform(target)->X = destX;
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return static_cast<double>(LayerProperties::GetAnchorPointX(target));
          })) },
    { "anchorPoint.y",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              // Zero out negative and null values
              auto fromValue = from ? std::max<double>(0.0, static_cast<double>(from)) : 0.0;
              auto toValue = to ? std::max<double>(0.0, static_cast<double>(to)) : 0.0;

              // Calculate values to animate on the AnchorPoint transform
              fromValue = -target->Height * fromValue;
              toValue = -target->Height * toValue;
              LayerProperties::AddAnimation(
                  LayerProperties::GetAnchorYTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from ? static_cast<Object^>(fromValue) : nullptr,
                  toValue);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              // Store the new anchorPointY value
              auto anchorPointY = static_cast<double>(toValue);
              LayerProperties::SetAnchorPointY(target, static_cast<float>(anchorPointY));

              // Calculate and update the AnchorPoint transform
              double destY = -target->Height * anchorPointY;
              LayerProperties::GetAnchorTransform(target)->Y = destY;
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return static_cast<double>(LayerProperties::GetAnchorPointY(target));
          })) },
    { "anchorPoint",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              s_animatableProperties["anchorPoint.x"]->AnimateValue(target,
                                                               storyboard,
                                                               properties,
                                                               (from != nullptr) ? (Object^)((Point)from).X : nullptr,
                                                               (double)((Point)to).X);
              s_animatableProperties["anchorPoint.y"]->AnimateValue(target,
                                                               storyboard,
                                                               properties,
                                                               (from != nullptr) ? (Object^)((Point)from).Y : nullptr,
                                                               (double)((Point)to).Y);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              LayerProperties::SetValue(target, "anchorPoint.x", static_cast<double>(static_cast<Point>(toValue).X));
              LayerProperties::SetValue(target, "anchorPoint.y", static_cast<double>(static_cast<Point>(toValue).Y));
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              // Unbox x and y values as doubles, before casting them to floats
              return Point(static_cast<float>(static_cast<double>(LayerProperties::GetValue(target, "anchorPoint.x"))),
                           static_cast<float>(static_cast<double>(LayerProperties::GetValue(target, "anchorPoint.y"))));
          })) },
    { "size.width",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              // Zero out negative and null values
              auto fromValue = from ? std::max<double>(0.0, static_cast<double>(from)) : 0.0;
              auto toValue = to ? std::max<double>(0.0, static_cast<double>(to)) : 0.0;

              // Calculate values to animate on the AnchorPoint transform
              // TODO: SHOULD WE JUST ANIMATE THEM DIRECTLY RATHER THAN DUPLICATING THIS LOGIC, OR PULL OUT TO SHARED LOCATION?
              float anchorPointX = LayerProperties::GetAnchorPointX(target);
              fromValue = (-(fromValue) * anchorPointX);
              toValue = (-(toValue) * anchorPointX);
              LayerProperties::AddAnimation(
                  LayerProperties::GetAnchorXTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from ? static_cast<Object^>(fromValue) : nullptr,
                  toValue);

              ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
              // Animate the VisualWidth property to the new value
              // TODO: Why is this necessary - is it only necessary to re-render every frame?
              LayerProperties::AddAnimation("(LayerProperties.VisualWidth)", target, storyboard, properties, from, to, true);
              ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              // Update our width
              auto toWidth = std::max<double>(0.0, static_cast<double>(toValue));
              target->Width = toWidth;

              // Update the AnchorPoint transform
              double destX = -(toWidth) * LayerProperties::GetAnchorPointX(target);
              LayerProperties::GetAnchorTransform(target)->X = destX;

              auto anchorX = LayerProperties::GetAnchorTransform(target)->X;
              auto anchorY = LayerProperties::GetAnchorTransform(target)->Y;

              ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
              // Update the VisualWidth property
              // TODO: Why is this necessary - is it only necessary to re-render every frame?
              LayerProperties::SetVisualWidth(target, toWidth);
              ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

              // Update the Clip rect if necessary
              if (target->Clip) {
                  Rect clipRect = target->Clip->Rect;
                  clipRect.Width = static_cast<float>(toWidth);
                  target->Clip->Rect = clipRect;
              }
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return LayerProperties::GetVisualWidth(target);
          })) },
    { "size.height",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              // Zero out negative and null values
              auto fromValue = from ? std::max<double>(0.0, static_cast<double>(from)) : 0.0;
              auto toValue = to ? std::max<double>(0.0, static_cast<double>(to)) : 0.0;

              // Calculate values to animate on the AnchorPoint transform
              // TODO: SHOULD WE JUST ANIMATE THEM DIRECTLY RATHER THAN DUPLICATING THIS LOGIC, OR PULL OUT TO SHARED LOCATION?
              float anchorPointY = LayerProperties::GetAnchorPointY(target);
              fromValue = (-(fromValue) * anchorPointY);
              toValue = (-(toValue) * anchorPointY);
              LayerProperties::AddAnimation(
                  LayerProperties::GetAnchorYTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from ? static_cast<Object^>(fromValue) : nullptr,
                  toValue);

              //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
              // Animate the VisualHeight property to the new value
              // TODO: Why is this necessary - is it only necessary to re-render every frame?
              LayerProperties::AddAnimation("(LayerProperties.VisualHeight)", target, storyboard, properties, from, to, true);
              //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              // Update our height
              auto toHeight = std::max<double>(0.0, static_cast<double>(toValue));
              target->Height = toHeight;

              // Update the AnchorPoint transform
              double destY = -(toHeight) * LayerProperties::GetAnchorPointY(target);
              LayerProperties::GetAnchorTransform(target)->Y = destY;

              //////////////////////////////////////////////////////////////////////////////////
              // Update the VisualHeight property
              // TODO: Why is this necessary - is it only necessary to re-render every frame?
              LayerProperties::SetVisualHeight(target, toHeight);
              //////////////////////////////////////////////////////////////////////////////////

              // Update the Clip rect if necessary
              if (target->Clip) {
                  Rect clipRect = target->Clip->Rect;
                  clipRect.Height = static_cast<float>(toHeight);
                  target->Clip->Rect = clipRect;
              }
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return LayerProperties::GetVisualHeight(target);
          })) },
    { "size",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              s_animatableProperties["size.width"]->AnimateValue(target,
                                                            storyboard,
                                                            properties,
                                                            (from != nullptr) ? (Object^)((Size)from).Width : nullptr,
                                                            (double)((Size)to).Width);
              s_animatableProperties["size.height"]->AnimateValue(target,
                                                             storyboard,
                                                             properties,
                                                             (from != nullptr) ? (Object^)((Size)from).Height : nullptr,
                                                             (double)((Size)to).Height);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              s_animatableProperties["size.width"]->SetValue(target, (double)((Size)toValue).Width);
              s_animatableProperties["size.height"]->SetValue(target, (double)((Size)toValue).Height);
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              // Unbox width and height values as doubles, before casting them to floats
              return Size((float)(double)LayerProperties::GetValue(target, "size.width"),
                          (float)(double)LayerProperties::GetValue(target, "size.height"));
          })) },
    { "transform.rotation",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              LayerProperties::AddAnimation(
                  LayerProperties::GetRotationTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from,
                  to);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              LayerProperties::GetRotationTransform(target)->Angle = static_cast<double>(toValue);
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return LayerProperties::GetRotationTransform(target)->Angle;
          })) },
    { "transform.scale.x",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              LayerProperties::AddAnimation(
                  LayerProperties::GetScaleXTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from,
                  to);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              LayerProperties::GetScaleTransform(target)->ScaleX = static_cast<double>(toValue);
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return LayerProperties::GetScaleTransform(target)->ScaleX;
          })) },
    { "transform.scale.y",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              LayerProperties::AddAnimation(
                  LayerProperties::GetScaleYTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from,
                  to);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              LayerProperties::GetScaleTransform(target)->ScaleY = static_cast<double>(toValue);
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return LayerProperties::GetScaleTransform(target)->ScaleY;
          })) },
    { "transform.translation.x",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              LayerProperties::AddAnimation(
                  LayerProperties::GetTranslationXTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from,
                  to);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              LayerProperties::GetTranslationTransform(target)->X = static_cast<double>(toValue);
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return LayerProperties::GetTranslationTransform(target)->X;
          })) },
    { "transform.translation.y",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              LayerProperties::AddAnimation(
                  LayerProperties::GetTranslationYTransformPath(),
                  target,
                  storyboard,
                  properties,
                  from,
                  to);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              LayerProperties::GetTranslationTransform(target)->Y = static_cast<double>(toValue);
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ {
              return LayerProperties::GetTranslationTransform(target)->Y;
          })) },
    { "opacity",
      ref new AnimatableProperty(
          ref new ApplyAnimationFunction([](FrameworkElement^ target, Storyboard^ storyboard, DoubleAnimation^ properties, Object^ from, Object^ to) {
              LayerProperties::AddAnimation("(UIElement.Opacity)", target, storyboard, properties, from, to);
          }),
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              target->Opacity = static_cast<double>(toValue);
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ { 
              return target->Opacity; 
          })) },
    { "gravity",
      ref new AnimatableProperty(
          nullptr,
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              LayerProperties::SetContentGravity(target, static_cast<ContentGravity>(static_cast<int>(toValue)));
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ { 
              return static_cast<int>(LayerProperties::GetContentGravity(target));
          })) },
    { "masksToBounds",
      ref new AnimatableProperty(nullptr,
          ref new ApplyTransformFunction([](FrameworkElement^ target, Object^ toValue) {
              bool masksToBounds = static_cast<bool>(toValue);
              if (masksToBounds) {
                  // Set up our Clip geometry based on the visual/transformed width/height of the target
                  // TODO: Why can't we just use Width/Height here??
                  auto clipGeometry = ref new RectangleGeometry();
                  clipGeometry->Rect = Rect(
                      0, 
                      0, 
                      static_cast<float>(LayerProperties::GetVisualWidth(target)), 
                      static_cast<float>(LayerProperties::GetVisualHeight(target)));

                  // Set up its origin transform
                  auto clipOriginTransform = ref new TranslateTransform();
                  clipOriginTransform->X = LayerProperties::GetOriginX(target);
                  clipOriginTransform->Y = LayerProperties::GetOriginY(target);
                  clipGeometry->Transform = clipOriginTransform;

                  target->Clip = clipGeometry;
              } else {
                  // Clear out the Clip geometry
                  target->Clip = nullptr;
              }
          }),
          ref new GetCurrentValueFunction([](FrameworkElement^ target) -> Object^ { 
              return target->Clip != nullptr; 
          })) },
};

// CALayer property management plumbing
bool LayerProperties::s_dependencyPropertiesRegistered = false;
DependencyProperty^ LayerProperties::s_anchorPointProperty = nullptr;
DependencyProperty^ LayerProperties::s_originProperty = nullptr;
DependencyProperty^ LayerProperties::s_positionProperty = nullptr;
DependencyProperty^ LayerProperties::s_visualWidthProperty = nullptr;
DependencyProperty^ LayerProperties::s_visualHeightProperty = nullptr;
DependencyProperty^ LayerProperties::s_contentGravityProperty = nullptr;
DependencyProperty^ LayerProperties::s_contentCenterProperty = nullptr;
DependencyProperty^ LayerProperties::s_contentSizeProperty = nullptr;

void LayerProperties::InitializeFrameworkElement(FrameworkElement^ element) {
    // No-op if already registered
    _registerDependencyProperties();

    // Make sure this FrameworkElement is hit-testable by default
    element->IsHitTestVisible = true;

    ////////////////////////////////////////////////////////////////
    // Set up all necessary RenderTransforms for CALayer support.
    // The order of transforms is critical.
    ////////////////////////////////////////////////////////////////

    // Grab Width/Height
    // These likely won't be set yet, but if not, we'll use 0 until the layer is resized
    auto width = (std::isnan(element->Width) ? 0.0 : element->Width);
    auto height = (std::isnan(element->Height) ? 0.0 : element->Height);

    // The anchor value modifies how the rest of the transforms are applied to this layer
    // Ideally we'd just set UIElement::RenderTransformOrigin, but that doesn't apply to TranslateTransforms
    auto sizeAnchorTransform = ref new TranslateTransform(); // anchorpoint
    sizeAnchorTransform->X = -width * GetAnchorPointX(element);
    sizeAnchorTransform->Y = -height * GetAnchorPointY(element);

    // Set up the origin transform
    auto originTransform = ref new TranslateTransform(); // origin
    originTransform->X = -GetOriginX(element);
    originTransform->Y = -GetOriginY(element);

    // Nested transform group for rotation, scale and translation transforms
    auto contentTransform = ref new TransformGroup();
    contentTransform->Children->Append(ref new RotateTransform()); // transform.rotation
    contentTransform->Children->Append(ref new ScaleTransform()); // transform.scale
    contentTransform->Children->Append(ref new TranslateTransform()); // transform.translation

    // Positioning transform
    auto positionTransform = ref new TranslateTransform(); // position
    positionTransform->X = GetPositionX(element);
    positionTransform->Y = GetPositionY(element);

    auto layerTransforms = ref new TransformGroup();
    layerTransforms->Children->Append(sizeAnchorTransform);
    layerTransforms->Children->Append(originTransform);
    layerTransforms->Children->Append(contentTransform);
    layerTransforms->Children->Append(positionTransform);
    element->RenderTransform = layerTransforms;

    SetVisualWidth(element, width);
    SetVisualHeight(element, height);
}

// CALayer property managment support
void LayerProperties::SetValue(FrameworkElement^ element, String^ propertyName, Object^ value) {
    s_animatableProperties[propertyName]->SetValue(element, value);
}

Object^ LayerProperties::GetValue(FrameworkElement^ element, String^ propertyName) {
    return s_animatableProperties[propertyName]->GetValue(element);
}

void LayerProperties::AnimateValue(
    FrameworkElement^ target,
    Storyboard^ storyboard,
    DoubleAnimation^ timeline,
    String^ propertyName,
    Object^ fromValue,
    Object^ toValue) {
    s_animatableProperties[propertyName]->AnimateValue(target, storyboard, timeline, fromValue, toValue);
}

// CALayer content support
void LayerProperties::SetContent(FrameworkElement^ element, ImageSource^ source, float width, float height, float scale) {
    // Get content 
    auto contentImage = _GetContentImage(element, true /* createIfPossible */);
    if (!contentImage) {
        return;
    }

    // Apply content source
    contentImage->Source = source;

    // Store content size
    _SetContentSize(element, Size(width, height));

    // Apply scale
    // TODOTODOTODO:

    // Refresh any content center settings
    _ApplyContentCenter(element, _GetContentCenter(element));

    // Refresh any content gravity settings
    _ApplyContentGravity(element, GetContentGravity(element));
}

Image^ LayerProperties::_GetContentImage(FrameworkElement^ element, bool createIfPossible) {
    // First check if the element implements ILayer
    Image^ contentImage;
    ILayer^ layerElement = dynamic_cast<ILayer^>(element);
    if (layerElement) {
        // Accessing the LayerContent will create it on-demand, so only do so if necessary
        if (layerElement->HasLayerContent || createIfPossible) {
            contentImage = layerElement->LayerContent;
        }
    } else {
        // Not an ILayer, so default to grabbing the xamlElement's LayerContentProperty (if it exists)
        contentImage = dynamic_cast<Image^>(element->GetValue(Layer::LayerContentProperty));
    }

    if (!contentImage) {
        UNIMPLEMENTED_WITH_MSG(
            "Layer Content not supported on this Xaml element: [%ws].",
            element->GetType()->FullName->Data());
    }

    return contentImage;
}

void LayerProperties::SetContentCenter(FrameworkElement^ element, Windows::Foundation::Rect rect) {
    element->SetValue(s_contentCenterProperty, rect);
    _ApplyContentCenter(element, rect);
}

Rect LayerProperties::_GetContentCenter(FrameworkElement^ element) {
    return static_cast<Rect>(element->GetValue(s_contentCenterProperty));
}

void LayerProperties::_ApplyContentCenter(FrameworkElement^ element, Rect contentCenter) {
    // Nothing to do if we don't have any content
    Image^ image = _GetContentImage(element);
    if (!image || !image->Source ) {
        return;
    }

    ContentGravity gravity = GetContentGravity(element);
    Size contentSize = _GetContentSize(element);
    if (contentCenter.Equals(Rect(0, 0, 1.0, 1.0))) {
        image->NineGrid = Thickness(0, 0, 0, 0);
    } else {
        int left = static_cast<int>(contentCenter.X * contentSize.Width);
        int top = static_cast<int>(contentCenter.Y * contentSize.Height);
        int right = (static_cast<int>(contentSize.Width) - (left + (static_cast<int>(contentCenter.Width * contentSize.Width))));
        int bottom = (static_cast<int>(contentSize.Height) - (top + (static_cast<int>(contentCenter.Height * contentSize.Height))));

        // Remove edge cases that contentsCenter supports but NineGrid does not. 1/3 for top 1/3 for bottom 1/3 for
        // the center etc..
        left = std::max<int>(0, left);
        top = std::max<int>(0, top);
        right = std::max<int>(0, right);
        bottom = std::max<int>(0, bottom);

        // Cap the left/right to the maximum width
        int maxWidth = static_cast<int>(contentSize.Width / 3);
        left = std::min<int>(left, maxWidth);
        right = std::min<int>(right, maxWidth);

        // Cap the top/bottom to the maximum height
        int maxHeight = static_cast<int>(contentSize.Height / 3);
        top = std::min<int>(top, maxHeight);
        bottom = std::min<int>(bottom, maxHeight);

        // Set the image's NineGrid to accomodate
        image->NineGrid = Thickness(left, top, right, bottom);
    }
}

// CALayer border support
// TODO: Add border support

///////////////////////////////////////////////////////////////////////////////////
// TODO: EVERYTHING BELOW SHOULD/CAN PROB GO INTO A .CPP FILE AS A HELPER REF CLASS

void LayerProperties::_registerDependencyProperties() {
    if (!s_dependencyPropertiesRegistered) {

        // AnchorPoint always starts at Point(0.5, 0.5)
        s_anchorPointProperty = DependencyProperty::RegisterAttached(
            "AnchorPoint",
            Point::typeid,
            FrameworkElement::typeid,
            ref new PropertyMetadata(Point(0.5, 0.5), nullptr));

        s_originProperty = DependencyProperty::RegisterAttached(
            "Origin",
            Point::typeid,
            FrameworkElement::typeid,
            ref new PropertyMetadata(Point(0.0, 0.0), nullptr));

        s_positionProperty = DependencyProperty::RegisterAttached(
            "Position",
            Point::typeid,
            FrameworkElement::typeid,
            ref new PropertyMetadata(Point(0.0, 0.0), nullptr));

        s_visualWidthProperty = DependencyProperty::RegisterAttached(
            "VisualWidth",
            double::typeid,
            FrameworkElement::typeid,
            ref new PropertyMetadata(0.0,
            ref new PropertyChangedCallback(&LayerProperties::_sizeChangedCallback)));

        s_visualHeightProperty = DependencyProperty::RegisterAttached(
            "VisualHeight",
            double::typeid,
            FrameworkElement::typeid,
            ref new PropertyMetadata(0.0,
            ref new PropertyChangedCallback(&LayerProperties::_sizeChangedCallback)));

        s_contentGravityProperty = DependencyProperty::RegisterAttached(
            "ContentGravity",
            ContentGravity::typeid,
            FrameworkElement::typeid,
            ref new PropertyMetadata(ContentGravity::Resize, nullptr));

        s_contentCenterProperty = DependencyProperty::RegisterAttached(
            "ContentCenter",
            Windows::Foundation::Rect::typeid,
            FrameworkElement::typeid,
            ref new PropertyMetadata(Windows::Foundation::Rect(0, 0, 1.0, 1.0), nullptr));

        s_contentSizeProperty = DependencyProperty::RegisterAttached(
            "ContentSize",
            Windows::Foundation::Size::typeid,
            FrameworkElement::typeid,
            ref new PropertyMetadata(Windows::Foundation::Size(0, 0), nullptr));

        s_dependencyPropertiesRegistered = true;
    }
}

// AnchorPoint
DependencyProperty^ LayerProperties::AnchorPointProperty::get() {
    return s_anchorPointProperty;
}

float LayerProperties::GetAnchorPointX(FrameworkElement^ element) {
    return static_cast<Point>(element->GetValue(s_anchorPointProperty)).X;
}

void LayerProperties::SetAnchorPointX(FrameworkElement^ element, float value) {
    if (DEBUG_ANCHORPOINT) {
        TraceVerbose(TAG, L"SetAnchorPointX [%ws:%f]", element->GetType()->FullName->Data(), value);
    }

    auto point = static_cast<Point>(element->GetValue(s_anchorPointProperty));
    point.X = value;
    element->SetValue(s_anchorPointProperty, point);
}

float LayerProperties::GetAnchorPointY(FrameworkElement^ element) {
    return static_cast<Point>(element->GetValue(s_anchorPointProperty)).Y;
}

void LayerProperties::SetAnchorPointY(FrameworkElement^ element, float value) {
    if (DEBUG_ANCHORPOINT) {
        TraceVerbose(TAG, L"SetAnchorPointY [%ws:%f]", element->GetType()->FullName->Data(), value);
    }

    auto point = static_cast<Point>(element->GetValue(s_anchorPointProperty));
    point.Y = value;
    element->SetValue(s_anchorPointProperty, point);
}

TranslateTransform^ LayerProperties::GetAnchorTransform(FrameworkElement^ element) {
    return safe_cast<TranslateTransform^>(safe_cast<TransformGroup^>(element->RenderTransform)->Children->GetAt(0));
}

String^ LayerProperties::GetAnchorXTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[0].(TranslateTransform.X)";
    return path;
}

String^ LayerProperties::GetAnchorYTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[0].(TranslateTransform.Y)";
    return path;
}

// Origin
DependencyProperty^ LayerProperties::OriginProperty::get() {
    return s_originProperty;
}

float LayerProperties::GetOriginX(FrameworkElement^ element) {
    return static_cast<Point>(element->GetValue(s_originProperty)).X;
}

void LayerProperties::SetOriginX(FrameworkElement^ element, float value) {
    auto point = static_cast<Point>(element->GetValue(s_originProperty));
    point.X = value;
    element->SetValue(s_originProperty, point);
}

float LayerProperties::GetOriginY(FrameworkElement^ element) {
    return static_cast<Point>(element->GetValue(s_originProperty)).Y;
}

void LayerProperties::SetOriginY(FrameworkElement^ element, float value) {
    auto point = static_cast<Point>(element->GetValue(s_originProperty));
    point.Y = value;
    element->SetValue(s_originProperty, point);
}

TranslateTransform^ LayerProperties::GetOriginTransform(FrameworkElement^ element) {
    return safe_cast<TranslateTransform^>(safe_cast<TransformGroup^>(element->RenderTransform)->Children->GetAt(1));
}

String^ LayerProperties::GetOriginXTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[1].(TranslateTransform.X)";
    return path;
}

String^ LayerProperties::GetOriginYTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[1].(TranslateTransform.Y)";
    return path;
}

// Position
DependencyProperty^ LayerProperties::PositionProperty::get() {
    return s_positionProperty;
}

float LayerProperties::GetPositionX(FrameworkElement^ element) {
    return static_cast<Point>(element->GetValue(s_positionProperty)).X;
}

void LayerProperties::SetPositionX(FrameworkElement^ element, float value) {
    if (DEBUG_POSITION) {
        TraceVerbose(TAG, L"SetPositionX [%ws:%f]", element->GetType()->FullName->Data(), value);
    }

    auto point = static_cast<Point>(element->GetValue(s_positionProperty));
    point.X = value;
    element->SetValue(s_positionProperty, point);
}

float LayerProperties::GetPositionY(FrameworkElement^ element) {
    return static_cast<Point>(element->GetValue(s_positionProperty)).Y;
}

void LayerProperties::SetPositionY(FrameworkElement^ element, float value) {
    if (DEBUG_POSITION) {
        TraceVerbose(TAG, L"SetPositionY [%ws:%f]", element->GetType()->FullName->Data(), value);
    }

    auto point = static_cast<Point>(element->GetValue(s_positionProperty));
    point.Y = value;
    element->SetValue(s_positionProperty, point);
}

TranslateTransform^ LayerProperties::GetPositionTransform(FrameworkElement^ element) {
    return safe_cast<TranslateTransform^>(safe_cast<TransformGroup^>(element->RenderTransform)->Children->GetAt(3));
}

String^ LayerProperties::GetPositionXTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[3].(TranslateTransform.X)";
    return path;
}

String^ LayerProperties::GetPositionYTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[3].(TranslateTransform.Y)";
    return path;
}

// Rotation
RotateTransform^ LayerProperties::GetRotationTransform(FrameworkElement^ element) {
    return safe_cast<RotateTransform^>(
        safe_cast<TransformGroup^>(
            safe_cast<TransformGroup^>(element->RenderTransform)->Children->GetAt(2))->Children->GetAt(0));
}

String^ LayerProperties::GetRotationTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[2].(TransformGroup.Children)[0].(RotateTransform.Angle)";
    return path;
}

// Scale
ScaleTransform^ LayerProperties::GetScaleTransform(FrameworkElement^ element) {
    return safe_cast<ScaleTransform^>(
        safe_cast<TransformGroup^>(
            safe_cast<TransformGroup^>(element->RenderTransform)->Children->GetAt(2))->Children->GetAt(1));
}

String^ LayerProperties::GetScaleXTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[2].(TransformGroup.Children)[1].(ScaleTransform.ScaleX)";
    return path;
}

String^ LayerProperties::GetScaleYTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[2].(TransformGroup.Children)[1].(ScaleTransform.ScaleY)";
    return path;
}

// Translation
TranslateTransform^ LayerProperties::GetTranslationTransform(FrameworkElement^ element) {
    return safe_cast<TranslateTransform^>(
        safe_cast<TransformGroup^>(
            safe_cast<TransformGroup^>(element->RenderTransform)->Children->GetAt(2))->Children->GetAt(2));
}

String^ LayerProperties::GetTranslationXTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[2].(TransformGroup.Children)[2].(TranslateTransform.X)";
    return path;
}

String^ LayerProperties::GetTranslationYTransformPath() {
    static String^ path = L"(UIElement.RenderTransform).(TransformGroup.Children)[2].(TransformGroup.Children)[2].(TranslateTransform.Y)";
    return path;
}

// VisualWidth
DependencyProperty^ LayerProperties::VisualWidthProperty::get() {
    return s_visualWidthProperty;
}

double LayerProperties::GetVisualWidth(FrameworkElement^ element) {
    return static_cast<double>(element->GetValue(s_visualWidthProperty));
}

void LayerProperties::SetVisualWidth(FrameworkElement^ element, double value) {
    element->SetValue(s_visualWidthProperty, value);
}

// VisualHeight
DependencyProperty^ LayerProperties::VisualHeightProperty::get() {
    return s_visualHeightProperty;
}

double LayerProperties::GetVisualHeight(FrameworkElement^ element) {
    return static_cast<double>(element->GetValue(s_visualHeightProperty));
}

void LayerProperties::SetVisualHeight(FrameworkElement^ element, double value) {
    element->SetValue(s_visualHeightProperty, value);
}

void LayerProperties::_sizeChangedCallback(DependencyObject^ sender, DependencyPropertyChangedEventArgs^ args) {
    auto element = safe_cast<FrameworkElement^>(sender);

    //////////////////////////////////////////////////////////////////////////////////////////////
    // TODO: Revisit whether or not we actually need this - it can't be very performant...?
    // element->InvalidateArrange();
    //////////////////////////////////////////////////////////////////////////////////////////////

    // Refresh any content gravity settings
    _ApplyContentGravity(element, GetContentGravity(element));
}

// ContentGravity
DependencyProperty^ LayerProperties::ContentGravityProperty::get() {
    return s_contentGravityProperty;
}

ContentGravity LayerProperties::GetContentGravity(FrameworkElement^ element) {
    return static_cast<ContentGravity>(element->GetValue(s_contentGravityProperty));
}

void LayerProperties::SetContentGravity(FrameworkElement^ element, ContentGravity value) {
    element->SetValue(s_contentGravityProperty, value);
    _ApplyContentGravity(element, value);
}

void LayerProperties::_ApplyContentGravity(FrameworkElement^ element, ContentGravity gravity) {
    // Calculate aspect ratio
    Size contentSize = _GetContentSize(element);
    double widthAspect = element->Width / contentSize.Width;
    double heightAspect = element->Height / contentSize.Height;
    double minAspect = std::min<double>(widthAspect, heightAspect);
    double maxAspect = std::max<double>(widthAspect, heightAspect);

    // Apply gravity mapping
    ////////////////////////////////////////////////////
    float scale = 1.0; // TODO: GET SCALE SETTING
    ////////////////////////////////////////////////////
    HorizontalAlignment horizontalAlignment;
    VerticalAlignment verticalAlignment;
    double contentWidth = 0.0;
    double contentHeight = 0.0;
    switch (gravity) {
    case ContentGravity::Center:
        horizontalAlignment = HorizontalAlignment::Center;
        verticalAlignment = VerticalAlignment::Center;
        contentWidth = contentSize.Width * scale;
        contentHeight = contentSize.Height * scale;
        break;

    case ContentGravity::Top:
        horizontalAlignment = HorizontalAlignment::Center;
        verticalAlignment = VerticalAlignment::Top;
        contentWidth = contentSize.Width * scale;
        contentHeight = contentSize.Height * scale;
        break;

    case ContentGravity::Bottom:
        horizontalAlignment = HorizontalAlignment::Center;
        verticalAlignment = VerticalAlignment::Bottom;
        contentWidth = contentSize.Width * scale;
        contentHeight = contentSize.Height * scale;
        break;

    case ContentGravity::Left:
        horizontalAlignment = HorizontalAlignment::Left;
        verticalAlignment = VerticalAlignment::Center;
        contentWidth = contentSize.Width * scale;
        contentHeight = contentSize.Height * scale;
        break;

    case ContentGravity::Right:
        horizontalAlignment = HorizontalAlignment::Right;
        verticalAlignment = VerticalAlignment::Center;
        contentWidth = contentSize.Width * scale;
        contentHeight = contentSize.Height * scale;
        break;

    case ContentGravity::TopLeft:
        horizontalAlignment = HorizontalAlignment::Left;
        verticalAlignment = VerticalAlignment::Top;
        contentWidth = contentSize.Width * scale;
        contentHeight = contentSize.Height * scale;
        break;

    case ContentGravity::TopRight:
        horizontalAlignment = HorizontalAlignment::Right;
        verticalAlignment = VerticalAlignment::Top;
        contentWidth = contentSize.Width * scale;
        contentHeight = contentSize.Height * scale;
        break;

    case ContentGravity::BottomLeft:
        horizontalAlignment = HorizontalAlignment::Left;
        verticalAlignment = VerticalAlignment::Bottom;
        contentWidth = contentSize.Width * scale;
        contentHeight = contentSize.Height * scale;
        break;

    case ContentGravity::BottomRight:
        horizontalAlignment = HorizontalAlignment::Right;
        verticalAlignment = VerticalAlignment::Bottom;
        contentWidth = contentSize.Width * scale;
        contentHeight = contentSize.Height * scale;
        break;

    case ContentGravity::Resize:
        // UIViewContentModeScaleToFill;
        horizontalAlignment = HorizontalAlignment::Left;
        verticalAlignment = VerticalAlignment::Top;
        contentWidth = element->Width * scale;
        contentHeight = element->Height * scale;
        break;

    // UIViewContentModeScaleAspectFit
    case ContentGravity::ResizeAspect:
        // Center the image
        horizontalAlignment = HorizontalAlignment::Center;
        verticalAlignment = VerticalAlignment::Center;

        // Scale the image with the smaller aspect.
        contentWidth = contentSize.Width * static_cast<float>(minAspect) * scale;
        contentHeight = contentSize.Height * static_cast<float>(minAspect) * scale;
        break;

    // UIViewContentModeScaleAspectFill
    case ContentGravity::ResizeAspectFill:
        // Center the image
        horizontalAlignment = HorizontalAlignment::Center;
        verticalAlignment = VerticalAlignment::Center;

        // Scale the image with the larger aspect.
        contentWidth = contentSize.Width * static_cast<float>(maxAspect);
        contentHeight = contentSize.Height * static_cast<float>(maxAspect);
        break;
    }

    // Do we have content?
    Image^ image = _GetContentImage(element);
    if (image) {
        // Using Fill since we calculate the aspect/size ourselves
        image->Stretch = Stretch::Fill;

        // Only resize the content image if we have one
        image->Width = contentWidth;
        image->Height = contentHeight;
    }

    // If we don't have content, apply the gravity alignment directly to the target FrameworkElement,
    // otherwise apply it to the content image
    FrameworkElement^ gravityElement = image ? image : element;
    gravityElement->HorizontalAlignment = horizontalAlignment;
    gravityElement->VerticalAlignment = verticalAlignment;
}

// ContentSize
DependencyProperty^ LayerProperties::ContentSizeProperty::get() {
    return s_contentSizeProperty;
}

Size LayerProperties::_GetContentSize(FrameworkElement^ element) {
    return static_cast<Size>(element->GetValue(s_contentSizeProperty));
}

void LayerProperties::_SetContentSize(Windows::UI::Xaml::FrameworkElement^ element, Size value) {
    element->SetValue(s_contentSizeProperty, value);
}

void LayerProperties::AddAnimation(Platform::String^ propertyName,
                                   FrameworkElement^ target,
                                   Media::Animation::Storyboard^ storyboard,
                                   Media::Animation::DoubleAnimation^ copyProperties,
                                   Platform::Object^ fromValue,
                                   Platform::Object^ toValue,
                                   bool dependent) {
    DoubleAnimation^ posxAnim = ref new DoubleAnimation();
    if (toValue) {
        posxAnim->To = static_cast<double>(toValue);
    }

    if (fromValue) {
        posxAnim->From = static_cast<double>(fromValue);
    }

    posxAnim->Duration = copyProperties->Duration;
    posxAnim->RepeatBehavior = copyProperties->RepeatBehavior;
    posxAnim->AutoReverse = copyProperties->AutoReverse;
    posxAnim->EasingFunction = copyProperties->EasingFunction;
    posxAnim->EnableDependentAnimation = dependent;
    posxAnim->FillBehavior = copyProperties->FillBehavior;
    posxAnim->BeginTime = copyProperties->BeginTime;
    storyboard->Children->Append(posxAnim);

    Storyboard::SetTarget(posxAnim, target);
    Storyboard::SetTargetProperty(posxAnim, propertyName);
}

} /* namespace CoreAnimation */

// clang-format on