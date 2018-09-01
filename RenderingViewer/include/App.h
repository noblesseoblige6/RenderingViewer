#pragma once

using namespace Microsoft::WRL;
using namespace std;

struct ResConstantBuffer
{
    Mat44f world;
    Mat44f view;
    Mat44f projection;

    DWORD size;
    BYTE  reserved[60];
};

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

    bool TermD3D12();
    bool TermApp();

    bool SearchFilePath(const wchar_t * filePath, std::wstring& result);

    void SetResourceBarrier( ID3D12GraphicsCommandList* pCmdList,
                             ID3D12Resource* pResource,
                             D3D12_RESOURCE_STATES stateBefore,
                             D3D12_RESOURCE_STATES stateAfter );

    void OnFrameRender();

    void Present( unsigned int syncInterval );

    void BeginDrawCommand();
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
    ComPtr<ID3D12CommandAllocator> m_pCommandAllocator;
    ComPtr<ID3D12CommandQueue> m_pCommandQueue;
    ComPtr<IDXGIFactory4> m_pFactory;
    ComPtr<ID3D12GraphicsCommandList> m_pCommandList;
    ComPtr<ID3D12Fence> m_pFence;

    D3D12_VIEWPORT         m_viewport;
    D3D12_RECT             m_scissorRect;
    ComPtr<IDXGISwapChain3> m_pSwapChain;
    
    ComPtr<ID3D12RootSignature>        m_pRootSignature;
    ComPtr<ID3D12PipelineState>        m_pPipelineState;
    ComPtr<ID3D12DescriptorHeap>    m_pDescHeapRenderTarget;
    UINT                            m_DescHeapRenderTargetSize;
    
    ComPtr<ID3D12Resource>          m_pRenderTarget[2];
    D3D12_CPU_DESCRIPTOR_HANDLE     m_renderTargetHandle[2];

    ComPtr<ID3D12DescriptorHeap>    m_pDescHeapDepthStencil;
    UINT                            m_DescHeapDepthStencilSize;
    ComPtr<ID3D12Resource>          m_pDepthStencil;

    ComPtr<ID3D12Resource>          m_pVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW        m_vertexBufferView;

    ComPtr<ID3D12Resource>          m_pIndexBuffer;
    D3D12_INDEX_BUFFER_VIEW         m_indexBufferView;

    ComPtr<ID3D12Resource>          m_pConstantBuffer;
    ComPtr<ID3D12DescriptorHeap>    m_pDescHeapConstant;
    ResConstantBuffer               m_constantBufferData;
    UINT8*                          m_pCbvDataBegin;

    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;

    UINT m_swapChainCount;

    bool m_isInit;

    acObjLoader m_loader;
    unique_ptr<InputManager> m_inputManager;
};

