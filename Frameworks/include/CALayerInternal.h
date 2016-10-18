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

#ifndef _CALAYERPRIVATE_H_
#define _CALAYERPRIVATE_H_

#import <LinkedList.h>
#import <AccessibilityInternal.h>
#import <UIKit/UIImage.h>
#import "UIColorInternal.h"

@class CAAnimation, CALayerContext;

class CADisplayProperties {
public:
    BOOL hidden;
    CGRect bounds;
    CGRect contentsRect, contentsCenter;
    uint32_t contentsOrientation;
    CGPoint position;
    CGPoint anchorPoint;
    CGSize contentsSize;
    float contentsScale;
    uint32_t gravity;
    BOOL masksToBounds;
    BOOL isOpaque;

    CATransform3D transform;
    __CGColorQuad backgroundColor, borderColor;
    float borderWidth, cornerRadius;
    float opacity;
};

class DisplayNode;
class DisplayTexture;
@class CALayer;

class CAPrivateInfo : public CADisplayProperties, public LLTreeNode<CAPrivateInfo, CALayer> {
public:
    id delegate;
    NSArray* sublayers;
    CALayer* superlayer;
    CGImageRef contents;
    BOOL ownsContents;
    CGContextRef savedContext;
    BOOL isRootLayer;
    BOOL needsDisplay;
    BOOL needsUpdate;
    idretain _name;
    NSMutableDictionary* _animations;
    CGColorRef _backgroundColor, _borderColor;
    BOOL needsDisplayOnBoundsChange;

    BOOL _shouldRasterize;

    ////////////////////////////////////////////
    // TODO: USE A SHARED_PTR!
    DisplayNode* _presentationNode;
    ////////////////////////////////////////////

    // The XAML element backing this CALayer
    StrongId<WXFrameworkElement> _xamlElement;

    idretain _undefinedKeys;
    idretain _actions;

    bool _frameIsCached;
    CGRect _cachedFrame;

    BOOL needsLayout;
    BOOL didLayout;
    bool _displayPending;

    CAPrivateInfo(CALayer* self, WXFrameworkElement* xamlElement);
    ~CAPrivateInfo();
};

@interface CALayer (Internal)
// Xaml interop
- (instancetype)_initWithXamlElement:(WXFrameworkElement*)xamlElement;
@property (nonatomic, readonly, strong) WXFrameworkElement* _xamlElement;

- (NSObject*)presentationValueForKey:(NSString*)key;

- (int)_pixelWidth;
- (int)_pixelHeight;

- (void)_setContentColor:(CGColorRef)newColor;
- (void)_setOrigin:(CGPoint)origin;
- (void)_setShouldLayout;
- (void)setContentsOrientation:(UIImageOrientation)orientation;
- (UIImageOrientation)contentsOrientation;
- (DisplayNode*)_presentationNode;
- (void)_releaseContents:(BOOL)immediately;

// Some additional non-standard layer swapping functionality:
- (void)exchangeSublayer:(CALayer*)layer1 withLayer:(CALayer*)layer2;
- (void)exchangeSubviewAtIndex:(int)index1 withSubviewAtIndex:(int)index2;
- (void)sendSublayerToBack:(CALayer*)sublayer;
- (void)bringSublayerToFront:(CALayer*)sublayer;

// Kicks off an update to the layer's layout and display hierarchy if needed
- (void)_displayChanged;

+ (CGPoint)convertPoint:(CGPoint)point fromLayer:(CALayer*)layer toLayer:(CALayer*)layer;

- (void)_removeAnimation:(CAAnimation*)animation;
- (DisplayTexture*)_getDisplayTexture;

- (CAPrivateInfo*)_priv;
- (void)_setRootLayer:(BOOL)isRootLayer;

- (void)_setZIndex:(int)zIndex;

@end

#endif /* _CALAYERPRIVATE_H_ */
