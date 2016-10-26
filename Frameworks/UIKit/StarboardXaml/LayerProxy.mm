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
#import "LayerProxy.h"

#import "QuartzCore\CATransform3D.h"
#import "Quaternion.h"
#import "UIColorInternal.h"

static const wchar_t* TAG = L"LayerProxy";

void LayerProxy::SetTexture(const std::shared_ptr<IDisplayTexture>& texture, float width, float height, float contentScale) {
    _currentTexture = texture;
    SetContents((texture ? texture->GetContent() : nullptr), width, height, contentScale);
}

void* LayerProxy::GetPropertyValue(const char* name) {
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

void LayerProxy::UpdateProperty(const char* name, void* value) {
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
        SetZIndex(value);
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
