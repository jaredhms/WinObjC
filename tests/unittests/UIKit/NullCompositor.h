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
    bool IsRunningAsFramework() override {
        return false;
    }
    float GetScreenScale() override {
        return 1.0f;
    }

    std::shared_ptr<ILayerTransaction> CreateLayerTransaction() override {
        return nullptr;
    }
    void QueueLayerTransaction(const std::shared_ptr<ILayerTransaction>& transaction,
        const std::shared_ptr<ILayerTransaction>& onTransaction) override {
    }
    void ProcessLayerTransactions() override {
    }

    std::shared_ptr<ILayerProxy> CreateLayerProxy(const Microsoft::WRL::ComPtr<IInspectable>& xamlElement) override {
        return nullptr;
    }

    std::shared_ptr<ILayerAnimation> CreateBasicAnimation(CAAnimation* animation,
                                                                 NSString* propertyName,
                                                                 NSObject* fromValue,
                                                                 NSObject* toValue,
                                                                 NSObject* byValue,
                                                                 CAMediaTimingProperties* timingProperties) override {
        return nullptr;
    }
    std::shared_ptr<ILayerAnimation> CreateTransitionAnimation(CAAnimation* animation,
                                                                      NSString* type,
                                                                      NSString* subtype) override {
        return nullptr;
    }

    std::shared_ptr<DisplayTexture> GetDisplayTextureForCGImage(CGImageRef img, bool create) override {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<IInspectable> GetBitmapForCGImage(CGImageRef img) override {
        return nullptr;
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
