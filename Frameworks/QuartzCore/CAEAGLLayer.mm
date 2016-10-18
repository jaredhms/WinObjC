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

#include "Starboard.h"

#include "UWP/WindowsUIXamlControls.h"
#include "UWP/WindowsUIXamlMedia.h"
#include "QuartzCore/CALayer.h"
#include "CALayerInternal.h"
#include "UIKit/UIView.h"
#include "QuartzCore/CAEAGLLayer.h"
#include "CACompositor.h"
#include "CAEAGLLayerInternal.h"

#include <d3d11.h>
#include <d3d11_1.h>

#include <COMIncludes.h>
#include "Windows.ui.xaml.media.dxinterop.h"
#include <COMIncludes_End.h>

@implementation CAEAGLLayer {
    NSDictionary* _properties;
    StrongId<WXCSwapChainPanel> _swapChainPanel;
}

/**
 @Status Interoperable
*/
- (void)setDrawableProperties:(NSDictionary*)propertiesDict {
    if (![propertiesDict isEqualToDictionary:_properties]) {
        [_properties release];
        _properties = [propertiesDict copy];

        [self setNeedsLayout];
    }
}

/**
 @Status Interoperable
*/
- (instancetype)init {
    if (self = [super init]) {
        // Create our swapchain panel
        _swapChainPanel.attach([WXCSwapChainPanel make]);

        // Create a sublayer for the swapchain panel
        StrongId<CALayer> sublayer;
        sublayer.attach([[CALayer alloc] _initWithXamlElement:_swapChainPanel]);
        [self addSublayer:sublayer];
    }

    return self;
}

/**
 @Status Caveat
 @Notes WinObjC extension
*/
- (WXCSwapChainPanel*)swapChainPanel {
    return _swapChainPanel;
}

- (DisplayTexture*)_getDisplayTexture {
    return NULL;
}

/**
   @Status Caveat
   @Notes kEAGLDrawablePropertyRetainedBacking, while implemented here, depends on ANGLE code that is not implemented.
*/
- (NSDictionary*)drawableProperties {
    return _properties;
}

- (void)display {
}

- (void)_releaseContents {
}

- (void)_unlockTexture {
}

- (void)dealloc {
    [_properties release];
    [super dealloc];
}
@end
