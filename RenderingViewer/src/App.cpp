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

            m_pRenderTargets[i]->CreateBufferView( m_pDevice.Get(), m_pDescHeapForRT, Buffer::BUFFER_VIEW_TYPE_RENDER_TARGET );
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
    if (!CreateScene())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreateScene() Failed." );
        return false;
    }

    if (!CreateRenderPass())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreateRenderPass() Failed." );
        return false;
    }

    return true;
}

bool App::CreateRenderPass()
{
    m_pRenderPassClear = make_shared<RenderPassClear>( m_pDevice.Get() );
    m_pRenderPassClear->Construct( m_pDevice.Get() );

    m_pRenderPassForward = make_shared<RenderPassForward>( m_pDevice.Get() );

    m_pRenderPassForward->SetScene( m_pScene );

    m_pRenderPassForward->Construct( m_pDevice.Get() );
    m_pRenderPassForward->BindResource(m_pDevice.Get(), m_pShadowMap, Buffer::BUFFER_VIEW_TYPE_SHADER_RESOURCE);

    m_pRenderPassClearShadow = make_shared<RenderPassClear>( m_pDevice.Get() );
    m_pRenderPassClearShadow->Construct( m_pDevice.Get() );

    m_pRenderPassShadow = make_shared<RenderPassShadow>( m_pDevice.Get() );
    m_pRenderPassShadow->SetScene( m_pScene );

    m_pRenderPassShadow->Construct( m_pDevice.Get() );

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
        D3D12_CLEAR_VALUE clearVal = {};
        clearVal.Format               = DXGI_FORMAT_D32_FLOAT;
        clearVal.DepthStencil.Depth   = 1.0f;
        clearVal.DepthStencil.Stencil = 0;

        m_pDSBuffer = make_shared<DepthStencilBuffer>();

        m_pDSBuffer->Create( m_pDevice.Get(), m_width, m_height, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal );
        m_pDSBuffer->CreateBufferView( m_pDevice.Get(), m_pDescHeapForDS, Buffer::BUFFER_VIEW_TYPE_DEPTH_STENCIL);
    }

        // create constant buffer
    {
        D3D12_CLEAR_VALUE clearVal = {};
        clearVal.Format               = DXGI_FORMAT_D32_FLOAT;
        clearVal.DepthStencil.Depth   = 1.0f;
        clearVal.DepthStencil.Stencil = 0;

        m_shadowSize.x = 2048;
        m_shadowSize.y = 2048;

        m_pShadowMap = make_shared<DepthStencilBuffer>();
        m_pShadowMap->Create( m_pDevice.Get(), m_shadowSize.x, m_shadowSize.y, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearVal );
        m_pShadowMap->CreateBufferView( m_pDevice.Get(), m_pDescHeapForDS, Buffer::BUFFER_VIEW_TYPE_DEPTH_STENCIL );
    }

    return true;
}

bool App::CreateScene()
{
    m_pScene = make_shared<Scene>( m_pDevice.Get() );

    m_pCamera = make_shared<Camera>( m_pDevice.Get() );
    m_pScene->GetRootNode()->AddChild( m_pCamera );

    m_pCamera->SetPosition( Vec3f( 0.0f, 0.0f, 5.0f ) );
    m_pCamera->SetLookAt( Vec3f::ZERO );

    float aspectRatio = (float)m_width / m_height;
    m_pCamera->CreateViewMatrix();
    m_pCamera->SetProjectionMatrix( Mat44f::CreatePerspectiveFieldOfViewLH( static_cast<float>(DEG2RAD( 50 )), aspectRatio, 1.0f, 100.0f ) );

    // Find camera pose matrix from view matrix
    Mat44f transMat( Mat44f::IDENTITY );
    transMat.Translate( m_pCamera->GetPosition() );

    Mat44f poseMat = m_pCamera->GetViewMatrix().Inverse() * transMat.Inverse();
    m_pCamera->SetPoseMatrix( poseMat.GetScaleAndRoation() );

    m_pCamera->UpdateGPUBuffer();

    m_pLight = make_shared<Light>( m_pDevice.Get() );
    m_pScene->GetRootNode()->AddChild( m_pLight );

    m_pBunny = make_shared<Model>( m_pDevice.Get() );
    m_pScene->GetRootNode()->AddChild( m_pBunny );

    m_pBunny->BindAsset( m_pDevice.Get(), "resource/bunny.obj" );

    m_pFloor = make_shared<Model>( m_pDevice.Get() );
    m_pScene->GetRootNode()->AddChild( m_pFloor );

    m_pFloor->BindAsset( m_pDevice.Get(), "resource/floor.obj" );

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

void App::Present( unsigned int syncInterval )
{
    m_pRenderPassClearShadow->Render( m_pCommandQueue.Get() );
    m_pRenderPassShadow->Render( m_pCommandQueue.Get() );

    m_pRenderPassClear->Render( m_pCommandQueue.Get() );
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
    RenderContext::ConstructParams params;

    D3D12_VIEWPORT& vp = params.viewport;
    vp.Width = static_cast<float>(m_shadowSize.x);
    vp.Height = static_cast<float>(m_shadowSize.y);

    params.depthStencil = m_pShadowMap;
    params.hadleDS      = m_pShadowMap->GetHandleFromHeap( m_pDescHeapForDS );
    params.clearVal     = 1.0f;

    params.targetStateSrc = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    params.targetStateDst = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    params.bDSOnly = true;

    m_pRenderPassClearShadow->Clear( params );
    m_pRenderPassShadow->Draw( params );
}

void App::RenderForwardPass()
{
    RenderContext::ConstructParams params;

    params.viewport = m_viewport;

    params.renderTarget = m_pRenderTargets[m_swapChainCount];
    params.hadleRT = m_pRenderTargets[m_swapChainCount]->GetHandleFromHeap( m_pDescHeapForRT );

    params.hadleDS = m_pDSBuffer->GetHandleFromHeap( m_pDescHeapForDS );

    params.targetStateSrc = D3D12_RESOURCE_STATE_PRESENT;
    params.targetStateDst = D3D12_RESOURCE_STATE_RENDER_TARGET;

    m_pRenderPassClear->Clear( params );
    m_pRenderPassForward->Draw( params );
}

void App::UpdateGPUBuffers()
{
    if (m_bUpdateCB == false)
        return;

    m_pCamera->UpdateGPUBuffer();

    m_bUpdateCB = false;
}

void App::ResetFrame()
{
    m_pRenderPassClearShadow->Reset();
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
        m_pCamera->GetPoseMatrix().Inverse().Transform( d );

        Vec3f lookAtDir = Vec3f::ZAXIS;
        Mat33f rotMat = m_pCamera->GetPoseMatrix().Inverse();
        rotMat.Transform( lookAtDir );
        lookAtDir.normalized();

        Vec3f rotAxis = Vec3f::cross( d, lookAtDir );
        float rotSpeed = 1.0f / 50.0f;
        float rot = rotAxis.norm() * rotSpeed;
        rotAxis.normalized();

        Quatf q( rot, rotAxis);
        Vec3f newPosition = m_pCamera->GetPosition();
        q.Rotate( newPosition );

        // update camera pose
        Vec3f targetDir = m_pCamera->GetLookAt() - newPosition;
        targetDir.normalized();

        Vec3f cameraRotAxis = Vec3f::cross( lookAtDir, targetDir );
        cameraRotAxis.normalized();
        float theta = acos( Vec3f::dot( targetDir, lookAtDir ) );
        
        q = Quatf( theta, cameraRotAxis);
        Mat33f newPose = m_pCamera->GetPoseMatrix() * q.GetRotationMatrix();

        m_pCamera->SetPosition( newPosition );
        m_pCamera->SetPoseMatrix( newPose );

        Mat44f viewMatrix = Mat44f( newPose, newPosition );
        m_pCamera->SetViewMatrix( viewMatrix.Inverse() );

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

        // update camera positio
        m_pCamera->GetPoseMatrix().Inverse().Transform( d );
        float transSpeed = 1.0f / 100.0f;
        d *= transSpeed;

        Vec3f newPos = m_pCamera->GetPosition() + d;
        Vec3f newLookAt = m_pCamera->GetLookAt() + d;

        m_pCamera->SetPosition( newPos );
        m_pCamera->SetLookAt( newLookAt );

        Mat44f viewMatrix = Mat44f( m_pCamera->GetPoseMatrix(), newPos );
        m_pCamera->SetViewMatrix( viewMatrix.Inverse() );

        m_bUpdateCB = true;
    }

    // Zoom in/out
    if (m_inputManager->GetMouseState().lZ != 0.0f)
    {
        float zoomSpeed = 1.0f / 100.0f;
        float dz = m_inputManager->GetMouseState().lZ * zoomSpeed;

        // update camera position
        Vec3f dir = m_pCamera->GetLookDir();
        Vec3f newPos = m_pCamera->GetPosition() + dz * dir;

        Vec3f tmpDir = Vec3f::normalize( m_pCamera->GetLookAt() - newPos );
        if (Vec3f::dot( dir, tmpDir ) < 0.0f)
        {
            newPos = m_pCamera->GetLookAt() - dir * 0.001f;
        }
        else
        {
            newPos = m_pCamera->GetPosition() + dz * dir;
        }

        m_pCamera->SetPosition( newPos );
        Mat44f viewMatrix = Mat44f( m_pCamera->GetPoseMatrix(), newPos );
        m_pCamera->SetViewMatrix( viewMatrix.Inverse() );

        m_bUpdateCB = true;
    }
}
