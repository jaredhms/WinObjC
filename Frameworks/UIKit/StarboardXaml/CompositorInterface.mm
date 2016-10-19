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
#import <algorithm>
#import <deque>
#import <map>
#import <mutex>
#import <algorithm>
#import "CompositorInterface.h"
#import "CAAnimationInternal.h"
#import "CALayerInternal.h"
#import "UWP/interopBase.h"
#import "UIApplicationInternal.h"
#import <LoggingNative.h>
#import "StringHelpers.h"
#import <MainDispatcher.h>
#import "CoreGraphics/CGImage.h"

#import <UWP/WindowsUIViewManagement.h>
#import <UWP/WindowsDevicesInput.h>
#import "UIColorInternal.h"

using namespace Microsoft::WRL;

static const wchar_t* TAG = L"CompositorInterface";

@class RTObject;

CompositionMode g_compositionMode = CompositionModeDefault;

float screenWidth = 320.0f;
float screenHeight = 480.0f;
float screenMagnification = 1.0f;

bool tabletMode = false;

float screenXDpi = 180, screenYDpi = 180;
int deviceWidth = 640, deviceHeight = 1136;

/////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: MOVE TO BITMAP MANAGMENT API
ComPtr<IInspectable> CreateBitmapFromBits(void* ptr, int width, int height, int stride);
ComPtr<IInspectable> CreateBitmapFromImageData(const void* ptr, int len);
ComPtr<IInspectable> CreateWritableBitmap(int width, int height);
ComPtr<IInspectable> LockWritableBitmap(const ComPtr<IInspectable>& bitmap, void** ptr, int* stride);
/////////////////////////////////////////////////////////////////////////////////////////////////

void SetScreenParameters(float width, float height, float magnification, float rotation);
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
std::map<CGImageRef, DisplayTextureRef> _displayTextureCache;

DisplayTextureRef CachedDisplayTextureForImage(CGImageRef img) {
    DisplayTextureRef ret = NULL;
    _displayTextureCacheLock.lock();
    auto texRef = _displayTextureCache.find(img);
    if (texRef != _displayTextureCache.end()) {
        ret = texRef->second;
    }
    _displayTextureCacheLock.unlock();

    return ret;
}

void SetCachedDisplayTextureForImage(CGImageRef img, DisplayTexture* texture) {
    _displayTextureCacheLock.lock();
    if (!texture) {
        auto texRef = _displayTextureCache.find(img);
        if (texRef != _displayTextureCache.end()) {
            _displayTextureCache.erase(texRef);
        }
    } else {
        _displayTextureCache[img] = texture;
    }
    _displayTextureCacheLock.unlock();
}

void UIReleaseDisplayTextureForCGImage(CGImageRef img) {
    SetCachedDisplayTextureForImage(img, NULL);
}

void RefCountedType::AddRef() {
    assert(refCount > 0 && refCount != 0xDDDDDDDD);
    EbrIncrement(&refCount);
}

void RefCountedType::Release() {
    assert(refCount > 0 && refCount != 0xDDDDDDDD);
    if (EbrDecrement(&refCount) == 0) {
        delete this;
    }
}

RefCountedType::RefCountedType() {
    refCount = 1;
}

RefCountedType::~RefCountedType() {
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

class DisplayAnimationTransition : public DisplayAnimation {
public:
    id _animHandler;
    NSString *_type, *_subType;

    void Completed() {
        id animHandler = _animHandler; // Save in a local for the block to retain.
        dispatch_async(dispatch_get_main_queue(),
                       ^{
                           [animHandler animationDidStop:TRUE];
                           [animHandler _removeAnimationsFromLayer];
                       });
    }

    DisplayAnimationTransition(id animHandler, NSString* type, NSString* subType) {
        _animHandler = [animHandler retain];

        _type = [type retain];
        _subType = [subType retain];
        beginTime = 0;
        duration = 1.0f;
        autoReverse = false;
        repeatCount = 0;
        repeatDuration = 0;
        speed = 1.5;
        timeOffset = 0;
    }

    ~DisplayAnimationTransition() {
        [_animHandler release];
        [_type release];
        [_subType release];
    }

    concurrency::task<void> AddToNode(DisplayNode& node) {
        CreateXamlAnimation();
        return AddTransitionAnimation(node, [_type UTF8String], [_subType UTF8String]);
    }
};

class DisplayAnimationBasic : public DisplayAnimation {
private:
    void _adjustFloatValuesForKeyPath(NSString* curKeyPath) {
        if (_fromValue != nil) {
            if (_toValue != nil) {
                // Interpolate between _fromValue and _toValue.
                return;
            } else if (_byValue != nil) {
                // Interpolate between _fromValue and (_fromValue + _byValue)
                float fromValue = [static_cast<NSNumber*>(_fromValue) floatValue];
                float byValue = [static_cast<NSNumber*>(_byValue) floatValue];
                _toValue = [[NSNumber numberWithFloat:(fromValue + byValue)] retain];
            } else {
                FAIL_FAST_MSG("Guaranteed to be taken care of by _createAnimation in CABasicAnimation.");
            }
        } else if (_toValue != nil) {
            if (_byValue != nil) {
                // Interpolate between (_toValue - _byValue) and _toValue
                float toValue = [static_cast<NSNumber*>(_toValue) floatValue];
                float byValue = [static_cast<NSNumber*>(_byValue) floatValue];
                _fromValue = [[NSNumber numberWithFloat:(toValue - byValue)] retain];
            } else {
                FAIL_FAST_MSG("Guaranteed to be taken care of by _createAnimation in CABasicAnimation.");
            }
        } else if (_byValue != nil) {
            FAIL_FAST_MSG("Guaranteed to be taken care of by _createAnimation in CABasicAnimation.");
        } else {
            UNIMPLEMENTED_WITH_MSG("Unsupported when all CABasicAnimation properties are nil");
        }
    }

    void _adjustCGSizeValuesForKeyPath(NSString* curKeyPath) {
        if (_fromValue != nil && _toValue != nil) {
            return;
        }

        bool fromValid = _fromValue != nil;
        bool toValid = _toValue != nil;
        bool byValid = _byValue != nil;

        CGSize fromSize;
        CGSize toSize;
        CGSize bySize;

        if (fromValid) {
            [static_cast<NSValue*>(_fromValue) getValue:&fromSize];
            [_fromValue release];
            _fromValue = [[NSNumber numberWithFloat:fromSize.width] retain];
        }

        if (toValid) {
            [static_cast<NSValue*>(_toValue) getValue:&toSize];
            [_toValue release];
            _toValue = [[NSNumber numberWithFloat:toSize.width] retain];
        }

        if (byValid) {
            [static_cast<NSValue*>(_byValue) getValue:&bySize];
            [_byValue release];
            _byValue = [NSNumber numberWithFloat:bySize.width];
        }

        NSString* newKeyPath = [curKeyPath stringByAppendingString:@".width"];
        _adjustFloatValuesForKeyPath(newKeyPath);
        fromSize.width = [static_cast<NSNumber*>(_fromValue) floatValue];
        toSize.width = [static_cast<NSNumber*>(_toValue) floatValue];
        [_fromValue release];
        [_toValue release];

        if (fromValid) {
            _fromValue = [[NSNumber numberWithFloat:fromSize.height] retain];
        } else {
            _fromValue = nil;
        }

        if (toValid) {
            _toValue = [[NSNumber numberWithFloat:toSize.height] retain];
        } else {
            _toValue = nil;
        }

        if (byValid) {
            _byValue = [NSNumber numberWithFloat:bySize.height];
        } else {
            _byValue = nil;
        }

        newKeyPath = [curKeyPath stringByAppendingString:@".height"];
        _adjustFloatValuesForKeyPath(newKeyPath);
        fromSize.height = [static_cast<NSNumber*>(_fromValue) floatValue];
        toSize.height = [static_cast<NSNumber*>(_toValue) floatValue];
        [_fromValue release];
        [_toValue release];

        _fromValue = [[NSValue valueWithCGSize:fromSize] retain];
        _toValue = [[NSValue valueWithCGSize:toSize] retain];
        _byValue = nil;
    }

    void _adjustCGPointValuesForKeyPath(NSString* curKeyPath) {
        if (_fromValue != nil && _toValue != nil) {
            return;
        }

        bool fromValid = _fromValue != nil;
        bool toValid = _toValue != nil;
        bool byValid = _byValue != nil;

        CGPoint fromPoint;
        CGPoint toPoint;
        CGPoint byPoint;

        if (fromValid) {
            [static_cast<NSValue*>(_fromValue) getValue:&fromPoint];
            [_fromValue release];
            _fromValue = [[NSNumber numberWithFloat:fromPoint.x] retain];
        }

        if (toValid) {
            [static_cast<NSValue*>(_toValue) getValue:&toPoint];
            [_toValue release];
            _toValue = [[NSNumber numberWithFloat:toPoint.x] retain];
        }

        if (byValid) {
            [static_cast<NSValue*>(_byValue) getValue:&byPoint];
            [_byValue release];
            _byValue = [NSNumber numberWithFloat:byPoint.x];
        }

        NSString* newKeyPath = [curKeyPath stringByAppendingString:@".x"];
        _adjustFloatValuesForKeyPath(newKeyPath);
        fromPoint.x = [static_cast<NSNumber*>(_fromValue) floatValue];
        toPoint.x = [static_cast<NSNumber*>(_toValue) floatValue];
        [_fromValue release];
        [_toValue release];

        if (fromValid) {
            _fromValue = [[NSNumber numberWithFloat:fromPoint.y] retain];
        } else {
            _fromValue = nil;
        }

        if (toValid) {
            _toValue = [[NSNumber numberWithFloat:toPoint.y] retain];
        } else {
            _toValue = nil;
        }

        if (byValid) {
            _byValue = [NSNumber numberWithFloat:byPoint.y];
        } else {
            _byValue = nil;
        }

        newKeyPath = [curKeyPath stringByAppendingString:@".y"];
        _adjustFloatValuesForKeyPath(newKeyPath);
        fromPoint.y = [static_cast<NSNumber*>(_fromValue) floatValue];
        toPoint.y = [static_cast<NSNumber*>(_toValue) floatValue];
        [_fromValue release];
        [_toValue release];

        _fromValue = [[NSValue valueWithCGPoint:fromPoint] retain];
        _toValue = [[NSValue valueWithCGPoint:toPoint] retain];
        _byValue = nil;
    }

    void _adjustCGRectValuesForKeyPath(NSString* curKeyPath) {
        if (_fromValue != nil && _toValue != nil) {
            return;
        }

        bool fromValid = _fromValue != nil;
        bool toValid = _toValue != nil;
        bool byValid = _byValue != nil;

        CGRect fromRect;
        CGRect toRect;
        CGRect byRect;

        if (fromValid) {
            [static_cast<NSValue*>(_fromValue) getValue:&fromRect];
            [_fromValue release];
            _fromValue = [[NSValue valueWithCGPoint:fromRect.origin] retain];
        }

        if (toValid) {
            [static_cast<NSValue*>(_toValue) getValue:&toRect];
            [_toValue release];
            _toValue = [[NSValue valueWithCGPoint:toRect.origin] retain];
        }

        if (byValid) {
            [static_cast<NSValue*>(_byValue) getValue:&byRect];
            [_byValue release];

            // _byValue is will be released by _adjustCGPointValuesForKeyPath
            _byValue = [[NSValue valueWithCGPoint:byRect.origin] retain];
        }

        NSString* newKeyPath = [curKeyPath stringByAppendingString:@".origin"];
        _adjustCGPointValuesForKeyPath(newKeyPath);
        [static_cast<NSValue*>(_fromValue) getValue:&(fromRect.origin)];
        [static_cast<NSValue*>(_toValue) getValue:&(toRect.origin)];
        [_fromValue release];
        [_toValue release];

        if (fromValid) {
            _fromValue = [[NSValue valueWithCGSize:fromRect.size] retain];
        } else {
            _fromValue = nil;
        }

        if (toValid) {
            _toValue = [[NSValue valueWithCGSize:toRect.size] retain];
        } else {
            _toValue = nil;
        }

        if (byValid) {
            // _byValue is will be released by _adjustCGSizeValuesForKeyPath
            _byValue = [[NSValue valueWithCGSize:byRect.size] retain];
        } else {
            _byValue = nil;
        }

        newKeyPath = [curKeyPath stringByAppendingString:@".size"];
        _adjustCGSizeValuesForKeyPath(newKeyPath);
        [static_cast<NSValue*>(_fromValue) getValue:&(fromRect.size)];
        [static_cast<NSValue*>(_toValue) getValue:&(toRect.size)];
        [_fromValue release];
        [_toValue release];

        _fromValue = [[NSValue valueWithCGRect:fromRect] retain];
        _toValue = [[NSValue valueWithCGRect:toRect] retain];
        _byValue = nil;
    }

    bool _adjustCATransform3DValues(
        float* translationFrom, float* scaleFrom, float* angleFrom, float* translationTo, float* scaleTo, float* angleTo) {
        const int dimensions = 3;
        float translationBy[dimensions] = { 0 };
        float scaleBy[dimensions] = { 1.0f, 1.0f, 1.0f };
        float angleBy = 0.0f;
        Quaternion qval;
        CATransform3D fromValue = [static_cast<NSValue*>(_fromValue) CATransform3DValue];
        CATransform3D toValue = [static_cast<NSValue*>(_toValue) CATransform3DValue];
        CATransform3D byValue = [static_cast<NSValue*>(_byValue) CATransform3DValue];
        float (^add)(float, float) = ^(float a, float b) {
            return (a + b);
        };
        float (^subtract)(float, float) = ^(float a, float b) {
            return (a - b);
        };
        float (^multiply)(float, float) = ^(float a, float b) {
            return a * b;
        };
        float (^divide)(float, float) = ^(float a, float b) {
            if (b == 0.0f) {
                return 0.0f;
            }
            return a / b;
        };
        void (^performOperation)(float*, float*, float*, int, float (^)(float, float)) =
            ^(float* array1, float* array2, float* resultArray, int size, float (^operation)(float, float)) {
                for (int i = 0; i < size; i++) {
                    resultArray[i] = operation(array1[i], array2[i]);
                }
            };

        bool isValid = true;
        if (_fromValue != nil) {
            qval.CreateFromMatrix(reinterpret_cast<float*>(&fromValue));
            *angleFrom = (float)-qval.roll() * 180.0f / M_PI;
            CATransform3DGetScale(fromValue, scaleFrom);
            CATransform3DGetPosition(fromValue, translationFrom);
            if (_toValue != nil) {
                // Interpolate between _fromValue and _toValue.
                qval.CreateFromMatrix(reinterpret_cast<float*>(&toValue));
                *angleTo = (float)-qval.roll() * 180.0f / M_PI;
                CATransform3DGetScale(toValue, scaleTo);
                CATransform3DGetPosition(toValue, translationTo);
            } else if (_byValue != nil) {
                // Interpolate between _fromValue and (_fromValue + _byValue)
                qval.CreateFromMatrix(reinterpret_cast<float*>(&byValue));
                angleBy = (float)-qval.roll() * 180.0f / M_PI;
                CATransform3DGetScale(byValue, scaleBy);
                CATransform3DGetPosition(byValue, translationBy);
                *angleTo = *angleFrom + angleBy;
                performOperation(scaleFrom, scaleBy, scaleTo, dimensions, multiply);
                performOperation(translationFrom, translationBy, translationTo, dimensions, add);
            } else {
                // Guaranteed to be taken care of by _createAnimation in CABasicAnimation.
                FAIL_FAST_MSG("Guaranteed to be taken care of by _createAnimation in CABasicAnimation.");
                isValid = false;
            }
        } else if (_toValue != nil) {
            qval.CreateFromMatrix(reinterpret_cast<float*>(&toValue));
            *angleTo = (float)-qval.roll() * 180.0f / M_PI;
            CATransform3DGetScale(toValue, scaleTo);
            CATransform3DGetPosition(toValue, translationTo);
            if (_byValue != nil) {
                // Interpolate between (_toValue - _byValue) and _toValue
                qval.CreateFromMatrix(reinterpret_cast<float*>(&byValue));
                angleBy = (float)-qval.roll() * 180.0f / M_PI;
                CATransform3DGetScale(byValue, scaleBy);
                CATransform3DGetPosition(byValue, translationBy);

                *angleFrom = *angleTo - angleBy;
                performOperation(scaleTo, scaleBy, scaleFrom, dimensions, divide);
                performOperation(translationTo, translationBy, translationFrom, dimensions, subtract);
            } else {
                FAIL_FAST_MSG("Guaranteed to be taken care of by _createAnimation in CABasicAnimation.");
                isValid = false;
            }
        } else if (_byValue != nil) {
            FAIL_FAST_MSG("Guaranteed to be taken care of by _createAnimation in CABasicAnimation.");
            isValid = false;
        } else {
            UNIMPLEMENTED_WITH_MSG("Unsupported when all interpolation values are nil");
            isValid = false;
        }
        return isValid;
    }

public:
    NSString* _propertyName;
    NSObject* _fromValue;
    NSObject* _toValue;
    NSObject* _byValue;
    id _animHandler;

    void Completed() {
        id animHandler = _animHandler; // Save in a local for the block to retain.
        dispatch_async(dispatch_get_main_queue(),
                       ^{
                           [animHandler animationDidStop:TRUE];
                           [animHandler _removeAnimationsFromLayer];
                       });
    }

    DisplayAnimationBasic(id animHandler,
                          NSString* propertyName,
                          NSObject* fromValue,
                          NSObject* toValue,
                          NSObject* byValue,
                          CAMediaTimingProperties* timingProperties) {
        CAMediaTimingFunction* mediaTiming = timingProperties->_timingFunction;
        float p1[2] = { 0 }, p2[2] = { 0 };
        [mediaTiming getControlPointAtIndex:0 values:p1];
        [mediaTiming getControlPointAtIndex:1 values:p2];

        if (p1[0] == 0.0f && p1[1] == 0.0f && p2[0] == 1.0f && p2[1] == 1.0f) {
            easingFunction = Linear;
        } else if (p1[0] == 0.5f && p1[1] == 0.0f && p2[0] == 1.0f && p2[1] == 1.0f) {
            easingFunction = EaseIn;
        } else if (p1[0] == 0.0f && p1[1] == 0.0f && p2[0] == 0.5f && p2[1] == 1.0f) {
            easingFunction = EaseOut;
        } else if (p1[0] == 0.5f && p1[1] == 0.0f && p2[0] == 0.5f && p2[1] == 1.0f) {
            easingFunction = EaseInEaseOut;
        } else if (p1[0] == 0.5f && p1[1] == 0.0f && p2[0] == 1.0f && p2[1] == 1.0f) {
            easingFunction = Default;
        }

        _animHandler = [animHandler retain];
        _propertyName = [propertyName retain];
        _fromValue = [fromValue retain];
        _toValue = [toValue retain];
        _byValue = [byValue retain];

        beginTime = timingProperties->_beginTime - CACurrentMediaTime();
        duration = timingProperties->_duration;
        autoReverse = (bool)timingProperties->_autoReverses;
        repeatCount = timingProperties->_repeatCount;
        repeatDuration = timingProperties->_repeatDuration;
        speed = timingProperties->_speed;
        timeOffset = timingProperties->_timeOffset;

        if (repeatCount > 0xFFFFFF)
            repeatCount = 0xFFFFFF;
        if (repeatDuration > 0xFFFFFF)
            repeatDuration = 0xFFFFFF;
    }

    ~DisplayAnimationBasic() {
        [_animHandler release];
        [_propertyName release];
        [_fromValue release];
        [_toValue release];
        [_byValue release];
    }

    concurrency::task<void> AddToNode(DisplayNode& node) {
        CreateXamlAnimation();

        const char* propName = [_propertyName UTF8String];
        if (strcmp(propName, "transform.scale") == 0) {
            _adjustFloatValuesForKeyPath(_propertyName);
            float fromValue = [(NSNumber*)_fromValue floatValue];
            float toValue = [(NSNumber*)_toValue floatValue];

            AddAnimation(node, L"transform.scale.x", _fromValue != nil, fromValue, _toValue != nil, toValue);
            AddAnimation(node, L"transform.scale.y", _fromValue != nil, fromValue, _toValue != nil, toValue);
            Start();
        } else if (strcmp(propName, "transform.rotation.z") == 0) {
            _adjustFloatValuesForKeyPath(_propertyName);
            float fromValue = [(NSNumber*)_fromValue floatValue];
            float toValue = [(NSNumber*)_toValue floatValue];

            fromValue = fromValue * 180.0f / M_PI;
            toValue = toValue * 180.0f / M_PI;

            AddAnimation(node, L"transform.rotation", _fromValue != nil, fromValue, _toValue != nil, toValue);
            Start();
        } else if (strcmp(propName, "transform.translation.x") == 0) {
            _adjustFloatValuesForKeyPath(_propertyName);
            float fromValue = [(NSNumber*)_fromValue floatValue];
            float toValue = [(NSNumber*)_toValue floatValue];

            AddAnimation(node, L"transform.translation.x", _fromValue != nil, fromValue, _toValue != nil, toValue);
            Start();
        } else if (strcmp(propName, "transform.translation.y") == 0) {
            _adjustFloatValuesForKeyPath(_propertyName);
            float fromValue = [(NSNumber*)_fromValue floatValue];
            float toValue = [(NSNumber*)_toValue floatValue];

            AddAnimation(node, L"transform.translation.y", _fromValue != nil, fromValue, _toValue != nil, toValue);
            Start();
        } else if (strcmp(propName, "position") == 0) {
            _adjustCGPointValuesForKeyPath(_propertyName);
            CGPoint fromValue = [(NSValue*)_fromValue CGPointValue];
            CGPoint toValue = [(NSValue*)_toValue CGPointValue];

            AddAnimation(node, L"position.x", _fromValue != nil, fromValue.x, _toValue != nil, toValue.x);
            AddAnimation(node, L"position.y", _fromValue != nil, fromValue.y, _toValue != nil, toValue.y);
            Start();
        } else if (strcmp(propName, "bounds") == 0) {
            _adjustCGRectValuesForKeyPath(_propertyName);
            CGRect fromValue = [(NSValue*)_fromValue CGRectValue];
            CGRect toValue = [(NSValue*)_toValue CGRectValue];

            AddAnimation(node, L"origin.x", _fromValue != nil, fromValue.origin.x, _toValue != nil, toValue.origin.x);
            AddAnimation(node, L"origin.y", _fromValue != nil, fromValue.origin.y, _toValue != nil, toValue.origin.y);
            AddAnimation(node, L"size.width", _fromValue != nil, fromValue.size.width, _toValue != nil, toValue.size.width);
            AddAnimation(node, L"size.height", _fromValue != nil, fromValue.size.height, _toValue != nil, toValue.size.height);
            Start();
        } else if (strcmp(propName, "opacity") == 0) {
            _adjustFloatValuesForKeyPath(_propertyName);
            float fromValue = [(NSNumber*)_fromValue floatValue];
            float toValue = [(NSNumber*)_toValue floatValue];
            AddAnimation(node, L"opacity", _fromValue != nil, fromValue, _toValue != nil, toValue);
            Start();
        } else if (strcmp(propName, "transform") == 0) {
            CATransform3D fromValue = [(NSValue*)_fromValue CATransform3DValue];
            CATransform3D toValue = [(NSValue*)_toValue CATransform3DValue];

            float translationFrom[3] = { 0 };
            float scaleFrom[3] = { 1.0f, 1.0f, 1.0f };
            float angleFrom = 0.0f;
            float translationTo[3] = { 0 };
            float scaleTo[3] = { 1.0f, 1.0f, 1.0f };
            float angleTo = 0.0f;

            bool isValid = _adjustCATransform3DValues(translationFrom, scaleFrom, &angleFrom, translationTo, scaleTo, &angleTo);

            if (scaleFrom[0] != 1.0f || scaleFrom[1] != 1.0f || scaleTo[0] != 1.0f || scaleTo[1] != 1.0f) {
                AddAnimation(node, L"transform.scale.x", isValid, scaleFrom[0], isValid, scaleTo[0]);
                AddAnimation(node, L"transform.scale.y", isValid, scaleFrom[1], isValid, scaleTo[1]);
            }
            if (translationFrom[0] != 0.0f || translationFrom[1] != 0.0f || translationTo[0] != 0.0f || translationTo[1] != 0.0f) {
                AddAnimation(node, L"transform.translation.x", isValid, translationFrom[0], isValid, translationTo[0]);
                AddAnimation(node, L"transform.translation.y", isValid, translationFrom[1], isValid, translationTo[1]);
            }
            if (angleFrom != 0.0f || angleTo != 0.0f) {
                AddAnimation(node, L"transform.rotation", isValid, angleFrom, isValid, angleTo);
            }
            Start();
        } else if (strcmp(propName, "transform.translation.y") == 0) {
            _adjustFloatValuesForKeyPath(_propertyName);
            float fromValue = [(NSNumber*)_fromValue floatValue];
            float toValue = [(NSNumber*)_toValue floatValue];

            AddAnimation(node, L"transform.translation.y", _fromValue != nil, fromValue, _toValue != nil, toValue);
            Start();
        } else if (strcmp(propName, "transform.translation.x") == 0) {
            _adjustFloatValuesForKeyPath(_propertyName);
            float fromValue = [(NSNumber*)_fromValue floatValue];
            float toValue = [(NSNumber*)_toValue floatValue];

            AddAnimation(node, L"transform.translation.x", _fromValue != nil, fromValue, _toValue != nil, toValue);
            Start();
        } else {
            UNIMPLEMENTED_WITH_MSG("Stubbed function called! Unsupported property name: %hs", propName);
        }

        // No specialized task needed for basic animations
        return concurrency::task_from_result();
    }
};

void DisplayNode::SetTexture(DisplayTexture* texture, float width, float height, float contentScale) {
    _currentTexture = texture;
    SetContents((texture ? texture->GetContent() : nullptr), width, height, contentScale);
}

void* DisplayNode::GetProperty(const char* name) {
    NSObject* ret = nil;

    if (strcmp(name, "position") == 0) {
        CGPoint pos;

        pos.x = _GetPresentationPropertyValue("position.x");
        pos.y = _GetPresentationPropertyValue("position.y");

        ret = [NSValue valueWithCGPoint:pos];
    } else if (strcmp(name, "bounds.origin") == 0) {
        CGPoint pos;

        pos.x = _GetPresentationPropertyValue("origin.x");
        pos.y = _GetPresentationPropertyValue("origin.y");

        ret = [NSValue valueWithCGPoint:pos];
    } else if (strcmp(name, "bounds.size") == 0) {
        CGSize size;

        size.width = _GetPresentationPropertyValue("size.width");
        size.height = _GetPresentationPropertyValue("size.height");

        ret = [NSValue valueWithCGSize:size];
    } else if (strcmp(name, "bounds") == 0) {
        CGRect rect;

        rect.size.width = _GetPresentationPropertyValue("size.width");
        rect.size.height = _GetPresentationPropertyValue("size.height");
        rect.origin.x = _GetPresentationPropertyValue("origin.x");
        rect.origin.y = _GetPresentationPropertyValue("origin.y");

        ret = [NSValue valueWithCGRect:rect];
    } else if (strcmp(name, "opacity") == 0) {
        float value = _GetPresentationPropertyValue("opacity");

        ret = [NSNumber numberWithFloat:value];
    } else if (strcmp(name, "transform") == 0) {
        float angle = _GetPresentationPropertyValue("transform.rotation");
        float scale[2];
        float translation[2];

        scale[0] = _GetPresentationPropertyValue("transform.scale.x");
        scale[1] = _GetPresentationPropertyValue("transform.scale.y");

        translation[0] = _GetPresentationPropertyValue("transform.translation.x");
        translation[1] = _GetPresentationPropertyValue("transform.translation.y");

        CATransform3D trans = CATransform3DMakeRotation(-angle * M_PI / 180.0f, 0, 0, 1.0f);
        trans = CATransform3DScale(trans, scale[0], scale[1], 0.0f);
        trans = CATransform3DTranslate(trans, translation[0], translation[1], 0.0f);

        ret = [NSValue valueWithCATransform3D:trans];
    } else {
        FAIL_FAST_HR(E_NOTIMPL);
    }

    return ret;
}

void DisplayNode::UpdateProperty(const char* name, void* value) {
    NSObject* newValue = (NSObject*)value;
    if (name == NULL)
        return;
    if ([NSThread currentThread] != [NSThread mainThread]) {
        return;
    }

    if (strcmp(name, "contentsCenter") == 0) {
        CGRect value = [(NSValue*)newValue CGRectValue];
        SetContentsCenter(value.origin.x, value.origin.y, value.size.width, value.size.height);
    } else if (strcmp(name, "anchorPoint") == 0) {
        CGPoint value = [(NSValue*)newValue CGPointValue];
        SetProperty(L"anchorPoint.x", value.x);
        SetProperty(L"anchorPoint.y", value.y);
    } else if (strcmp(name, "position") == 0) {
        CGPoint position = [(NSValue*)newValue CGPointValue];
        SetProperty(L"position.x", position.x);
        SetProperty(L"position.y", position.y);
    } else if (strcmp(name, "bounds.origin") == 0) {
        CGPoint value = [(NSValue*)newValue CGPointValue];
        SetProperty(L"origin.x", value.x);
        SetProperty(L"origin.y", value.y);
    } else if (strcmp(name, "bounds.size") == 0) {
        CGSize size = [(NSValue*)newValue CGSizeValue];
        SetProperty(L"size.width", size.width);
        SetProperty(L"size.height", size.height);
    } else if (strcmp(name, "opacity") == 0) {
        float value = [(NSNumber*)newValue floatValue];
        SetProperty(L"opacity", value);
    } else if (strcmp(name, "hidden") == 0) {
        bool value = [(NSNumber*)newValue boolValue];
        SetHidden(value);
    } else if (strcmp(name, "masksToBounds") == 0) {
        if (!_isRoot) {
            bool value = [(NSNumber*)newValue boolValue];
            SetMasksToBounds(value);
        } else {
            SetMasksToBounds(true);
        }
    } else if (strcmp(name, "transform") == 0) {
        CATransform3D value = [(NSValue*)newValue CATransform3DValue];
        float translation[3] = { 0 };
        float scale[3] = { 0 };

        Quaternion qFrom;
        qFrom.CreateFromMatrix((float*)&(value));

        CATransform3DGetScale(value, scale);
        CATransform3DGetPosition(value, translation);

        SetProperty(L"transform.rotation", (float)-qFrom.roll() * 180.0f / M_PI);
        SetProperty(L"transform.scale.x", scale[0]);
        SetProperty(L"transform.scale.y", scale[1]);
        SetProperty(L"transform.translation.x", translation[0]);
        SetProperty(L"transform.translation.y", translation[1]);
    } else if (strcmp(name, "contentsScale") == 0) {
        UNIMPLEMENTED_WITH_MSG("contentsScale not implemented");
    } else if (strcmp(name, "contentsOrientation") == 0) {
        int position = [(NSNumber*)newValue intValue];
        float toPosition = 0;
        if (position == UIImageOrientationUp) {
            toPosition = 0;
        } else if (position == UIImageOrientationDown) {
            toPosition = 180;
        } else if (position == UIImageOrientationLeft) {
            toPosition = 270;
        } else if (position == UIImageOrientationRight) {
            toPosition = 90;
        }
        SetProperty(L"transform.rotation", toPosition);
    } else if (strcmp(name, "zIndex") == 0) {
        ///////////////////////////////////////////////////////////////////////////////////////
        // TODO: This should just happen in UIWindow.mm and should get deleted from here
        int value = [(NSNumber*)newValue intValue];
        SetNodeZIndex(value);
        ///////////////////////////////////////////////////////////////////////////////////////
    } else if (strcmp(name, "gravity") == 0) {
        SetPropertyInt(L"gravity", [(NSNumber*)newValue intValue]);
    } else if (strcmp(name, "backgroundColor") == 0) {
        const __CGColorQuad* color = [(UIColor*)newValue _getColors];
        if (color) {
            SetBackgroundColor(color->r, color->g, color->b, color->a);
        } else {
            SetBackgroundColor(0.0f, 0.0f, 0.0f, 0.0f);
        }
    } else {
        FAIL_FAST_HR(E_NOTIMPL);
    }
}

class QueuedAnimation : public ICompositorAnimationTransaction {
public:
    id _layer = nil;
    id _animation = nil;
    id _key = nil;
    std::shared_ptr<DisplayAnimation> _abortAnimation;

    QueuedAnimation(id layer, id anim, id key) {
        _layer = [layer retain];
        _animation = [anim retain];
        _key = [key retain];
    }

    QueuedAnimation(const std::shared_ptr<DisplayAnimation>& animToAbort) {
        _abortAnimation = animToAbort;
    }

    ~QueuedAnimation() {
        [_layer release];
        [_animation release];
        [_key release];
    }

    concurrency::task<void> Process() override {
        if (_abortAnimation) {
            _abortAnimation->Stop();
        } else {
            if (![_animation wasRemoved] && ![_animation wasAborted]) {
                std::shared_ptr<DisplayAnimation> newAnimation = [_animation _createAnimation:_layer forKey:_key];
                [_animation animationDidStart];
                [_animation animationHasStarted];

                if (newAnimation) {
                    std::shared_ptr<DisplayNode> node = [_layer _presentationNode];
                    return newAnimation->AddToNode(*node);
                } else {
                    [_animation animationDidStop:FALSE];
                }
            }
        }

        return concurrency::task_from_result();
    }
};

class QueuedProperty : public ICompositorTransaction {
public:
    std::shared_ptr<DisplayNode> _node;
    char* _propertyName;
    NSObject* _propertyValue;
    DisplayTextureRef _newTexture;
    CGSize _contentsSize;
    float _contentsScale;
    bool _applyingTexture;

    QueuedProperty(const std::shared_ptr<DisplayNode>& node, DisplayTexture* newTexture, CGSize contentsSize, float contentsScale) {
        _node = node;
        _propertyName = IwStrDup("contents");
        _propertyValue = NULL;
        _newTexture = newTexture;
        _contentsScale = contentsScale;
        _contentsSize = contentsSize;
        _applyingTexture = true;
    }

    QueuedProperty(const std::shared_ptr<DisplayNode>& node, const char* propertyName, NSObject* propertyValue) {
        _node = node;
        _propertyName = IwStrDup(propertyName);
        _propertyValue = [propertyValue retain];
        _newTexture = NULL;
        _applyingTexture = false;
    }

    ~QueuedProperty() {
        if (_propertyName) {
            IwFree(_propertyName);
        }

        [_propertyValue release];
        _propertyName = nullptr;
        _propertyValue = nullptr;
    }

    void Process() override {
        if (_applyingTexture) {
            _node->SetTexture(_newTexture.Get(), _contentsSize.width, _contentsSize.height, _contentsScale);
        } else {
            _node->UpdateProperty(_propertyName, _propertyValue);
        }
    }
};

class QueuedNodeMovement : public ICompositorTransaction {
public:
    std::shared_ptr<DisplayNode> _node;
    std::shared_ptr<DisplayNode> _before, _after;
    std::shared_ptr<DisplayNode> _supernode;

    enum MovementType { Add, Move, Remove };

    MovementType _type;

    QueuedNodeMovement(MovementType type,
                       const std::shared_ptr<DisplayNode>& node,
                       const std::shared_ptr<DisplayNode>& before,
                       const std::shared_ptr<DisplayNode>& after,
                       const std::shared_ptr<DisplayNode>& supernode) {
        _type = type;
        _node = node;
        _before = before;
        _after = after;
        _supernode = supernode;
    }

    void Process() override {
        switch (_type) {
            case Add:
                if (!_supernode) {
                    _node->AddToRoot();
                } else {
                    _supernode->AddSubnode(_node, _before, _after);
                }
                break;

            case Move:
                _node->MoveNode(_before, _after);
                break;

            case Remove:
                _node->RemoveFromSupernode();
                break;
        }
    }
};

using namespace std;

class DisplayTransaction : public ICompositorTransaction {
    std::deque<std::shared_ptr<ICompositorTransaction>> _queuedTransactions;
    std::deque<std::shared_ptr<ICompositorAnimationTransaction>> _queuedAnimations;
    std::map<std::shared_ptr<DisplayNode>, std::map<std::string, std::shared_ptr<ICompositorTransaction>>> _queuedProperties;
    std::deque<std::shared_ptr<ICompositorTransaction>> _queuedNodeMovements;

public:
    void QueueProperty(const std::shared_ptr<QueuedProperty> property) {
        auto& currentUpdates = _queuedProperties[property->_node];
        currentUpdates[property->_propertyName] = property;
    }

    void QueueNodeMovement(const std::shared_ptr<QueuedNodeMovement>& nodeMovement) {
        _queuedNodeMovements.push_back(nodeMovement);
    }

    void QueueAnimation(const std::shared_ptr<QueuedAnimation>& animation) {
        _queuedAnimations.push_back(animation);
    }

    void QueueTransaction(const std::shared_ptr<DisplayTransaction>& transaction) {
        //  Move all of our existing queues into a new transaction
        if (!_queuedAnimations.empty() || !_queuedProperties.empty() || !_queuedNodeMovements.empty()) {
            auto currentChanges = std::make_shared<DisplayTransaction>();
            currentChanges->_queuedAnimations = std::move(_queuedAnimations);
            currentChanges->_queuedProperties = std::move(_queuedProperties);
            currentChanges->_queuedNodeMovements = std::move(_queuedNodeMovements);

            _queuedTransactions.push_back(currentChanges);
        }

        _queuedTransactions.push_back(transaction);
    }

    void Process() override {
        DispatchCompositorTransactions(std::move(_queuedTransactions),
                                       std::move(_queuedAnimations),
                                       std::move(_queuedProperties),
                                       std::move(_queuedNodeMovements));
    }
};

deque<std::shared_ptr<DisplayTransaction>> s_queuedTransactions;

class CAXamlCompositor : public CACompositorInterface {
public:
    virtual std::shared_ptr<DisplayNode> CreateDisplayNode(const ComPtr<IInspectable>& xamlElement) override {
        return std::make_shared<DisplayNode>(xamlElement.Get());
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TODO: WE SHOULDN'T NEED THIS ANYMORE, BUT IF WE DO, MOVE IT TO DISPLAYNODE AND JUST CALL IT GETXAMLELEMENT
    virtual ComPtr<IInspectable> GetXamlLayoutElement(const std::shared_ptr<DisplayNode>& displayNode) override {
        return displayNode->_xamlElement;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<DisplayTransaction> CreateDisplayTransaction() override {
        return std::make_shared<DisplayTransaction>();
    }

    void QueueDisplayTransaction(const std::shared_ptr<DisplayTransaction>& transaction,
                                 const std::shared_ptr<DisplayTransaction>& onTransaction) override {
        if (onTransaction) {
            onTransaction->QueueTransaction(transaction);
        } else {
            s_queuedTransactions.push_back(transaction);
        }
    }

    virtual void addNode(const std::shared_ptr<DisplayTransaction>& transaction,
                         const std::shared_ptr<DisplayNode>& node,
                         const std::shared_ptr<DisplayNode>& superNode,
                         const std::shared_ptr<DisplayNode>& beforeNode,
                         const std::shared_ptr<DisplayNode>& afterNode) override {
        transaction->QueueNodeMovement(
            std::make_shared<QueuedNodeMovement>(QueuedNodeMovement::Add, node, beforeNode, afterNode, superNode));
    }

    virtual void moveNode(const std::shared_ptr<DisplayTransaction>& transaction,
                          const std::shared_ptr<DisplayNode>& node,
                          const std::shared_ptr<DisplayNode>& beforeNode,
                          const std::shared_ptr<DisplayNode>& afterNode) override {
        transaction->QueueNodeMovement(
            std::make_shared<QueuedNodeMovement>(QueuedNodeMovement::Move, node, beforeNode, afterNode, nullptr));
    }

    virtual void removeNode(const std::shared_ptr<DisplayTransaction>& transaction, const std::shared_ptr<DisplayNode>& node) override {
        transaction->QueueNodeMovement(std::make_shared<QueuedNodeMovement>(QueuedNodeMovement::Remove, node, nullptr, nullptr, nullptr));
    }

    virtual void addAnimation(const std::shared_ptr<DisplayTransaction>& transaction, id layer, id animation, id forKey) override {
        transaction->QueueAnimation(std::make_shared<QueuedAnimation>(layer, animation, forKey));
    }

    virtual void removeAnimation(const std::shared_ptr<DisplayTransaction>& transaction,
                                 const std::shared_ptr<DisplayAnimation>& animation) override {
        transaction->QueueAnimation(std::make_shared<QueuedAnimation>(animation));
    }

    virtual void setNodeTexture(const std::shared_ptr<DisplayTransaction>& transaction,
                                const std::shared_ptr<DisplayNode>& node,
                                DisplayTexture* newTexture,
                                CGSize contentsSize,
                                float contentsScale) override {
        transaction->QueueProperty(std::make_shared<QueuedProperty>(node, newTexture, contentsSize, contentsScale));
    }

    virtual void setDisplayProperty(const std::shared_ptr<DisplayTransaction>& transaction,
                                    const std::shared_ptr<DisplayNode>& node,
                                    const char* propertyName,
                                    NSObject* newValue) override {
        transaction->QueueProperty(std::make_shared<QueuedProperty>(node, propertyName, newValue));
    }

    virtual void setNodeTopMost(const std::shared_ptr<DisplayNode>& node, bool topMost) override {
        node->SetTopMost();
    }

    virtual DisplayTexture* GetDisplayTextureForCGImage(CGImageRef img, bool create) override {
        CGImageRetain(img);
        DisplayTexture* ret = img->Backing()->GetDisplayTexture();
        if (ret) {
            ret->AddRef();
            CGImageRelease(img);
            return ret;
        }
        DisplayTextureRef cached = CachedDisplayTextureForImage(img);
        if (cached) {
            cached->AddRef();
            return cached.Get();
        }

        ret = new DisplayTextureContent(img);
        SetCachedDisplayTextureForImage(img, ret);
        CGImageRelease(img);
        return ret;
    }

    virtual ComPtr<IInspectable> GetBitmapForCGImage(CGImageRef img) override {
        auto content = std::make_unique<DisplayTextureContent>(img);
        return content->GetContent();
    }

    DisplayTexture* CreateWritableBitmapTexture32(int width, int height) override {
        DisplayTexture* ret = new DisplayTextureContent(width, height);
        return ret;
    }

    void* LockWritableBitmapTexture(DisplayTexture* tex, int* stride) override {
        return ((DisplayTextureContent*)tex)->LockWritableBitmap(stride);
    }

    void UnlockWritableBitmapTexture(DisplayTexture* tex) override {
        ((DisplayTextureContent*)tex)->UnlockWritableBitmap();
    }

    virtual std::shared_ptr<DisplayAnimation> GetBasicDisplayAnimation(id animobj,
                                                                       NSString* propertyName,
                                                                       NSObject* fromValue,
                                                                       NSObject* toValue,
                                                                       NSObject* byValue,
                                                                       CAMediaTimingProperties* timingProperties) override {
        return std::make_shared<DisplayAnimationBasic>(animobj, propertyName, fromValue, toValue, byValue, timingProperties);
    }

    virtual std::shared_ptr<DisplayAnimation> GetMoveDisplayAnimation(id animobj,
                                                                      const std::shared_ptr<DisplayNode>& animNode,
                                                                      NSString* typeStr,
                                                                      NSString* subtypeStr,
                                                                      CAMediaTimingProperties* timingProperties) override {
        return std::make_shared<DisplayAnimationTransition>(animobj, typeStr, subtypeStr);
    }

    virtual void RetainDisplayTexture(DisplayTexture* tex) override {
        tex->AddRef();
    }

    virtual void ReleaseDisplayTexture(DisplayTexture* tex) override {
        tex->Release();
    }

    virtual bool isTablet() override {
        return tabletMode;
    }
    virtual float screenWidth() override {
        return ::screenWidth;
    }
    virtual float screenHeight() override {
        return ::screenHeight;
    }
    virtual float screenScale() override {
        float scale = ::screenMagnification;

        if ([[UIApplication displayMode] useHostScaleFactor]) {
            scale *= [UIApplication displayMode].hostScreenScale;
        }

        // On an iOS device, the only expected values for UIScreen.scale is 1, or 2 for retina displays.
        // Some code paths rely on this.
        if ([[UIApplication displayMode] clampScaleToClosestExpected]) {
            // If we've set a fixed width or height, we're also expecting a fixed screenScale.
            if ([UIApplication displayMode].autoMagnification &&
                (([UIApplication displayMode].fixedWidth != 0) || ([UIApplication displayMode].fixedHeight != 0))) {
                static float fixedScreenScale = 0;
                static float prevWidth = 0;
                static float prevHeight = 0;

                if (fixedScreenScale &&
                    (([UIApplication displayMode].fixedWidth == prevWidth) && ([UIApplication displayMode].fixedHeight == prevHeight))) {
                    return fixedScreenScale;
                }

                prevWidth = [UIApplication displayMode].fixedWidth;
                prevHeight = [UIApplication displayMode].fixedHeight;

                float maxDimension = 0;
                NSArray* pointerDevices = [WDIPointerDevice getPointerDevices];

                for (int i = 0; i < [pointerDevices count]; i++) {
                    WFRect* screenRect = [(WDIPointerDevice*)[pointerDevices objectAtIndex:i] screenRect];
                    float hostScreenScale = [UIApplication displayMode].hostScreenScale;
                    maxDimension =
                        std::max(maxDimension, std::max(screenRect.width * hostScreenScale, screenRect.height * hostScreenScale));
                }

                // We can't know whether the app will be rotated, or moved from screen to screen. We have to take the
                // worst case scenario, and set the scale using the maximum possible dimension.

                float maxScreenDimension = std::max([UIApplication displayMode].fixedWidth, [UIApplication displayMode].fixedHeight);

                if (maxDimension == 0) {
                    TraceWarning(TAG, L"Could not determine screen size, defaulting to a screenScale of 2!");
                    fixedScreenScale = 2.0f;
                } else if (maxDimension > maxScreenDimension) {
                    fixedScreenScale = 2.0f;
                } else {
                    fixedScreenScale = 1.0f;
                }

                return fixedScreenScale;
            } else {
                // Round to nearest int
                scale = static_cast<float>(static_cast<int>(scale + 0.5f));

                // Clamp
                if (scale > 2.0f) {
                    scale = 2.0f;
                }
                if (scale < 1.0f) {
                    scale = 1.0f;
                }
            }
        }

        return scale;
    }
    virtual int deviceWidth() override {
        return ::deviceWidth;
    }
    virtual int deviceHeight() override {
        return ::deviceHeight;
    }
    virtual float screenXDpi() override {
        return ::screenXDpi;
    }
    virtual float screenYDpi() override {
        return ::screenYDpi;
    }

    virtual void ProcessTransactions() override {
        for (auto& transaction : s_queuedTransactions) {
            transaction->Process();
        }
        s_queuedTransactions.clear();
    }

    virtual void setScreenSize(float width, float height, float scale, float rotation) override {
        ::screenWidth = width;
        ::screenHeight = height;
        ::screenMagnification = scale;
        SetScreenParameters(::screenWidth, ::screenHeight, ::screenMagnification, rotation);
    }

    virtual void setDeviceSize(int width, int height) override {
        ::deviceWidth = width;
        ::deviceHeight = height;
    }

    virtual void setScreenDpi(int xDpi, int yDpi) override {
        ::screenXDpi = xDpi;
        ::screenYDpi = yDpi;
    }

    virtual void setTablet(bool isTablet) override {
        ::tabletMode = isTablet;
    }

    void EnableDisplaySyncNotification() override {
        EnableRenderingListener(OnRenderedFrame);
    }

    void DisableDisplaySyncNotification() override {
        DisableRenderingListener();
    }

    NSObject* getDisplayProperty(const std::shared_ptr<DisplayNode>& node, const char* propertyName) override {
        return (NSObject*)node->GetProperty(propertyName);
    }

    virtual void SetShouldRasterize(const std::shared_ptr<DisplayNode>& node, bool rasterize) override {
        node->SetShouldRasterize(rasterize);
    }

    virtual bool IsRunningAsFramework() override {
        return g_compositionMode == CompositionModeLibrary;
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
