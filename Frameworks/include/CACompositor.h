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

#if defined(__cplusplus) && defined(__OBJC__) && !defined(__CA_COMPOSITOR_H)
#define __CA_COMPOSITOR_H

#include <memory>
#include "winobjc\winobjc.h"

class DisplayNode;
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

    // DisplayNode creation
    virtual std::shared_ptr<DisplayNode> CreateDisplayNode(const Microsoft::WRL::ComPtr<IInspectable>& xamlElement) = 0;

    virtual Microsoft::WRL::ComPtr<IInspectable> GetXamlLayoutElement(const std::shared_ptr<DisplayNode>&) = 0;

    // DisplayNode Sublayer management
    virtual void addNode(const std::shared_ptr<DisplayTransaction>& transaction,
                         const std::shared_ptr<DisplayNode>& node,
                         const std::shared_ptr<DisplayNode>& superNode,
                         const std::shared_ptr<DisplayNode>& beforeNode,
                         const std::shared_ptr<DisplayNode>& afterNode) = 0;
    virtual void moveNode(const std::shared_ptr<DisplayTransaction>& transaction,
                          const std::shared_ptr<DisplayNode>& node,
                          const std::shared_ptr<DisplayNode>& beforeNode,
                          const std::shared_ptr<DisplayNode>& afterNode) = 0;
    virtual void removeNode(const std::shared_ptr<DisplayTransaction>& transaction, const std::shared_ptr<DisplayNode>& node) = 0;

    // DisplayNode layer display properties
    virtual void setDisplayProperty(const std::shared_ptr<DisplayTransaction>& transaction,
                                    const std::shared_ptr<DisplayNode>& node,
                                    const char* propertyName,
                                    NSObject* newValue) = 0;
    virtual void setNodeTexture(const std::shared_ptr<DisplayTransaction>& transaction,
                                const std::shared_ptr<DisplayNode>& node,
                                DisplayTexture* newTexture,
                                CGSize contentsSize,
                                float contentsScale) = 0;
    virtual NSObject* getDisplayProperty(const std::shared_ptr<DisplayNode>& node, const char* propertyName = NULL) = 0;
    virtual void setNodeTopMost(const std::shared_ptr<DisplayNode>& node, bool topMost) = 0;
    virtual void SetShouldRasterize(const std::shared_ptr<DisplayNode>& node, bool rasterize) = 0;

    // DisplayTextures
    virtual DisplayTexture* GetDisplayTextureForCGImage(CGImageRef img, bool create) = 0;
    virtual Microsoft::WRL::ComPtr<IInspectable> GetBitmapForCGImage(CGImageRef img) = 0;

    ////////////////////////////////////////////////////////////////////////
    // TODO: Move to some bitmap/texture helper API?
    virtual DisplayTexture* CreateWritableBitmapTexture32(int width, int height) = 0;
    virtual void* LockWritableBitmapTexture(DisplayTexture* tex, int* stride) = 0;
    virtual void UnlockWritableBitmapTexture(DisplayTexture* tex) = 0;

    // Animations
    virtual void addAnimation(const std::shared_ptr<DisplayTransaction>& transaction, id layer, id animation, id forKey) = 0;
    virtual void removeAnimation(const std::shared_ptr<DisplayTransaction>& transaction, const std::shared_ptr<DisplayAnimation>& animation) = 0;
    virtual std::shared_ptr<DisplayAnimation> GetBasicDisplayAnimation(id caanim,
                                                       NSString* propertyName,
                                                       NSObject* fromValue,
                                                       NSObject* toValue,
                                                       NSObject* byValue,
                                                       CAMediaTimingProperties* timingProperties) = 0;
    virtual std::shared_ptr<DisplayAnimation> GetMoveDisplayAnimation(id caanim,
                                                      const std::shared_ptr<DisplayNode>& animNode,
                                                      NSString* type,
                                                      NSString* subtype,
                                                      CAMediaTimingProperties* timingProperties) = 0;

    ////////////////////////////////////////////////////////////////////////
    // TODO: Switch to shared_ptr for alla these
    virtual void RetainDisplayTexture(DisplayTexture* tex) = 0;
    virtual void ReleaseDisplayTexture(DisplayTexture* tex) = 0;
    ////////////////////////////////////////////////////////////////////////

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
