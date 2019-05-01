#pragma once

using namespace Microsoft::WRL;
using namespace std;

#define Aligned(x) (sizeof(x) + 255) &~255

struct ResConstantBuffer
{
    Mat44f world;
    Mat44f view;
    Mat44f projection;

    DWORD size;
};

struct ResMaterialData
{
    Vec4f ka;
    Vec4f kd;
    Vec4f ks; // Alpha: Shininess

    DWORD size;
};

struct ResLightData
{
    // Directinal Light
    static const int DIRECTIONAL_LIGHT_NUM = 1;
    Vec4f position[DIRECTIONAL_LIGHT_NUM];
    Vec4f color[DIRECTIONAL_LIGHT_NUM];

    Mat44f view[DIRECTIONAL_LIGHT_NUM];
    Mat44f projection[DIRECTIONAL_LIGHT_NUM];

    Vec3f direction[DIRECTIONAL_LIGHT_NUM];
    Vec3f intensity[DIRECTIONAL_LIGHT_NUM];
    //Mat44f lightVP[DIRECTIONAL_LIGHT_NUM];
    char padding[68];
    DWORD size;
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
    bool CreateRootSignature();
    bool CreatePipelineState();
    bool CreatePipelineStateForShadow();
    bool CreateGeometry();
    bool CreateDepthStencilBuffer();
    bool CreateCB();
    bool CreateCBForShadow();


    bool TermD3D12();
    bool TermApp();

    bool CompileShader( const std::wstring& file, ComPtr<ID3DBlob>& pVSBlob, ComPtr<ID3DBlob>& pPSBlob );
    bool SearchFilePath(const std::wstring& filePath, std::wstring& result);

    void OnFrameRender();
    
    void RenderShadowPass();
    void RenderForwardPass();

    void UpdateGPUBuffers();

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
    ComPtr<ID3D12CommandQueue> m_pCommandQueue;
    shared_ptr<CommandList>            m_pCommandList;
    ComPtr<IDXGIFactory4> m_pFactory;

    ComPtr<ID3D12Fence> m_pFence;

    D3D12_VIEWPORT         m_viewport;
    D3D12_RECT             m_scissorRect;
    ComPtr<IDXGISwapChain3> m_pSwapChain;
    
    shared_ptr<RootSignature>          m_pRootSignature;
    shared_ptr<PipelineState>          m_pPipelineState;

    shared_ptr<DescriptorHeap>      m_pDescHeapForRT;
    shared_ptr<RenderTarget>        m_pRenderTargets[2];

    shared_ptr<DescriptorHeap>      m_pDescHeapForDS;
    shared_ptr<DepthStencilBuffer>  m_pDSBuffer;

    shared_ptr<Model>           m_model;

    shared_ptr<DescriptorHeap>    m_pDescHeapForCB;
    shared_ptr<ConstantBuffer>    m_pCameraCB;
    shared_ptr<ConstantBuffer>    m_pLightCB;
    shared_ptr<ConstantBuffer>    m_pMaterialCB;

    ResConstantBuffer               m_constantBufferData;
    ResLightData                    m_lightData;
    ResMaterialData                 m_materialData;

    shared_ptr<RenderInfoShadow>       m_pRenderInfoShadow;

    Vec2i                             m_shadowSize;
    shared_ptr<DepthStencilBuffer>    m_pShadowMap;


    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;

    UINT m_swapChainCount;

    bool m_isInit;

    Mat44f m_viewMatrix;
    Mat44f m_projectionMatrix;

    Mat33f m_cameraPoseMatrix;
    Vec3f  m_cameraPosition;
    Vec3f  m_cameraLookAt;

    bool m_bUpdateCB;

    acObjLoader m_loader;
    unique_ptr<InputManager> m_inputManager;
};

