#include "App.h"

App::App( HWND hWnd, HINSTANCE hInst )
    : m_isInit( false )
{
    m_hWnd = hWnd;
    m_hInst = hInst;

    Initialize();
}

App::~App()
{
    Terminate();
}

bool App::Run()
{
    if (!IsInitialized())
    {
        cerr << "Failed to run app" << endl;

        return false;
    }

    OnFrameRender();

    ProcessInput();

    return true;
}


bool App::Initialize()
{
    if (!InitD3D12())
    {
        cerr << "Failed to initialize D3D12" << endl;
        return false;
    }

    if (!InitApp())
    {
        cerr << "Failed to initialize App" << endl;
        return false;
    }

    m_inputManager = unique_ptr<InputManager>( new InputManager( m_hWnd, m_hInst ) );

    SetIsInitialized( true );

    return true;
}

void App::Terminate()
{
    TermD3D12();

    // COMライブラリの終了処理
    CoUninitialize();

    if(m_inputManager != nullptr)
        m_inputManager->Release();
}

bool App::InitD3D12()
{
    HRESULT hr = S_OK;

    // ウィンドウ幅を取得.
    RECT rc;
    GetClientRect( m_hWnd, &rc );
    m_width = rc.right - rc.left;
    m_height = rc.bottom - rc.top;

    UINT flags = 0;

#if defined(DEBUG) || defined(_DEBUG)
    ID3D12Debug* pDebug;
    D3D12GetDebugInterface( IID_ID3D12Debug, (void**)&pDebug );
    if (pDebug)
    {
        pDebug->EnableDebugLayer();
        pDebug->Release();
    }
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    hr = CreateDXGIFactory2( flags, IID_IDXGIFactory4, (void**)(m_pFactory.ReleaseAndGetAddressOf()) );
    if (FAILED( hr ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "CreateDXGIFactory() Failed.");
        return false;
    }

    hr = m_pFactory->EnumAdapters( 0, m_pAdapter.GetAddressOf() );
    if (FAILED( hr ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "IDXGIFactory::EnumAdapters() Failed.");
        return false;
    }

    // デバイス生成.
    hr = D3D12CreateDevice( m_pAdapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_ID3D12Device,
        (void**)(m_pDevice.ReleaseAndGetAddressOf()) );

    // 生成チェック.
    if (FAILED( hr ))
    {
        // Warpアダプターで再トライ.
        m_pAdapter.Reset();

        hr = m_pFactory->EnumWarpAdapter( IID_PPV_ARGS( m_pAdapter.ReleaseAndGetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "IDXGIFactory::EnumWarpAdapter() Failed.");
            return false;
        }

        // デバイス生成.
        hr = D3D12CreateDevice( m_pAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_ID3D12Device,
            (void**)(m_pDevice.ReleaseAndGetAddressOf()) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "D3D12CreateDevice() Failed.");
            return false;
        }
    }

    // create command queue
    {
        D3D12_COMMAND_QUEUE_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );

        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority = 0;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        hr = m_pDevice->CreateCommandQueue( &desc,
                                            IID_ID3D12CommandQueue,
                                            (void**)(m_pCommandQueue.
                                            ReleaseAndGetAddressOf()) );
        if (FAILED( hr ))
        {
            return false;
        }
    }

    // create command list
    m_pCommandList = make_shared<CommandList>( m_pDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT );

    // create swap chain
    {
        hr = CreateDXGIFactory( IID_IDXGIFactory,
                                (void**)m_pFactory.ReleaseAndGetAddressOf() );
        if (FAILED( hr ))
        {
            return false;
        }

        DXGI_SWAP_CHAIN_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );

        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferDesc.Width = m_width;
        desc.BufferDesc.Height = m_height;
        desc.BufferDesc.RefreshRate.Numerator = 60;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
        desc.OutputWindow = m_hWnd;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Windowed = true;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        ComPtr<IDXGISwapChain> pSwapChain;
        hr = m_pFactory->CreateSwapChain( m_pCommandQueue.Get(), &desc, pSwapChain.ReleaseAndGetAddressOf() );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "CreateSwapChain() Failed.");
            return false;
        }

        hr = pSwapChain->QueryInterface( IID_PPV_ARGS(m_pSwapChain.ReleaseAndGetAddressOf()));
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "QueryInterface() Failed.");
            return false;
        }

        m_swapChainCount = 0;
    }

    // create descripter heap for render target
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc;
        ZeroMemory( &desc, sizeof( desc ) );

        desc.NumDescriptors = 2;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        m_pDescHeapForRT = make_shared<DescriptorHeap>();
        m_pDescHeapForRT->Create( m_pDevice.Get(), desc );
    }

    // create render targets from back buffers
    {
        for (int i = 0; i < 2; ++i)
        {
            m_pRenderTargets[i] = make_shared<RenderTarget>();

            // Resources come from back buffers
            hr = m_pSwapChain->GetBuffer( i, IID_ID3D12Resource, (void**)m_pRenderTargets[i]->GetBufferAddressOf() );
            if (FAILED( hr ))
            {
                return false;
            }

            const D3D12_RESOURCE_DESC& desc = m_pRenderTargets[i]->GetBuffer()->GetDesc();
            m_pRenderTargets[i]->CreateBufferView( m_pDevice.Get(), desc, m_pDescHeapForRT, Buffer::BUFFER_VIEW_TYPE_RENDER_TARGET );
        }
    }

    // create depth stencil buffer
    if (!CreateDepthStencilBuffer())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreateDepthStencilBuffer() Failed." );
        return false;
    }

    // create fence
    m_fenceEvent = CreateEvent( 0, false, false, 0 );

    hr = m_pDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, (void**)m_pFence.ReleaseAndGetAddressOf() );
    if (FAILED( hr ))
    {
        return false;
    }

    // viewport config
    {
        m_viewport.TopLeftX = 0;
        m_viewport.TopLeftY = 0;
        m_viewport.Width = static_cast<float>(m_width);
        m_viewport.Height = static_cast<float>(m_height);
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;
    }

    {
        m_scissorRect.left   = 0;
        m_scissorRect.top    = 0;
        m_scissorRect.right  = static_cast<long>(m_width);
        m_scissorRect.bottom = static_cast<long>(m_height);
    }

    return true;
}

bool App::InitApp()
{
    if (!CreateCB())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreateCB() Failed." );
        return false;
    }

    if (!CreateGeometry())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreateGeometry() Failed." );
        return false;
    }

    if (!CreateRenderPass())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreateRenderPass() Failed." );
        return false;
    }

    return true;
}

bool App::CreateGeometry()
{
    m_pBunny = make_shared<Model>();
    m_pBunny->BindAsset(m_pDevice.Get(), "resource/bunny.obj" );

    m_pFloor = make_shared<Model>();
    m_pFloor->BindAsset( m_pDevice.Get(), "resource/floor.obj" );

    return true;
}

bool App::CreateRenderPass()
{
    m_pRenderPassClear = make_shared<RenderPassClear>( m_pDevice.Get() );
    m_pRenderPassClear->BindResources( m_pDevice.Get() );

    m_pRenderPassForward = make_shared<RenderPassForward>( m_pDevice.Get() );

    m_pRenderPassForward->AddModel( m_pBunny );
    m_pRenderPassForward->AddModel( m_pFloor );

    m_pRenderPassForward->AddResource( Buffer::BUFFER_VIEW_TYPE_CONSTANT, m_pCameraCB );
    m_pRenderPassForward->AddResource( Buffer::BUFFER_VIEW_TYPE_CONSTANT, m_pLightCB );
    m_pRenderPassForward->AddResource( Buffer::BUFFER_VIEW_TYPE_CONSTANT, m_pMaterialCB );
    m_pRenderPassForward->AddResource( Buffer::BUFFER_VIEW_TYPE_SHADER_RESOURCE, m_pShadowMap );

    m_pRenderPassForward->BindResources(m_pDevice.Get());

    m_pRenderPassShadow = make_shared<RenderPassShadow>( m_pDevice.Get() );

    m_pRenderPassShadow->AddModel( m_pBunny );
    m_pRenderPassShadow->AddModel( m_pFloor );
    
    m_pRenderPassShadow->AddResource( Buffer::BUFFER_VIEW_TYPE_CONSTANT     , m_pLightCB );

    m_pRenderPassShadow->BindResources( m_pDevice.Get() );

    return true;
}

bool App::CreateDepthStencilBuffer()
{
    // create descriptor heap for depth buffer
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 2;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

        m_pDescHeapForDS = make_shared<DescriptorHeap>();
        m_pDescHeapForDS->Create( m_pDevice.Get(), desc );
    }

    // create constant buffer
    {
        // ヒーププロパティの設定.
        D3D12_HEAP_PROPERTIES prop = {};
        prop.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        prop.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask     = 0;
        prop.VisibleNodeMask      = 0;

        // リソースの設定.
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment          = 0;
        desc.Width              = m_width;
        desc.Height             = m_height;
        desc.DepthOrArraySize   = 1;
        desc.MipLevels          = 1;
        desc.Format             = DXGI_FORMAT_R32_TYPELESS;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearVal = {};
        clearVal.Format               = DXGI_FORMAT_D32_FLOAT;
        clearVal.DepthStencil.Depth   = 1.0f;
        clearVal.DepthStencil.Stencil = 0;

        m_pDSBuffer = make_shared<DepthStencilBuffer>();

        m_pDSBuffer->Create( m_pDevice.Get(), prop, desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal );
        m_pDSBuffer->CreateBufferView( m_pDevice.Get(), desc, m_pDescHeapForDS, Buffer::BUFFER_VIEW_TYPE_DEPTH_STENCIL);
    }

        // create constant buffer
    {
        // ヒーププロパティの設定.
        D3D12_HEAP_PROPERTIES prop = {};
        prop.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        prop.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask     = 0;
        prop.VisibleNodeMask      = 0;

        m_shadowSize.x = 2048;
        m_shadowSize.y = 2048;
        
        // リソースの設定.
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment          = 0;
        desc.Width              = m_shadowSize.x;
        desc.Height             = m_shadowSize.y;
        desc.DepthOrArraySize   = 1;
        desc.MipLevels          = 1;
        desc.Format             = DXGI_FORMAT_R32_TYPELESS;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearVal = {};
        clearVal.Format               = DXGI_FORMAT_D32_FLOAT;
        clearVal.DepthStencil.Depth   = 1.0f;
        clearVal.DepthStencil.Stencil = 0;

        m_pShadowMap = make_shared<DepthStencilBuffer>();
        m_pShadowMap->Create( m_pDevice.Get(), prop, desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearVal );
        m_pShadowMap->CreateBufferView( m_pDevice.Get(), desc, m_pDescHeapForDS, Buffer::BUFFER_VIEW_TYPE_DEPTH_STENCIL );
    }

    return true;
}

bool App::CreateCB()
{
    // ヒーププロパティの設定.
    D3D12_HEAP_PROPERTIES prop = {};
    prop.Type = D3D12_HEAP_TYPE_UPLOAD;
    prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    prop.CreationNodeMask = 1;
    prop.VisibleNodeMask = 1;

    // リソースの設定.
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // create CB for camera
    {
        m_pCameraCB = make_shared<ConstantBuffer>();

        desc.Width = Aligned( ResConstantBuffer );

        m_pCameraCB->Create( m_pDevice.Get(), prop, desc, D3D12_RESOURCE_STATE_GENERIC_READ );

        // アスペクト比算出.
        auto aspectRatio = static_cast<FLOAT>(m_width / m_height);

        m_cameraPosition = Vec3f( 0.0f, 0.0f, 5.0f );
        m_cameraLookAt = Vec3f::ZERO;

        m_viewMatrix = Mat44f::CreateLookAt( m_cameraPosition, m_cameraLookAt, Vec3f::YAXIS );
        m_projectionMatrix = Mat44f::CreatePerspectiveFieldOfViewLH( static_cast<float>(DEG2RAD( 50 )), aspectRatio, 1.0f, 100.0f );

        // Find camera pose matrix from view matrix
        Mat44f transMat( Mat44f::IDENTITY );
        transMat.Translate( m_cameraPosition );

        Mat44f poseMat = m_viewMatrix.Inverse() * transMat.Inverse();
        m_cameraPoseMatrix = poseMat.GetScaleAndRoation();

        // 定数バッファデータの設定.
        m_constantBufferData.size       = sizeof( ResConstantBuffer );
        m_constantBufferData.world      = Mat44f::IDENTITY;
        m_constantBufferData.view       = m_viewMatrix;
        m_constantBufferData.projection = m_projectionMatrix;

        m_pCameraCB->Map( &m_constantBufferData, sizeof( m_constantBufferData ) );
    }

    {
        m_pLightCB = make_shared<ConstantBuffer>();

        desc.Width = Aligned( ResLightData );

        m_pLightCB->Create( m_pDevice.Get(),prop, desc, D3D12_RESOURCE_STATE_GENERIC_READ );

        // 定数バッファデータの設定.
        m_lightData.size = sizeof( ResLightData );
        m_lightData.position[0] = Vec4f( 0.0f, 5.0f, 0.0f, 1.0f );
        m_lightData.color[0] = Vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
        Vec3f lightDir = Vec3f( 0.0f, -1.0f, -1.0f );
        lightDir.normalized();
        m_lightData.direction[0] = lightDir;
        m_lightData.intensity[0] = Vec3f::ONE;

        Vec3f position = Vec3f( m_lightData.position[0].x, m_lightData.position[0].y, m_lightData.position[0].z );
        Vec3f dir = m_lightData.direction[0];

        Mat44f viewMatrix = Mat44f::CreateLookAt( position, dir, Vec3f::YAXIS );

        float w = 1.86523065f * 5;
        float h = 1.86523065f * 5;

        Mat44f projectionMatrix = Mat44f::CreateOrthoLH( -0.5f*w, 0.5f*w, -0.5f*h, 0.5f*h, 1.0f, 100.0f );
        m_lightData.view[0] = viewMatrix;
        m_lightData.projection[0] = projectionMatrix;

        m_pLightCB->Map( &m_lightData, sizeof( m_lightData ) );
    }

    {
        m_pMaterialCB = make_shared<ConstantBuffer>();

        desc.Width = Aligned( ResMaterialData );

        m_pMaterialCB->Create( m_pDevice.Get(), prop, desc, D3D12_RESOURCE_STATE_GENERIC_READ );

        // 定数バッファデータの設定.
        m_materialData.size = sizeof( ResMaterialData );
        m_materialData.ka = Vec4f::ZERO;
        m_materialData.kd = Vec4f( 0.5f );
        m_materialData.ks = Vec4f( 1.0f, 1.0f, 1.0, 50.0f );

        m_pMaterialCB->Map( &m_materialData, sizeof( m_materialData ) );
    }

    return true;
}

bool App::TermD3D12()
{
    WaitDrawCommandDone();

    if (!CloseHandle( m_fenceEvent ))
    {
        cerr << "Failed to terminate app" << endl;
        return false;
    }
    m_fenceEvent = nullptr;

    m_pSwapChain.Reset();
    m_pFence.Reset();
    m_pCommandQueue.Reset();
    m_pDevice.Reset();

    return true;
}

bool App::TermApp()
{
    return true;
}

bool App::CompileShader( const std::wstring& file, ComPtr<ID3DBlob>& pVSBlob, ComPtr<ID3DBlob>& pPSBlob )
{
    // 頂点シェーダのファイルパスを検索.
    std::wstring path;
    if (!SearchFilePath( file.c_str(), path ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "File Not Found." );
        return false;
    }

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> pBlob;
    hr = D3DCompileFromFile( path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, pVSBlob.ReleaseAndGetAddressOf(), pBlob.ReleaseAndGetAddressOf() );
    if (FAILED( hr ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "D3DCompileFromFile() Failed." );
        return false;
    }

    hr = D3DCompileFromFile( path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, pPSBlob.ReleaseAndGetAddressOf(), nullptr );
    if (FAILED( hr ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "D3DCompileFromFile Failed." );
        return false;
    }

    return true;
}

bool App::SearchFilePath( const std::wstring& filePath, std::wstring& result )
{
    if (filePath.length() == 0 || filePath == L" ")
    {
        return false;
    }

    wchar_t exePath[520] = { 0 };
    GetModuleFileNameW( nullptr, exePath, 520 );
    exePath[519] = 0; // null終端化.
    PathRemoveFileSpecW( exePath );

    wchar_t dstPath[520] = { 0 };

    swprintf_s( dstPath, L"%s", filePath.c_str() );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L".\\shader\\%s", filePath.c_str() );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"..\\%s", filePath.c_str() );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    return false;
}

void App::Present( unsigned int syncInterval )
{
    m_pRenderPassClear->Render( m_pCommandQueue.Get() );
    m_pRenderPassShadow->Render( m_pCommandQueue.Get() );
    m_pRenderPassForward->Render( m_pCommandQueue.Get() );

    m_pSwapChain->Present( syncInterval, 0 );

    WaitDrawCommandDone();

    m_swapChainCount = m_pSwapChain->GetCurrentBackBufferIndex();
}

void App::OnFrameRender()
{
    ResetFrame();

    UpdateGPUBuffers();

    RenderShadowPass();
    RenderForwardPass();

    Present( 1 );
}

void App::RenderShadowPass()
{
    RenderInfo::ConstructParams params;

    D3D12_VIEWPORT& vp = params.viewport;
    vp.Width = static_cast<float>(m_shadowSize.x);
    vp.Height = static_cast<float>(m_shadowSize.y);

    params.depthStencil = m_pShadowMap;
    params.hadleDS      = m_pShadowMap->GetHandleFromHeap( m_pDescHeapForDS );
    params.clearVal     = 1.0f;

    params.targetStateSrc = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    params.targetStateDst = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    params.bDSOnly = true;

    //m_pRenderPassClear->Construct( params );
    m_pRenderPassShadow->Construct( params );
}

void App::RenderForwardPass()
{
    RenderInfo::ConstructParams params;

    params.viewport = m_viewport;

    params.renderTarget = m_pRenderTargets[m_swapChainCount];
    params.hadleRT = m_pRenderTargets[m_swapChainCount]->GetHandleFromHeap( m_pDescHeapForRT );

    params.hadleDS = m_pDSBuffer->GetHandleFromHeap( m_pDescHeapForDS );

    params.targetStateSrc = D3D12_RESOURCE_STATE_PRESENT;
    params.targetStateDst = D3D12_RESOURCE_STATE_RENDER_TARGET;

    m_pRenderPassClear->Construct( params );
    m_pRenderPassForward->Construct( params );
}

void App::UpdateGPUBuffers()
{
    if (m_bUpdateCB == false)
        return;

    m_constantBufferData.world = Mat44f::IDENTITY;
    m_constantBufferData.view = m_viewMatrix;
    m_constantBufferData.projection = m_projectionMatrix;

    m_pCameraCB->Map( &m_constantBufferData, sizeof( m_constantBufferData ) );

    m_bUpdateCB = false;
}

void App::ResetFrame()
{
    m_pRenderPassShadow->Reset();
    m_pRenderPassClear->Reset();
    m_pRenderPassForward->Reset();
}

void App::WaitDrawCommandDone()
{
    const UINT64 fenceValue = m_fenceValue;

    // set target fence value 
    m_pCommandQueue->Signal( m_pFence.Get(), fenceValue );
    m_fenceValue++;

    if (m_pFence->GetCompletedValue() >= fenceValue)
        return;

    // ignite the event when fence value reached target value
    m_pFence->SetEventOnCompletion( fenceValue, m_fenceEvent );

    WaitForSingleObject( m_fenceEvent, INFINITE );
}

void App::ProcessInput()
{
    m_inputManager->ProcessKeyboard();

    m_inputManager->ProcessMouse();

    // Rotating
    if (m_inputManager->IsPressed( InputManager::MOUSE_BUTTON_LEFT)   ||
        m_inputManager->IsPressing( InputManager::MOUSE_BUTTON_LEFT ) )
    {
        float dx = static_cast<float>(m_inputManager->GetMouseState().lX);
        float dy = static_cast<float>(m_inputManager->GetMouseState().lY);

        Vec3f d( dx, -dy, 0.0f );
        if (d.norm() == 0.0f)
            return;

        // update camera position
        m_cameraPoseMatrix.Inverse().Transform( d );

        Vec3f lookAtDir = Vec3f::ZAXIS;
        Mat33f rotMat = m_cameraPoseMatrix.Inverse();
        rotMat.Transform( lookAtDir );
        lookAtDir.normalized();

        Vec3f rotAxis = Vec3f::cross( d, lookAtDir );
        float rotSpeed = 1.0f / 50.0f;
        float rot = rotAxis.norm() * rotSpeed;
        rotAxis.normalized();

        Quatf q( rot, rotAxis);
        Vec3f newPosition = m_cameraPosition;
        q.Rotate( newPosition );

        // update camera pose
        Vec3f targetDir = m_cameraLookAt - newPosition;
        targetDir.normalized();

        Vec3f cameraRotAxis = Vec3f::cross( lookAtDir, targetDir );
        cameraRotAxis.normalized();
        float theta = acos( Vec3f::dot( targetDir, lookAtDir ) );
        
        q = Quatf( theta, cameraRotAxis);
        Mat33f newPose = m_cameraPoseMatrix * q.GetRotationMatrix();

#if 0
        // Limit inverted
        Vec3f tmp = newPose.GetRow( 1 ).normalized();
        if (Vec3f::dot( tmp, Vec3f::YAXIS ) <= 0.0f)
        {
            Quatf tmpQ( rot, lookAtDir );
            m_cameraPoseMatrix = m_cameraPoseMatrix * tmpQ.GetRotationMatrix();
        }
        else
        {
            m_cameraPosition   = newPosition;
            m_cameraPoseMatrix = newPose;
        }
#else
        m_cameraPosition = newPosition;
        m_cameraPoseMatrix = newPose;
#endif

        m_viewMatrix = Mat44f( m_cameraPoseMatrix, m_cameraPosition );
        m_viewMatrix = m_viewMatrix.Inverse();

        m_bUpdateCB = true;
    }

    // Translating screen
    if (m_inputManager->IsPressed( InputManager::MOUSE_BUTTON_CENTER ) ||
        m_inputManager->IsPressing( InputManager::MOUSE_BUTTON_CENTER ))
    {
        float dx = static_cast<float>(m_inputManager->GetMouseState().lX);
        float dy = static_cast<float>(m_inputManager->GetMouseState().lY);

        Vec3f d( -dx, dy, 0.0f );
        if (d.norm() == 0.0f)
            return;

        // update camera position
        m_cameraPoseMatrix.Inverse().Transform( d );
        float transSpeed = 1.0f / 100.0f;
        d *= transSpeed;

        m_cameraPosition += d;
        m_cameraLookAt += d;

        m_viewMatrix = Mat44f( m_cameraPoseMatrix, m_cameraPosition );
        m_viewMatrix = m_viewMatrix.Inverse();

        m_bUpdateCB = true;
    }

    // Zoom in/out
    if (m_inputManager->GetMouseState().lZ != 0.0f)
    {
        float zoomSpeed = 1.0f / 100.0f;
        float dz = m_inputManager->GetMouseState().lZ * zoomSpeed;

        // update camera position
        Vec3f dir = Vec3f::normalize( m_cameraLookAt - m_cameraPosition );
        Vec3f newPos = m_cameraPosition + dz * dir;

        Vec3f tmpDir = Vec3f::normalize( m_cameraLookAt - newPos );
        if (Vec3f::dot( dir, tmpDir ) < 0.0f)
        {
            m_cameraPosition = m_cameraLookAt - dir * 0.001f;
        }
        else
        {
            m_cameraPosition = m_cameraPosition + dz * dir;
        }

        m_viewMatrix = Mat44f( m_cameraPoseMatrix, m_cameraPosition );
        m_viewMatrix = m_viewMatrix.Inverse();

        m_bUpdateCB = true;
    }
}
