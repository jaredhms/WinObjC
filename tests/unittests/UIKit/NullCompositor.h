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

#import "CACompositor.h"

class NullCompositor : public CACompositorInterface {
public:
    virtual bool IsRunningAsFramework() override {
        return false;
    }

    std::shared_ptr<DisplayTransaction> CreateDisplayTransaction() override {
        return nullptr;
    }
    void QueueDisplayTransaction(const std::shared_ptr<DisplayTransaction>& transaction,
        const std::shared_ptr<DisplayTransaction>& onTransaction) override {
    }
    void ProcessTransactions() override {
    }

    std::shared_ptr<ILayerProxy> CreateLayerProxy(const Microsoft::WRL::ComPtr<IInspectable>& xamlElement) override {
        return nullptr;
    }

    void addNode(const std::shared_ptr<DisplayTransaction>& transaction,
                 const std::shared_ptr<ILayerProxy>& node,
                 const std::shared_ptr<ILayerProxy>& superNode,
                 const std::shared_ptr<ILayerProxy>& beforeNode,
                 const std::shared_ptr<ILayerProxy>& afterNode) override {
    }
    void moveNode(const std::shared_ptr<DisplayTransaction>& transaction,
                  const std::shared_ptr<ILayerProxy>& node,
                  const std::shared_ptr<ILayerProxy>& beforeNode,
                  const std::shared_ptr<ILayerProxy>& afterNode) override {
    }
    void removeNode(const std::shared_ptr<DisplayTransaction>& transaction, const std::shared_ptr<ILayerProxy>& node) override {
    }

    void setDisplayProperty(const std::shared_ptr<DisplayTransaction>& transaction,
                            const std::shared_ptr<ILayerProxy>& node,
                            const char* propertyName,
                            NSObject* newValue) override {
    }
    void setNodeTexture(const std::shared_ptr<DisplayTransaction>& transaction,
                        const std::shared_ptr<ILayerProxy>& node,
                        const std::shared_ptr<DisplayTexture>& newTexture,
                        CGSize contentsSize,
                        float contentsScale) override {
    }
    void setNodeTopMost(const std::shared_ptr<ILayerProxy>& node, bool topMost) override {
    }

    std::shared_ptr<DisplayTexture> GetDisplayTextureForCGImage(CGImageRef img, bool create) override {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<IInspectable> GetBitmapForCGImage(CGImageRef img) override {
        return nullptr;
    }

    void addAnimation(const std::shared_ptr<DisplayTransaction>& transaction, id layer, id animation, id forKey) override {
    }
    void removeAnimation(const std::shared_ptr<DisplayTransaction>& transaction,
        const std::shared_ptr<DisplayAnimation>& animation) override {
    }
    std::shared_ptr<DisplayAnimation> GetBasicDisplayAnimation(id caanim,
                                                               NSString* propertyName,
                                                               NSObject* fromValue,
                                                               NSObject* toValue,
                                                               NSObject* byValue,
                                                               CAMediaTimingProperties* timingProperties) override {
        return nullptr;
    }
    std::shared_ptr<DisplayAnimation> GetMoveDisplayAnimation(id caanim,
                                                              const std::shared_ptr<ILayerProxy>& animNode,
                                                              NSString* type,
                                                              NSString* subtype,
                                                              CAMediaTimingProperties* timingProperties) override {
        return nullptr;
    }

    bool isTablet() override {
        return false;
    }
    float screenWidth() override {
        return 100.0f;
    }
    float screenHeight() override {
        return 100.0f;
    }
    float screenScale() override {
        return 1.0f;
    }
    int deviceWidth() override {
        return 100;
    }
    int deviceHeight() override {
        return 100;
    }
    float screenXDpi() override {
        return 100;
    }
    float screenYDpi() override {
        return 100;
    }

    void setScreenSize(float width, float height, float scale, float rotationClockwise) override {
    }
    void setDeviceSize(int width, int height) override {
    }
    void setScreenDpi(int xDpi, int yDpi) override {
    }
    void setTablet(bool isTablet) override {
    }

    std::shared_ptr<DisplayTexture> CreateWritableBitmapTexture32(int width, int height) override {
        return nullptr;
    }
    void* LockWritableBitmapTexture(const std::shared_ptr<DisplayTexture>& tex, int* stride) override {
        return nullptr;
    }
    void UnlockWritableBitmapTexture(const std::shared_ptr<DisplayTexture>& tex) override {
    }

    void EnableDisplaySyncNotification() override {
    }
    void DisableDisplaySyncNotification() override {
    }
};
