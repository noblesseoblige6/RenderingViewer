#pragma once

using namespace Microsoft::WRL;
using namespace std;

class App
{
public:
    App( HWND hWnd, HINSTANCE hInst );
    ~App();

public: // Process
    bool Run();

    bool Initialize();
    void Terminate();

public: // Accessor
    void SetIsInitialized( const bool isInit ) { m_isInit = isInit; }
    bool IsInitialized() const { return m_isInit; }

protected:
    bool InitD3D12();
    bool InitApp();
    bool CreateDepthStencilBuffer();
    bool CreateScene();
    bool CreateRenderPass();


    bool TermD3D12();
    bool TermApp();

    void OnFrameRender();
    
    void RenderShadowPass();
    void RenderForwardPass();

    void UpdateGPUBuffers();

    void Present( unsigned int syncInterval );

    void WaitDrawCommandDone();
    void ResetFrame();

    void ProcessInput();

protected:
    HWND m_hWnd;
    HINSTANCE m_hInst;
    int  m_width;
    int  m_height;

    ComPtr<IDXGIAdapter> m_pAdapter;
    ComPtr<ID3D12Device> m_pDevice;
    ComPtr<ID3D12CommandQueue> m_pCommandQueue;
    shared_ptr<CommandList>            m_pCommandList;
    ComPtr<IDXGIFactory4> m_pFactory;

    ComPtr<ID3D12Fence> m_pFence;

    D3D12_VIEWPORT         m_viewport;
    D3D12_RECT             m_scissorRect;
    ComPtr<IDXGISwapChain3> m_pSwapChain;
    
    shared_ptr<DescriptorHeap>      m_pDescHeapForRT;
    shared_ptr<RenderTarget>        m_pRenderTargets[2];

    shared_ptr<DescriptorHeap>      m_pDescHeapForDS;
    shared_ptr<DepthStencilBuffer>  m_pDSBuffer;

    shared_ptr<Scene>           m_pScene;

    shared_ptr<Model>           m_pBunny;
    shared_ptr<Model>           m_pFloor;

    shared_ptr<Camera>          m_pCamera;
    shared_ptr<Light>           m_pLight;

    shared_ptr<RenderPassClear>               m_pRenderPassClear;
    shared_ptr<RenderPassClear>               m_pRenderPassClearShadow;
    shared_ptr<RenderPassForward>             m_pRenderPassForward;
    shared_ptr<RenderPassShadow>              m_pRenderPassShadow;

    Vec2i                             m_shadowSize;
    shared_ptr<DepthStencilBuffer>    m_pShadowMap;


    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;

    UINT m_swapChainCount;

    bool m_isInit;

    bool m_bUpdateCB;

    acObjLoader m_loader;
    unique_ptr<InputManager> m_inputManager;
};

