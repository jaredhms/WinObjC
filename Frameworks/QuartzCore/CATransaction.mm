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

#import <StubReturn.h>
#import "Starboard.h"
#import "Foundation/NSString.h"
#import "Foundation/NSThread.h"
#import "QuartzCore/CATransaction.h"
#import "QuartzCore/CABasicAnimation.h"
#import "CALayerInternal.h"
#import "LoggingNative.h"

#import "CACompositor.h"

static const wchar_t* TAG = L"CATransaction";

NSString* const kCATransactionAnimationDuration = @"kCATransactionAnimationDuration";
NSString* const kCATransactionDisableActions = @"kCATransactionDisableActions";
NSString* const kCATransactionAnimationTimingFunction = @"kCATransactionAnimationTimingFunction";
NSString* const kCATransactionCompletionBlock = @"kCATransactionCompletionBlock";

__declspec(thread) CATransaction* _curTransaction, *_rootTransaction;

@implementation CATransaction {
    CATransaction* _parent;
    std::shared_ptr<ILayerTransaction> _transactionQueue;

    BOOL _disableActions;
    double _duration;
    id _timingFunction;

    id _completionBlock;
}

+ (CATransaction*)_currentTransaction {
    if (_curTransaction == NULL) {
        [self begin];
    }

    return _curTransaction;
}

/**
 @Status Interoperable
*/
- (instancetype)init {
    if (self = [super init]) {
        _disableActions = FALSE;
        _duration = 0.25f;

        _transactionQueue = GetCACompositor()->CreateLayerTransaction();

        if (_curTransaction != NULL) {
            _parent = _curTransaction;
        } else {
            _parent = _rootTransaction;
        }
    }
    return self;
}

/**
 @Status Interoperable
*/
+ (void)begin {
    if (_rootTransaction == NULL) {
        _rootTransaction = [CATransaction new];
    }

    CATransaction* ret = [CATransaction new];
    ret->_duration = ret->_parent->_duration;
    _curTransaction = ret;
}

/**
 @Status Interoperable
*/
+ (void)commit {
    if (_curTransaction != NULL) {
        CATransaction* rel = _curTransaction;
        GetCACompositor()->QueueLayerTransaction(_curTransaction->_transactionQueue, _curTransaction->_parent->_transactionQueue);
        if (_curTransaction->_parent != _rootTransaction) {
            _curTransaction = _curTransaction->_parent;
        } else {
            _curTransaction = NULL;
        }

        [rel release];
    }
}

+ (void)_commitAndProcessRootQueue {
    while (_curTransaction) {
        [self commit];
    }
    
    if (_rootTransaction != NULL) {
        GetCACompositor()->QueueLayerTransaction(_rootTransaction->_transactionQueue, NULL);
        [_rootTransaction release];
        _rootTransaction = NULL;
    }

    GetCACompositor()->ProcessLayerTransactions();
}

/**
 @Status Stub
*/
+ (void)flush {
    UNIMPLEMENTED();
    TraceVerbose(TAG, L"CATransaction flush?");
}

/**
 @Status Interoperable
*/
+ (BOOL)disableActions {
    return [self _currentTransaction]->_disableActions;
}

/**
 @Status Interoperable
*/
+ (void)setDisableActions:(BOOL)disable {
    [self _currentTransaction]->_disableActions = disable;
}

/**
 @Status Stub
*/
+ (void)setCompletionBlock:(void (^)(void))block {
    UNIMPLEMENTED();
    FAIL_FAST_MSG("setCompletionBlock not implemented!");
    [self _currentTransaction]->_completionBlock = [block copy];
}

/**
 @Status Stub
*/
+ (void (^)(void))completionBlock {
    UNIMPLEMENTED();
    return [self _currentTransaction]->_completionBlock;
}

/**
 @Status Interoperable
*/
+ (void)setAnimationDuration:(double)duration {
    [self _currentTransaction]->_duration = duration;
}

/**
 @Status Interoperable
*/
+ (void)setAnimationTimingFunction:(id)timing {
    [self _currentTransaction]->_timingFunction = timing;
}

/**
 @Status Caveat
 @Notes Only kCATransactionDisableActions and kCATransactionAnimationDuration supported
*/
+ (void)setValue:(id)anObject forKey:(NSString*)keyName {
    if (strcmp([keyName UTF8String], "kCATransactionDisableActions") == 0) {
        [self setDisableActions:[anObject boolValue]];
    }
    if (strcmp([keyName UTF8String], "kCATransactionAnimationDuration") == 0) {
        [self setAnimationDuration:[anObject floatValue]];
    }
}

+ (CAAnimation*)_implicitAnimationForKey:(NSString*)forKey {
    if ([self _currentTransaction]->_disableActions) {
        return nil;
    }

    CABasicAnimation* propAnim = [CABasicAnimation animationWithKeyPath:forKey];

    [propAnim setDuration:[self _currentTransaction]->_duration];
    [propAnim setTimingFunction:[CAMediaTimingFunction functionWithName:@"kCAMediaTimingFunctionDefault"]];

    return propAnim;
}

+ (void)_setPropertyForLayer:(CALayer*)layer name:(NSString*)propertyName value:(NSObject*)newValue {
    [self _currentLayerTransaction]->SetLayerProperty(
                                          [layer _layerProxy],
                                          [propertyName UTF8String],
                                          newValue);
    [layer _displayChanged];
}

+ (std::shared_ptr<ILayerTransaction>)_currentLayerTransaction {
    return [self _currentTransaction]->_transactionQueue;
}

+ (void)_addSublayerToTop:(CALayer*)layer {
    [self _currentLayerTransaction]->AddLayer([layer _layerProxy], nullptr, nullptr, nullptr);
}

+ (void)_addSublayerToLayer:(CALayer*)layer sublayer:(CALayer*)sublayer {
    [self _currentLayerTransaction]->AddLayer(
                               [sublayer _layerProxy],
                               [layer _layerProxy],
                               nullptr,
                               nullptr);
}

+ (void)_addSublayerToLayer:(CALayer*)layer sublayer:(CALayer*)sublayer before:(CALayer*)before {
    [self _currentLayerTransaction]->AddLayer(
                               [sublayer _layerProxy],
                               [layer _layerProxy],
                               [before _layerProxy],
                               nullptr);
}

+ (void)_addSublayerToLayer:(CALayer*)layer sublayer:(CALayer*)sublayer after:(CALayer*)after {
    [self _currentLayerTransaction]->AddLayer(
                               [sublayer _layerProxy],
                               [layer _layerProxy],
                               nullptr,
                               [after _layerProxy]);
}

+ (void)_replaceInLayer:(CALayer*)layer sublayer:(CALayer*)sublayer withSublayer:(CALayer*)newlayer {
    [self _currentLayerTransaction]->AddLayer(
                               [newlayer _layerProxy],
                               [layer _layerProxy],
                               [sublayer _layerProxy],
                               NULL);
    [self _currentLayerTransaction]->RemoveLayer([sublayer _layerProxy]);
}

+ (void)_moveLayer:(CALayer*)layer beforeLayer:(CALayer*)before afterLayer:(CALayer*)after {
    [self _currentLayerTransaction]->MoveLayer(
                                [layer _layerProxy],
                                before ? [before _layerProxy] : nullptr,
                                after ? [after _layerProxy] : nullptr);
}

+ (void)_removeLayer:(CALayer*)layer {
    [self _currentLayerTransaction]->RemoveLayer([layer _layerProxy]);
}

+ (void)_addAnimationToLayer:(CALayer*)layer animation:(CAAnimation*)anim forKey:(NSString*)key {
    [self _currentLayerTransaction]->AddAnimation(layer, anim, key);
    [layer _displayChanged];
}

+ (void)_removeAnimationFromLayer:(CALayer*)layer animation:(std::shared_ptr<ILayerAnimation>)anim {
    [self _currentLayerTransaction]->RemoveAnimation(anim);
}

/**
 @Status Stub
 @Notes
*/
+ (CFTimeInterval)animationDuration {
    UNIMPLEMENTED();
    return StubReturn();
}

/**
 @Status Stub
 @Notes
*/
+ (CAMediaTimingFunction*)animationTimingFunction {
    UNIMPLEMENTED();
    return StubReturn();
}

/**
 @Status Stub
 @Notes
*/
+ (id)valueForKey:(NSString*)key {
    UNIMPLEMENTED();
    return StubReturn();
}

/**
 @Status Stub
 @Notes
*/
+ (void)lock {
    UNIMPLEMENTED();
}

/**
 @Status Stub
 @Notes
*/
+ (void)unlock {
    UNIMPLEMENTED();
}

@end
