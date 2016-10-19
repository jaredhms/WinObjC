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
#include <deque>
#include <map>
#include <set>

#include "CACompositor.h"
#include "winobjc\winobjc.h"
#include <ppltasks.h>

class DisplayTexture;
class DisplayAnimation;

typedef enum {
    CompositionModeDefault = 0,
    CompositionModeLibrary = 1,
} CompositionMode;

void CreateXamlCompositor(winobjc::Id& root, CompositionMode compositionMode);

class DisplayAnimation : public std::enable_shared_from_this<DisplayAnimation> {
public:
    winobjc::Id _xamlAnimation;
    enum Easing { EaseInEaseOut, EaseIn, EaseOut, Linear, Default };

    double beginTime;
    double duration;
    bool autoReverse;
    float repeatCount;
    float repeatDuration;
    float speed;
    double timeOffset;
    Easing easingFunction;

    DisplayAnimation();
    virtual ~DisplayAnimation(){};

    virtual void Completed() = 0;
    // TODO: CAN WE DO const ILayerProxy&????
    virtual concurrency::task<void> AddToNode(ILayerProxy& node) = 0;

    void CreateXamlAnimation();
    void Start();
    void Stop();

    // TODO: CAN WE DO const ILayerProxy&????
    concurrency::task<void> AddAnimation(
        ILayerProxy& node, const wchar_t* propertyName, bool fromValid, float from, bool toValid, float to);
    // TODO: CAN WE DO const ILayerProxy&????
    concurrency::task<void> AddTransitionAnimation(ILayerProxy& node, const char* type, const char* subtype);
};

class DisplayTexture {
public:
    virtual ~DisplayTexture(){};
    virtual const Microsoft::WRL::ComPtr<IInspectable>& GetContent() const = 0;

protected:
    Microsoft::WRL::ComPtr<IInspectable> _xamlImage;
};

class CAXamlCompositor;

struct ICompositorTransaction {
public:
    virtual ~ICompositorTransaction() {
    }
    virtual void Process() = 0;
};

struct ICompositorAnimationTransaction {
public:
    virtual ~ICompositorAnimationTransaction() {
    }
    virtual concurrency::task<void> Process() = 0;
};

// Dispatches the compositor transactions that have been queued up
void DispatchCompositorTransactions(
    std::deque<std::shared_ptr<ICompositorTransaction>>&& subTransactions,
    std::deque<std::shared_ptr<ICompositorAnimationTransaction>>&& animationTransactions,
    std::map<std::shared_ptr<ILayerProxy>, std::map<std::string, std::shared_ptr<ICompositorTransaction>>>&& propertyTransactions,
    std::deque<std::shared_ptr<ICompositorTransaction>>&& movementTransactions);
