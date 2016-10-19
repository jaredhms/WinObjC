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
#pragma once

#if defined(__OBJC__)

#include <COMIncludes.h>
#import <WRLHelpers.h>
#import <ErrorHandling.h>
#import <RawBuffer.h>
#import <wrl/client.h>
#import <wrl/implements.h>
#import <wrl/async.h>
#import <wrl/wrappers/corewrappers.h>
#import <windows.foundation.h>
#include <COMIncludes_end.h>

#else

#include <wrl/client.h>

#endif

// Proxy between CALayer and its Xaml representation
struct ILayerProxy {
public:
    virtual ~ILayerProxy() {}

    virtual Microsoft::WRL::ComPtr<IInspectable> GetXamlElement() = 0;
    virtual void* GetPropertyValue(const char* propertyName) = 0;
    virtual void SetShouldRasterize(bool shouldRasterize) = 0;
};

#if defined(__cplusplus) && defined(__OBJC__)

#include <memory>
#include "winobjc\winobjc.h"

class DisplayAnimation;
class DisplayTexture;
class DisplayTransaction;
struct CAMediaTimingProperties;

#define CACompositorRotationNone 0.0f
#define CACompositorRotation90Clockwise 90.0f
#define CACompositorRotation180 180.0f
#define CACompositorRotation90CounterClockwise 270.0f

class CACompositorInterface {
public:
    // Compositor APIs
    virtual bool IsRunningAsFramework() = 0;

    // CATransaction support
    virtual std::shared_ptr<DisplayTransaction> CreateDisplayTransaction() = 0;
    virtual void QueueDisplayTransaction(const std::shared_ptr<DisplayTransaction>& transaction,
                                         const std::shared_ptr<DisplayTransaction>& onTransaction) = 0;
    virtual void ProcessTransactions() = 0;

    // LayerProxy creation
    virtual std::shared_ptr<ILayerProxy> CreateLayerProxy(const Microsoft::WRL::ComPtr<IInspectable>& xamlElement) = 0;

    // Sublayer management
    virtual void addNode(const std::shared_ptr<DisplayTransaction>& transaction,
                         const std::shared_ptr<ILayerProxy>& node,
                         const std::shared_ptr<ILayerProxy>& superNode,
                         const std::shared_ptr<ILayerProxy>& beforeNode,
                         const std::shared_ptr<ILayerProxy>& afterNode) = 0;
    virtual void moveNode(const std::shared_ptr<DisplayTransaction>& transaction,
                          const std::shared_ptr<ILayerProxy>& node,
                          const std::shared_ptr<ILayerProxy>& beforeNode,
                          const std::shared_ptr<ILayerProxy>& afterNode) = 0;
    virtual void removeNode(const std::shared_ptr<DisplayTransaction>& transaction, const std::shared_ptr<ILayerProxy>& node) = 0;

    // Layer properties
    virtual void setDisplayProperty(const std::shared_ptr<DisplayTransaction>& transaction,
                                    const std::shared_ptr<ILayerProxy>& node,
                                    const char* propertyName,
                                    NSObject* newValue) = 0;
    virtual void setNodeTexture(const std::shared_ptr<DisplayTransaction>& transaction,
                                const std::shared_ptr<ILayerProxy>& node,
                                const std::shared_ptr<DisplayTexture>& newTexture,
                                CGSize contentsSize,
                                float contentsScale) = 0;
    virtual void setNodeTopMost(const std::shared_ptr<ILayerProxy>& node, bool topMost) = 0;

    // DisplayTextures
    virtual std::shared_ptr<DisplayTexture> GetDisplayTextureForCGImage(CGImageRef img, bool create) = 0;
    virtual Microsoft::WRL::ComPtr<IInspectable> GetBitmapForCGImage(CGImageRef img) = 0;

    ////////////////////////////////////////////////////////////////////////
    // TODO: Move to some bitmap/texture helper API?
    virtual std::shared_ptr<DisplayTexture> CreateWritableBitmapTexture32(int width, int height) = 0;
    virtual void* LockWritableBitmapTexture(const std::shared_ptr<DisplayTexture>& texture, int* stride) = 0;
    virtual void UnlockWritableBitmapTexture(const std::shared_ptr<DisplayTexture>& texture) = 0;

    // Animations
    virtual void addAnimation(const std::shared_ptr<DisplayTransaction>& transaction, id layer, id animation, id forKey) = 0;
    virtual void removeAnimation(const std::shared_ptr<DisplayTransaction>& transaction,
                                 const std::shared_ptr<DisplayAnimation>& animation) = 0;
    virtual std::shared_ptr<DisplayAnimation> GetBasicDisplayAnimation(id caanim,
                                                                       NSString* propertyName,
                                                                       NSObject* fromValue,
                                                                       NSObject* toValue,
                                                                       NSObject* byValue,
                                                                       CAMediaTimingProperties* timingProperties) = 0;
    virtual std::shared_ptr<DisplayAnimation> GetMoveDisplayAnimation(id caanim,
                                                                      const std::shared_ptr<ILayerProxy>& animNode,
                                                                      NSString* type,
                                                                      NSString* subtype,
                                                                      CAMediaTimingProperties* timingProperties) = 0;

    ////////////////////////////////////////////////////////////////////////
    // TODO: Move to some screen/device API
    virtual bool isTablet() = 0;
    virtual float screenWidth() = 0;
    virtual float screenHeight() = 0;
    virtual float screenScale() = 0;
    virtual int deviceWidth() = 0;
    virtual int deviceHeight() = 0;
    virtual float screenXDpi() = 0;
    virtual float screenYDpi() = 0;

    virtual void setScreenSize(float width, float height, float scale, float rotationClockwise) = 0;
    virtual void setDeviceSize(int width, int height) = 0;
    virtual void setScreenDpi(int xDpi, int yDpi) = 0;
    virtual void setTablet(bool isTablet) = 0;
    ////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////
    // TODO: Move to some displaylink helper API
    virtual void EnableDisplaySyncNotification() = 0;
    virtual void DisableDisplaySyncNotification() = 0;
    ////////////////////////////////////////////////////////////////////////
};

extern CACompositorInterface* _globalCompositor;

#ifndef CA_EXPORT
#define CA_EXPORT
#endif

CA_EXPORT CACompositorInterface* GetCACompositor();
CA_EXPORT void SetCACompositor(CACompositorInterface* compositorInterface);

CA_EXPORT bool CASignalDisplayLink();
#endif
