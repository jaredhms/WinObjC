//******************************************************************************
//
// Copyright (c) Microsoft Corporation. All rights reserved.
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

#import <cmath>
#import <UIKit/UIScreen.h>
#import "_UIPopupViewController.h"
#import <UWP/WindowsUIXamlControlsPrimitives.h>

static const wchar_t* TAG = L"_UIPopupViewController";

// Compare UI element offsets
static bool offsetsPrettyClose(double off1, double off2) {
    // Avoid comparing for exact equality
    return std::fabs(off1 - off2) < 1.0;
};

@implementation _UIPopupViewController {
    WUXCPPopup* _popup;
    EventRegistrationToken _layoutUpdated;
}

- (void)loadView {
    _popup = [WUXCPPopup make];
    UIView* view = [[UIView alloc] initWithFrame:CGRectZero xamlElement:_popup];

    _UIPopupViewController __weak* weakSelf = self;

    // Keep the popup centered in the window
    _layoutUpdated = [_popup addLayoutUpdatedEvent:^(RTObject* sender, RTObject* args) {
        _UIPopupViewController* strongSelf = weakSelf;

        if (strongSelf == nil) {
            return;
        }

        WUXCPPopup* popup = strongSelf->_popup;
        id popupChild = popup.child;

        if (popupChild != nil) {
            WXFrameworkElement* child = rt_dynamic_cast<WXFrameworkElement>(popupChild);

            WFRect* appFrame = [[WXWindow current] bounds];

            double desiredHorizontalOffset = (appFrame.width - child.width) / 2.0;
            double desiredVerticalOffset = (appFrame.height - child.height) / 2.0;

            if (!offsetsPrettyClose(desiredHorizontalOffset, popup.horizontalOffset)) {
                popup.horizontalOffset = desiredHorizontalOffset;
            }

            if (!offsetsPrettyClose(desiredVerticalOffset, popup.verticalOffset)) {
                popup.verticalOffset = desiredVerticalOffset;
            }
        }
    }];

    self.view = view;
}

- (void)dealloc {
    [_popup removeLayoutUpdatedEvent:_layoutUpdated];
}

- (void)presentViewController:(UIViewController*)viewControllerToPresent animated:(BOOL)flag completion:(void (^)())completion {
    _popup.child = viewControllerToPresent.view.xamlElement;
    _popup.isOpen = YES;
    [super presentViewController:viewControllerToPresent animated:flag completion:completion];
}

- (void)dismissViewControllerAnimated:(BOOL)flag completion:(void (^)())completion {
    _popup.isOpen = NO;
    _popup.child = nil;
    [super dismissViewControllerAnimated:flag completion:completion];
}

@end
