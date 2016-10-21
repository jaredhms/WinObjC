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

#include "CACompositor.h"
#include "winobjc\winobjc.h"

///////////////////////////////////////////////////////////////////////
// TODO: Will be DisplayTexture.h if we move it to its own file too
#include "CompositorInterface.h"
///////////////////////////////////////////////////////////////////////

#include <memory>
#include <set>

// A LayerProxy is CALayer's proxy to its backing Xaml FrameworkElement.  
// LayerProxy is used to update the Xaml FrameworkElement's visual state (positioning, animations, etc.) based upon CALayer API calls,
// and it's also responsible for the CALayer's sublayer management (*if* that backing Xaml FrameworkElement supports sublayers).
// When constructed, the LayerProxy inspects the passed-in Xaml FrameworkElement, and lights up all CALayer functionality that's
// supported by the given FrameworkElement.
class LayerProxy : public ILayerProxy, public std::enable_shared_from_this<LayerProxy> {
public:
    explicit LayerProxy(IInspectable* xamlElement);
    ~LayerProxy();

    // ILayerProxy
    Microsoft::WRL::ComPtr<IInspectable> GetXamlElement() override;
    void* GetPropertyValue(const char* propertyName) override;
    void SetShouldRasterize(bool shouldRasterize) override;

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // TODO: Can we remove this altogether and just set the z-index on the backing xaml element?
    void SetTopMost() override;
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Display properties
    void SetProperty(const wchar_t* name, float value);
    void SetPropertyInt(const wchar_t* name, int value);
    void SetHidden(bool hidden);
    void SetMasksToBounds(bool masksToBounds);
    void SetBackgroundColor(float r, float g, float b, float a);
    void SetTexture(const std::shared_ptr<DisplayTexture>& texture, float width, float height, float scale);
    void SetContents(const Microsoft::WRL::ComPtr<IInspectable>& bitmap, float width, float height, float scale);
    void SetContentsCenter(float x, float y, float width, float height);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // TODO: SHOULD REMOVE AND JUST DO ON UIWINDOW'S CANVAS
    void SetNodeZIndex(int zIndex);
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // General property management
    void UpdateProperty(const char* name, void* value);

    // Sublayer management
    void AddToRoot();
    void AddSubnode(const std::shared_ptr<LayerProxy>& subNode,
                    const std::shared_ptr<LayerProxy>& before,
                    const std::shared_ptr<LayerProxy>& after);
    void MoveNode(const std::shared_ptr<LayerProxy>& before, const std::shared_ptr<LayerProxy>& after);
    void RemoveFromSupernode();

protected:
    // Property management
    float _GetPresentationPropertyValue(const char* name);

    bool _isRoot;
    // TODO: weak_ptr or reference??
    LayerProxy* _parent;
    std::set<std::shared_ptr<LayerProxy>> _subnodes;
    std::shared_ptr<DisplayTexture> _currentTexture;
    bool _topMost;

private:
    // The Xaml element that backs this LayerProxy
    Microsoft::WRL::ComPtr<IInspectable> _xamlElement;
};