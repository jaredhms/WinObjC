//******************************************************************************
//
// Copyright (c) 2016 Intel Corporation. All rights reserved.
// Copyright (c) 2016 Microsoft Corporation. All rights reserved.
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

#import "Starboard.h"
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
#import "CACompositor.h"
#import "QuartzCore\CALayer.h"
#import "CGContextInternal.h"
#import "UIInterface.h"
#import "UIColorInternal.h"
#import "QuartzCore\CATransform3D.h"
#import "Quaternion.h"

#import <deque>
#import <map>
#import <mutex>
#import <algorithm>
#import "CompositorInterface.h"
#import "CAAnimationInternal.h"
#import "CALayerInternal.h"
#import "UWP/interopBase.h"

#import <LoggingNative.h>
#import "StringHelpers.h"
#import <MainDispatcher.h>
#import "CoreGraphics/CGImage.h"

#import <UWP/WindowsUIViewManagement.h>
#import <UWP/WindowsDevicesInput.h>
#import "UIColorInternal.h"

#import "DisplayProperties.h"
#import "LayerAnimation.h"
#import "LayerProxy.h"
#import "LayerTransaction.h"

#import "UIApplicationInternal.h"

using namespace Microsoft::WRL;

static const wchar_t* TAG = L"CompositorInterface";

@class RTObject;

CompositionMode g_compositionMode = CompositionModeDefault;

/////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: MOVE TO BITMAP MANAGMENT API
ComPtr<IInspectable> CreateBitmapFromBits(void* ptr, int width, int height, int stride);
ComPtr<IInspectable> CreateBitmapFromImageData(const void* ptr, int len);
ComPtr<IInspectable> CreateWritableBitmap(int width, int height);
ComPtr<IInspectable> LockWritableBitmap(const ComPtr<IInspectable>& bitmap, void** ptr, int* stride);
/////////////////////////////////////////////////////////////////////////////////////////////////

void SetRootGrid(winobjc::Id& root);

/////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: MOVE TO DisplayLink API
void EnableRenderingListener(void (*callback)());
void DisableRenderingListener();

void OnRenderedFrame() {
    CASignalDisplayLink();
}
/////////////////////////////////////////////////////////////////////////////////////////////////

std::mutex _displayTextureCacheLock;
std::map<CGImageRef, std::shared_ptr<DisplayTexture>> _displayTextureCache;

std::shared_ptr<DisplayTexture> CachedDisplayTextureForImage(CGImageRef img) {
    std::shared_ptr<DisplayTexture> ret;
    _displayTextureCacheLock.lock();
    auto cachedEntry = _displayTextureCache.find(img);
    if (cachedEntry != _displayTextureCache.end()) {
        ret = cachedEntry->second;
    }
    _displayTextureCacheLock.unlock();

    return ret;
}

void SetCachedDisplayTextureForImage(CGImageRef img, const std::shared_ptr<DisplayTexture>& texture) {
    _displayTextureCacheLock.lock();
    if (!texture) {
        // Clear the cache if the texture is nullptr
        _displayTextureCache.erase(img);
    } else {
        // Cache the texture
        _displayTextureCache[img] = texture;
    }
    _displayTextureCacheLock.unlock();
}

void UIReleaseDisplayTextureForCGImage(CGImageRef img) {
    SetCachedDisplayTextureForImage(img, nullptr);
}

class DisplayTextureContent : public DisplayTexture {
public:
    DisplayTextureContent(CGImageRef img) {
        if (img->_imgType == CGImageTypePNG || img->_imgType == CGImageTypeJPEG) {
            const void* data = NULL;
            bool freeData = false;
            int len = 0;

            switch (img->_imgType) {
                case CGImageTypePNG: {
                    CGPNGImageBacking* pngImg = (CGPNGImageBacking*)img->Backing();

                    if (pngImg->_fileName) {
                        EbrFile* fpIn;
                        fpIn = EbrFopen(pngImg->_fileName, "rb");
                        if (!fpIn) {
                            FAIL_FAST();
                        }
                        EbrFseek(fpIn, 0, SEEK_END);
                        int fileLen = EbrFtell(fpIn);
                        EbrFseek(fpIn, 0, SEEK_SET);
                        void* pngData = (void*)IwMalloc(fileLen);
                        len = EbrFread(pngData, 1, fileLen, fpIn);
                        EbrFclose(fpIn);

                        data = pngData;
                        freeData = true;
                    } else {
                        data = [(NSData*)pngImg->_data bytes];
                        len = [(NSData*)pngImg->_data length];
                    }
                } break;

                case CGImageTypeJPEG: {
                    CGJPEGImageBacking* jpgImg = (CGJPEGImageBacking*)img->Backing();

                    if (jpgImg->_fileName) {
                        EbrFile* fpIn;
                        fpIn = EbrFopen(jpgImg->_fileName, "rb");
                        if (!fpIn) {
                            FAIL_FAST();
                        }
                        EbrFseek(fpIn, 0, SEEK_END);
                        int fileLen = EbrFtell(fpIn);
                        EbrFseek(fpIn, 0, SEEK_SET);
                        void* imgData = (void*)IwMalloc(fileLen);
                        len = EbrFread(imgData, 1, fileLen, fpIn);
                        EbrFclose(fpIn);

                        data = imgData;
                        freeData = true;
                    } else {
                        data = [(NSData*)jpgImg->_data bytes];
                        len = [(NSData*)jpgImg->_data length];
                    }
                } break;
                default:
                    TraceError(TAG, L"Warning: unrecognized image format sent to DisplayTextureContent!");
                    break;
            }
            _xamlImage = CreateBitmapFromImageData(data, len);
            if (freeData) {
                IwFree((void*)data);
            }
            return;
        }

        int texWidth = img->Backing()->Width();
        int texHeight = img->Backing()->Height();
        int imageWidth = texWidth;
        int imageHeight = texHeight;
        int imageX = 0;
        int imageY = 0;
        CGImageRef pNewImage = 0;
        CGImageRef pTexImage = img;

        bool matched = false;
        if (img->Backing()->SurfaceFormat() == _ColorARGB) {
            matched = true;
        }

        // Unrecognized, make 8888 ARGB:
        if (!matched) {
            CGContextRef ctx = _CGBitmapContextCreateWithFormat(texWidth, texHeight, _ColorARGB);

            pNewImage = CGBitmapContextGetImage(ctx);
            CGImageRetain(pNewImage);

            pTexImage = pNewImage;

            CGRect rect, destRect;

            int w = fmin(imageWidth, texWidth), h = fmin(imageHeight, texHeight);

            rect.origin.x = float(imageX);
            rect.origin.y = float(imageY);
            rect.size.width = float(w);
            rect.size.height = float(h);

            destRect.origin.x = 0;
            destRect.origin.y = float(texHeight - imageHeight);
            destRect.size.width = float(w);
            destRect.size.height = float(h);

            CGContextDrawImageRect(ctx, img, rect, destRect);
            CGContextRelease(ctx);
        }

        void* data = (void*)pTexImage->Backing()->LockImageData();
        int bytesPerRow = pTexImage->Backing()->BytesPerRow();

        int width = pTexImage->Backing()->Width();
        int height = pTexImage->Backing()->Height();

        _xamlImage = CreateBitmapFromBits(data, width, height, bytesPerRow);

        pTexImage->Backing()->ReleaseImageData();
        img->Backing()->DiscardIfPossible();

        CGImageRelease(pNewImage);
    }

    DisplayTextureContent(int width, int height) {
        _xamlImage = CreateWritableBitmap(width, height);
    }

    void* LockWritableBitmap(int* stride) {
        void* ret = nullptr;
        _lockPtr = ::LockWritableBitmap(_xamlImage, &ret, stride);
        return ret;
    }

    void UnlockWritableBitmap() {
        // Release our _lockPtr to unlock the bitmap
        _lockPtr.Reset();
    }

    const ComPtr<IInspectable>& GetContent() const {
        return _xamlImage;
    }

private:
    Microsoft::WRL::ComPtr<IInspectable> _lockPtr;
};

std::deque<std::shared_ptr<ICompositorTransaction>> s_queuedTransactions;

class CAXamlCompositor : public CACompositorInterface {
public:
    // Compositor APIs
    virtual bool IsRunningAsFramework() override {
        return g_compositionMode == CompositionModeLibrary;
    }

    float GetScreenScale() override {
        return DisplayProperties::ScreenScale();
    }

    // CATransaction support
    std::shared_ptr<ILayerTransaction> CreateLayerTransaction() override {
        return std::make_shared<LayerTransaction>();
    }

    void QueueLayerTransaction(const std::shared_ptr<ILayerTransaction>& transaction,
                               const std::shared_ptr<ILayerTransaction>& onTransaction) override {
        if (onTransaction) {
            std::dynamic_pointer_cast<LayerTransaction>(onTransaction)->QueueTransaction(transaction);
        } else {
            s_queuedTransactions.push_back(std::dynamic_pointer_cast<ICompositorTransaction>(transaction));
        }
    }

    void ProcessLayerTransactions() override {
        for (auto& transaction : s_queuedTransactions) {
            transaction->Process();
        }
        s_queuedTransactions.clear();
    }

    // CALayer support
    virtual std::shared_ptr<ILayerProxy> CreateLayerProxy(const ComPtr<IInspectable>& xamlElement) override {
        return std::make_shared<LayerProxy>(xamlElement.Get());
    }

    // CAAnimation support
    virtual std::shared_ptr<ILayerAnimation> CreateBasicAnimation(CAAnimation* animation,
                                                                       NSString* propertyName,
                                                                       NSObject* fromValue,
                                                                       NSObject* toValue,
                                                                       NSObject* byValue,
                                                                       CAMediaTimingProperties* timingProperties) override {
        return LayerAnimation::CreateBasicAnimation(animation, propertyName, fromValue, toValue, byValue, timingProperties);
    }

    virtual std::shared_ptr<ILayerAnimation> CreateTransitionAnimation(CAAnimation* animation,
                                                                      NSString* type,
                                                                      NSString* subType) override {
        return LayerAnimation::CreateTransitionAnimation(animation, type, subType);
    }

    virtual std::shared_ptr<DisplayTexture> GetDisplayTextureForCGImage(CGImageRef img, bool create) override {
        CGImageRetain(img);
        auto cleanup = wil::ScopeExit([img] {
            CGImageRelease(img);
        });

        // If the image has a backing texture, use it
        std::shared_ptr<DisplayTexture> texture = img->Backing()->GetDisplayTexture();
        if (texture) {
            return texture;
        }

        // If we have a cached texture for this image, use it
        texture = CachedDisplayTextureForImage(img);
        if (texture) {
            return texture;
        }

        // Else, create and cache a new texture
        texture = std::make_shared<DisplayTextureContent>(img);
        SetCachedDisplayTextureForImage(img, texture);
        return texture;
    }

    virtual ComPtr<IInspectable> GetBitmapForCGImage(CGImageRef img) override {
        auto content = std::make_unique<DisplayTextureContent>(img);
        return content->GetContent();
    }

    std::shared_ptr<DisplayTexture> CreateWritableBitmapTexture32(int width, int height) override {
        return std::make_shared<DisplayTextureContent>(width, height);
    }

    void* LockWritableBitmapTexture(const std::shared_ptr<DisplayTexture>& tex, int* stride) override {
        return reinterpret_cast<DisplayTextureContent*>(tex.get())->LockWritableBitmap(stride);
    }

    void UnlockWritableBitmapTexture(const std::shared_ptr<DisplayTexture>& tex) override {
        reinterpret_cast<DisplayTextureContent*>(tex.get())->UnlockWritableBitmap();
    }

    void EnableDisplaySyncNotification() override {
        EnableRenderingListener(OnRenderedFrame);
    }

    void DisableDisplaySyncNotification() override {
        DisableRenderingListener();
    }
};

void CreateXamlCompositor(winobjc::Id& root, CompositionMode compositionMode) {
    g_compositionMode = compositionMode;
    CGImageAddDestructionListener(UIReleaseDisplayTextureForCGImage);
    static CAXamlCompositor* compIntr = new CAXamlCompositor();
    SetCACompositor(compIntr);
    SetRootGrid(root);
}

void GridSizeChanged(float newWidth, float newHeight) {
    [[UIApplication displayMode] _setWindowSize:CGSizeMake(newWidth, newHeight)];
    [[UIApplication displayMode] _updateDisplaySettings];
}
