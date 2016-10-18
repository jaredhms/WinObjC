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

class DisplayNode;
class DisplayTexture;
class DisplayAnimation;

typedef enum {
    CompositionModeDefault = 0,
    CompositionModeLibrary = 1,
} CompositionMode;

void CreateXamlCompositor(winobjc::Id& root, CompositionMode compositionMode);

template <class T>
class RefCounted;

class RefCountedType {
    template <class T>
    friend class RefCounted;
    int refCount;

public:
    void AddRef();
    void Release();

protected:
    RefCountedType();
    virtual ~RefCountedType();
};

template <class T>
class RefCounted {
private:
    T* _ptr;

public:
    RefCounted() {
        _ptr = NULL;
    }
    RefCounted(T* ptr) {
        _ptr = ptr;
        if (_ptr)
            _ptr->AddRef();
    }

    RefCounted(const RefCounted& copy) {
        _ptr = copy._ptr;
        if (_ptr)
            _ptr->AddRef();
    }

    ~RefCounted() {
        if (_ptr)
            _ptr->Release();
        _ptr = NULL;
    }
    RefCounted& operator=(const RefCounted& val) {
        if (_ptr == val._ptr)
            return *this;
        T* oldPtr = _ptr;
        _ptr = val._ptr;
        if (_ptr)
            _ptr->AddRef();
        if (oldPtr)
            oldPtr->Release();

        return *this;
    }

    T* operator->() {
        return _ptr;
    }

    T* Get() {
        return _ptr;
    }

    operator bool() {
        return _ptr != NULL;
    }

    bool operator<(const RefCounted& other) const {
        return _ptr < other._ptr;
    }
};

typedef RefCounted<DisplayTexture> DisplayTextureRef;
typedef RefCounted<DisplayAnimation> DisplayAnimationRef;

class DisplayAnimation : public RefCountedType {
    friend class CAXamlCompositor;

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
    virtual ~DisplayAnimation() {};

    virtual void Completed() = 0;
    // TODO: CAN WE DO const DisplayNode&????
    virtual concurrency::task<void> AddToNode(DisplayNode& node) = 0;

    void CreateXamlAnimation();
    void Start();
    void Stop();

    // TODO: CAN WE DO const DisplayNode&????
    concurrency::task<void> AddAnimation(
        DisplayNode& node, 
        const wchar_t* propertyName, 
        bool fromValid, 
        float from, 
        bool toValid, 
        float to);
    // TODO: CAN WE DO const DisplayNode&????
    concurrency::task<void> AddTransitionAnimation(
        DisplayNode& node, 
        const char* type, 
        const char* subtype);
};

class DisplayTexture : public RefCountedType {
public:
    virtual ~DisplayTexture(){};
    virtual const Microsoft::WRL::ComPtr<IInspectable>& GetContent() const = 0;

protected:
    Microsoft::WRL::ComPtr<IInspectable> _xamlImage;
};


class CAXamlCompositor;

// A DisplayNode is CALayer's proxy to its backing Xaml FrameworkElement.  DisplayNodes
// are used to update the Xaml FrameworkElement's visual state (positioning, animations, etc.) based upon CALayer API calls, 
// and it's also responsible for the CALayer's sublayer management (*if* that backing Xaml FrameworkElement supports sublayers).
// When constructed, the DisplayNode inspects the passed-in Xaml FrameworkElement, and lights up all CALayer functionality that's
// supported by the given FrameworkElement.
class DisplayNode : public std::enable_shared_from_this<DisplayNode> {
public:
    // The Xaml element that backs this DisplayNode
    Microsoft::WRL::ComPtr<IInspectable> _xamlElement;

    explicit DisplayNode(IInspectable* xamlElement);
    ~DisplayNode();

    // Animation support
    concurrency::task<void> AddAnimation(DisplayAnimation* animation);

    // Display properties
    void SetProperty(const wchar_t* name, float value);
    void SetPropertyInt(const wchar_t* name, int value);
    void SetHidden(bool hidden);
    void SetMasksToBounds(bool masksToBounds);
    void SetBackgroundColor(float r, float g, float b, float a);
    void SetTexture(DisplayTexture* texture, float width, float height, float scale);
    void SetContents(const Microsoft::WRL::ComPtr<IInspectable>& bitmap, float width, float height, float scale);
    void SetContentsCenter(float x, float y, float width, float height);
    void SetShouldRasterize(bool shouldRasterize);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // TODO: SHOULD REMOVE AND JUST DO ON UIWINDOW'S CANVAS
    void SetNodeZIndex(int zIndex);
    ///////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // TODO: CAN/SHOULD WE REMOVE THIS?
    void SetTopMost();
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // General property management
    void* GetProperty(const char* name);
    void UpdateProperty(const char* name, void* value);

    // Sublayer management
    void AddToRoot();
    void AddSubnode(const std::shared_ptr<DisplayNode>& subNode, const std::shared_ptr<DisplayNode>& before, const std::shared_ptr<DisplayNode>& after);
    void MoveNode(const std::shared_ptr<DisplayNode>& before, const std::shared_ptr<DisplayNode>& after);
    void RemoveFromSupernode();

protected:
    // Property management
    float _GetPresentationPropertyValue(const char* name);

    bool _isRoot;
    DisplayNode* _parent;
    std::set<std::shared_ptr<DisplayNode>> _subnodes;
    DisplayTextureRef _currentTexture;
    bool _topMost;
};

struct ICompositorTransaction {
public:
    virtual ~ICompositorTransaction() {}
    virtual void Process() = 0;
};

struct ICompositorAnimationTransaction {
public:
    virtual ~ICompositorAnimationTransaction() {}
    virtual concurrency::task<void> Process() = 0;
};

// Dispatches the compositor transactions that have been queued up
void DispatchCompositorTransactions(
    std::deque<std::shared_ptr<ICompositorTransaction>>&& subTransactions,
    std::deque<std::shared_ptr<ICompositorAnimationTransaction>>&& animationTransactions,
    std::map<std::shared_ptr<DisplayNode>, std::map<std::string, std::shared_ptr<ICompositorTransaction>>>&& propertyTransactions,
    std::deque<std::shared_ptr<ICompositorTransaction>>&& movementTransactions);
