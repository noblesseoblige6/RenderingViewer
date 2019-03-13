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
        m_pDevice.Reset();

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

    // create command allocator
    hr = m_pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            IID_ID3D12CommandAllocator,
                                            (void**)(m_pCommandAllocator.ReleaseAndGetAddressOf()) );
    if (FAILED( hr ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "CreateCommandAllocator() Failed." );
        return false;
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
    {
        hr = m_pDevice->CreateCommandList( 1,
                                           D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           m_pCommandAllocator.Get(),
                                           nullptr,
                                           IID_ID3D12GraphicsCommandList,
                                           (void**)m_pCommandList.ReleaseAndGetAddressOf() );
        if (FAILED( hr ))
        {
            return false;
        }
        m_pCommandList->Close();
    }

        // create command allocator
    hr = m_pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            IID_ID3D12CommandAllocator,
                                            (void**)(m_pCommandAllocatorForShadow.ReleaseAndGetAddressOf()) );
    if (FAILED( hr ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "CreateCommandAllocator() Failed." );
        return false;
    }

    // create command list
    {
        hr = m_pDevice->CreateCommandList( 1,
                                           D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           m_pCommandAllocatorForShadow.Get(),
                                           nullptr,
                                           IID_ID3D12GraphicsCommandList,
                                           (void**)m_pCommandListForShadow.ReleaseAndGetAddressOf() );
        if (FAILED( hr ))
        {
            return false;
        }
        m_pCommandListForShadow->Close();
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
            m_pRenderTargets[i]->SetDescHeap( m_pDescHeapForRT );

            // Resources come from back buffers
            hr = m_pSwapChain->GetBuffer( i, IID_ID3D12Resource, (void**)m_pRenderTargets[i]->GetBufferAddressOf() );
            if (FAILED( hr ))
            {
                return false;
            }

            const D3D12_RESOURCE_DESC& desc = m_pRenderTargets[i]->GetBuffer()->GetDesc();
            m_pRenderTargets[i]->CreateBufferView( m_pDevice.Get(), desc );
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
        m_scissorRect.right = static_cast<long>(m_width);
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

    if (!CreateCBForShadow())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreateCBForShadow() Failed." );
        return false;
    }

    if (!CreateRootSignature())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreateRootSignature() Failed." );
        return false;
    }

    if (!CreatePipelineState())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreatePipelineState() Failed." );
        return false;
    }

    if (!CreatePipelineStateForShadow())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreatePipelineState() Failed." );
        return false;
    }

    if (!CreateGeometry())
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "App::CreateGeometry() Failed." );
        return false;
    }

    return true;
}

bool App::CreateRootSignature()
{
    HRESULT hr = S_OK;

    // create root sinature
    {
        // ディスクリプタレンジの設定.
        D3D12_DESCRIPTOR_RANGE ranges[3];
        ranges[0].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        ranges[0].NumDescriptors                    = 1;
        ranges[0].BaseShaderRegister                = 0;
        ranges[0].RegisterSpace                     = 0;
        ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        ranges[1].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        ranges[1].NumDescriptors                    = 1;
        ranges[1].BaseShaderRegister                = 1;
        ranges[1].RegisterSpace                     = 0;
        ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        ranges[2].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        ranges[2].NumDescriptors                    = 1;
        ranges[2].BaseShaderRegister                = 2;
        ranges[2].RegisterSpace                     = 0;
        ranges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // ルートパラメータの設定.
        D3D12_ROOT_PARAMETER params[1];
        params[0].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
        params[0].DescriptorTable.NumDescriptorRanges = 3;
        params[0].DescriptorTable.pDescriptorRanges   = &ranges[0];
        
        // ルートシグニチャの設定.
        D3D12_ROOT_SIGNATURE_DESC desc;
        desc.NumParameters      = _countof(params);
        desc.pParameters        = params;
        desc.NumStaticSamplers  = 0;
        desc.pStaticSamplers    = nullptr;
        desc.Flags              = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                  D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                  D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                                  D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;;

        ComPtr<ID3DBlob> pSignature;
        ComPtr<ID3DBlob> pError;

        // シリアライズする.
        hr = D3D12SerializeRootSignature( &desc,
                                          D3D_ROOT_SIGNATURE_VERSION_1,
                                          pSignature.GetAddressOf(),
                                          pError.GetAddressOf() );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "D3D12SerializeRootSignataure() Failed." );
            return false;
        }

        // ルートシグニチャを生成.
        hr = m_pDevice->CreateRootSignature( 0,
                                             pSignature->GetBufferPointer(),
                                             pSignature->GetBufferSize(),
                                             IID_PPV_ARGS( m_pRootSignature.GetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "ID3D12Device::CreateRootSignature() Failed." );
            return false;
        }
    }

    // create root sinature
    {
        // ディスクリプタレンジの設定.
        D3D12_DESCRIPTOR_RANGE ranges[1];
        ranges[0].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        ranges[0].NumDescriptors                    = 1;
        ranges[0].BaseShaderRegister                = 0;
        ranges[0].RegisterSpace                     = 0;
        ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // ルートパラメータの設定.
        D3D12_ROOT_PARAMETER params[1];
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        params[0].DescriptorTable.NumDescriptorRanges = _countof( ranges );
        params[0].DescriptorTable.pDescriptorRanges = &ranges[0];

        // ルートシグニチャの設定.
        D3D12_ROOT_SIGNATURE_DESC desc;
        desc.NumParameters = _countof( params );
        desc.pParameters = params;
        desc.NumStaticSamplers = 0;
        desc.pStaticSamplers = nullptr;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

        ComPtr<ID3DBlob> pSignature;
        ComPtr<ID3DBlob> pError;

        // シリアライズする.
        hr = D3D12SerializeRootSignature( &desc,
                                          D3D_ROOT_SIGNATURE_VERSION_1,
                                          pSignature.GetAddressOf(),
                                          pError.GetAddressOf() );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "D3D12SerializeRootSignataure() Failed." );
            return false;
        }

        // ルートシグニチャを生成.
        hr = m_pDevice->CreateRootSignature( 0,
                                             pSignature->GetBufferPointer(),
                                             pSignature->GetBufferSize(),
                                             IID_PPV_ARGS( m_pRootSignatureForShadow.GetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "ID3D12Device::CreateRootSignature() Failed." );
            return false;
        }
    }

    return true;
}

bool App::CreatePipelineState()
{
    HRESULT hr = S_OK;

    // create pipe-line state
    {
        ComPtr<ID3DBlob> pVSBlob;
        ComPtr<ID3DBlob> pPSBlob;
        if (!CompileShader( L"ForwardShading.hlsl", pVSBlob, pPSBlob ))
        {
            return false;
        }

        // 入力レイアウトの設定.
        D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "VTX_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // ラスタライザーステートの設定.
        D3D12_RASTERIZER_DESC descRS;
        descRS.FillMode              = D3D12_FILL_MODE_SOLID;
        descRS.CullMode              = D3D12_CULL_MODE_BACK;
        descRS.FrontCounterClockwise = FALSE;
        descRS.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
        descRS.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        descRS.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        descRS.DepthClipEnable       = TRUE;
        descRS.MultisampleEnable     = FALSE;
        descRS.AntialiasedLineEnable = FALSE;
        descRS.ForcedSampleCount     = 0;
        descRS.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // レンダーターゲットのブレンド設定.
        D3D12_RENDER_TARGET_BLEND_DESC descRTBS = {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL
        };

        // ブレンドステートの設定.
        D3D12_BLEND_DESC descBS;
        descBS.AlphaToCoverageEnable = FALSE;
        descBS.IndependentBlendEnable = FALSE;
        for (UINT i = 0; i<D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            descBS.RenderTarget[i] = descRTBS;
        }

        // デプスステンシルの設定
        D3D12_DEPTH_STENCIL_DESC descDS = {};
        descDS.DepthEnable      = TRUE;                             //深度テストあり
        descDS.DepthFunc        = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        descDS.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ALL;

        descDS.StencilEnable    = FALSE;                            //ステンシルテストなし
        descDS.StencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK;
        descDS.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

        descDS.FrontFace.StencilFailOp      = D3D12_STENCIL_OP_KEEP;
        descDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        descDS.FrontFace.StencilPassOp      = D3D12_STENCIL_OP_KEEP;
        descDS.FrontFace.StencilFunc        = D3D12_COMPARISON_FUNC_ALWAYS;

        descDS.BackFace.StencilFailOp      = D3D12_STENCIL_OP_KEEP;
        descDS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        descDS.BackFace.StencilPassOp      = D3D12_STENCIL_OP_KEEP;
        descDS.BackFace.StencilFunc        = D3D12_COMPARISON_FUNC_ALWAYS;

        // パイプラインステートの設定.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC stateDesc = {};
        // Shader
        stateDesc.VS                = { reinterpret_cast<UINT8*>(pVSBlob->GetBufferPointer()), pVSBlob->GetBufferSize() };
        stateDesc.PS                = { reinterpret_cast<UINT8*>(pPSBlob->GetBufferPointer()), pPSBlob->GetBufferSize() };
        
        // Input layout
        stateDesc.InputLayout       = { inputElements, _countof( inputElements ) };
        
        // Rasterier
        stateDesc.RasterizerState   = descRS;

        // Blend State
        stateDesc.BlendState        = descBS;

        // Depth Stencil
        stateDesc.DepthStencilState = descDS;
        stateDesc.DSVFormat         = DXGI_FORMAT_D32_FLOAT;

        stateDesc.NumRenderTargets      = 1;
        stateDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        // Sampler
        stateDesc.SampleDesc.Count   = 1;
        stateDesc.SampleDesc.Quality = 0;
        stateDesc.SampleMask         = UINT_MAX;

        stateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        stateDesc.pRootSignature        = m_pRootSignature.Get();

        // パイプラインステートを生成.
        hr = m_pDevice->CreateGraphicsPipelineState( &stateDesc, IID_PPV_ARGS( m_pPipelineState.GetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "ID3D12Device::CreateGraphicsPipelineState() Failed." );
            return false;
        }
    }

    return true;
}

bool App::CreatePipelineStateForShadow()
{
    HRESULT hr = S_OK;

    // create pipe-line state
    {
        ComPtr<ID3DBlob> pVSBlob;
        ComPtr<ID3DBlob> pPSBlob;
        if (!CompileShader( L"Shadow.hlsl", pVSBlob, pPSBlob ))
        {
            return false;
        }

        // 入力レイアウトの設定.
        D3D12_INPUT_ELEMENT_DESC inputElements[] = {
            { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "VTX_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // ラスタライザーステートの設定.
        D3D12_RASTERIZER_DESC descRS;
        descRS.FillMode = D3D12_FILL_MODE_SOLID;
        descRS.CullMode = D3D12_CULL_MODE_BACK;
        descRS.FrontCounterClockwise = FALSE;
        descRS.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        descRS.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        descRS.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        descRS.DepthClipEnable = TRUE;
        descRS.MultisampleEnable = FALSE;
        descRS.AntialiasedLineEnable = FALSE;
        descRS.ForcedSampleCount = 0;
        descRS.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // レンダーターゲットのブレンド設定.
        D3D12_RENDER_TARGET_BLEND_DESC descRTBS = {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL
        };

        // ブレンドステートの設定.
        D3D12_BLEND_DESC descBS;
        descBS.AlphaToCoverageEnable = FALSE;
        descBS.IndependentBlendEnable = FALSE;
        for (UINT i = 0; i<D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            descBS.RenderTarget[i] = descRTBS;
        }

        // デプスステンシルの設定
        D3D12_DEPTH_STENCIL_DESC descDS = {};
        descDS.DepthEnable = TRUE;                             //深度テストあり
        descDS.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        descDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

        descDS.StencilEnable = FALSE;                            //ステンシルテストなし
        descDS.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        descDS.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

        descDS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        descDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        descDS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        descDS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        descDS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        descDS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        descDS.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        descDS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        // パイプラインステートの設定.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC stateDesc = {};
        // Shader
        stateDesc.VS = { reinterpret_cast<UINT8*>(pVSBlob->GetBufferPointer()), pVSBlob->GetBufferSize() };
        stateDesc.PS = { reinterpret_cast<UINT8*>(pPSBlob->GetBufferPointer()), pPSBlob->GetBufferSize() };

        // Input layout
        stateDesc.InputLayout = { inputElements, _countof( inputElements ) };

        // Rasterier
        stateDesc.RasterizerState = descRS;

        // Blend State
        stateDesc.BlendState = descBS;

        // Depth Stencil
        stateDesc.DepthStencilState = descDS;
        stateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        stateDesc.NumRenderTargets = 1;
        stateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        // Sampler
        stateDesc.SampleDesc.Count = 1;
        stateDesc.SampleDesc.Quality = 0;
        stateDesc.SampleMask = UINT_MAX;

        stateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        stateDesc.pRootSignature = m_pRootSignatureForShadow.Get();

        // パイプラインステートを生成.
        hr = m_pDevice->CreateGraphicsPipelineState( &stateDesc, IID_PPV_ARGS( m_pPipelineStateForShadow.GetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "ID3D12Device::CreateGraphicsPipelineState() Failed." );
            return false;
        }
    }

    return true;
}

bool App::CreateGeometry()
{
    acModelLoader::LoadOption option;
    m_loader.SetLoadOption( option );
    m_loader.Load( "resource/bunny.obj" );

    // create vertex buffer
    {
        vector<Vertex> vertices;
        for (int i = 0; i < m_loader.GetVertexCount(); ++i)
        {
            Vertex v;
            v.position = m_loader.GetVertex( i );

            if (i < m_loader.GetNormalCount())
                v.normal = m_loader.GetNormal( i );

            if (i < m_loader.GetTexCoordCount())
                v.texCoord = m_loader.GetTexCoord( i );

            // TODO: Vertex color
            //if (i < m_loader.GetColor())
            //    v.color = m_loader.GetColor( i );

            vertices.push_back( v );
        }

        int vertexSize = static_cast<int>(sizeof( Vertex ) * vertices.size());

        // ヒーププロパティの設定.
        D3D12_HEAP_PROPERTIES prop;
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask = 1;
        prop.VisibleNodeMask = 1;

        // リソースの設定.
        D3D12_RESOURCE_DESC desc;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = vertexSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        m_pVertexBuffer = make_shared<VertexBuffer>();
        m_pVertexBuffer->SetDataStride( sizeof( Vertex ) );
        m_pVertexBuffer->Create( m_pDevice.Get(), prop, desc, D3D12_RESOURCE_STATE_GENERIC_READ );
        m_pVertexBuffer->Map( &vertices[0], vertexSize );
        m_pVertexBuffer->Unmap();
    }

    // create index buffer
    {
        vector<unsigned short> indices;
        for (int i = 0; i < m_loader.GetIndexCount(); ++i)
        {
            unsigned short index;
            index = static_cast<unsigned short>(m_loader.GetIndex( i ));

            indices.push_back( index );
        }

        int indexSize = static_cast<int>(sizeof( unsigned short ) * indices.size());

        // ヒーププロパティの設定.
        D3D12_HEAP_PROPERTIES prop;
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask = 1;
        prop.VisibleNodeMask = 1;

        // リソースの設定.
        D3D12_RESOURCE_DESC desc;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = indexSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        m_pIndexBuffer = make_shared<IndexBuffer>();
        m_pIndexBuffer->SetDataFormat(DXGI_FORMAT_R16_UINT);
        m_pIndexBuffer->Create( m_pDevice.Get(), prop, desc, D3D12_RESOURCE_STATE_GENERIC_READ );
        m_pIndexBuffer->Map( &indices[0], indexSize );
        m_pIndexBuffer->Unmap();
    }

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
        m_pDSBuffer->SetDescHeap( m_pDescHeapForDS );
        m_pDSBuffer->Create( m_pDevice.Get(), prop, desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal );
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

        m_shadowSize.x = 1024;
        m_shadowSize.y = 1024;
        
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
        m_pShadowMap->SetDescHeap( m_pDescHeapForDS );
        m_pShadowMap->Create( m_pDevice.Get(), prop, desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearVal );
    }

    return true;
}

bool App::CreateCB()
{
    // create descriptor heap for constant buffer
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 3;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        m_pDescHeapForCB = make_shared<DescriptorHeap>();
        m_pDescHeapForCB->Create( m_pDevice.Get(), desc );
    }

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

        m_pCameraCB->SetDescHeap( m_pDescHeapForCB );

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

        m_pLightCB->SetDescHeap( m_pDescHeapForCB );

        desc.Width = Aligned( ResLightData );

        m_pLightCB->Create( m_pDevice.Get(),prop, desc, D3D12_RESOURCE_STATE_GENERIC_READ );

        // 定数バッファデータの設定.
        m_lightData.position[0] = Vec4f( 0.0f, 5.0f, 5.0f, 1.0f );
        m_lightData.color[0] = Vec4f( 0.1f, 1.0f, 0.1f, 1.0f );
        m_lightData.direction[0] = Vec3f( 0.0f, -1.0f, -1.0f );
        m_lightData.intensity[0] = Vec3f::ONE;

        m_pLightCB->Map( &m_lightData, sizeof( m_lightData ) );
    }

    {
        m_pMaterialCB = make_shared<ConstantBuffer>();

        m_pMaterialCB->SetDescHeap( m_pDescHeapForCB );

        desc.Width = Aligned( ResMaterialData );

        m_pMaterialCB->Create( m_pDevice.Get(), prop, desc, D3D12_RESOURCE_STATE_GENERIC_READ );

        // 定数バッファデータの設定.
        m_materialData.ka = Vec4f::ZERO;
        m_materialData.kd = Vec4f( 0.5f );
        m_materialData.ks = Vec4f( 1.0f, 1.0f, 1.0, 50.0f );

        m_pMaterialCB->Map( &m_materialData, sizeof( m_materialData ) );

    }

    return true;
}

bool App::CreateCBForShadow()
{
    // create descriptor heap for constant buffer
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        m_pDescHeapForShadow = make_shared<DescriptorHeap>();
        m_pDescHeapForShadow->Create( m_pDevice.Get(), desc );
    }

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

    // create CB for light view
    {
        m_pLightViewCB = make_shared<ConstantBuffer>();

        m_pLightViewCB->SetDescHeap( m_pDescHeapForShadow );

        desc.Width = Aligned( ResConstantBuffer );

        m_pLightViewCB->Create( m_pDevice.Get(), prop, desc, D3D12_RESOURCE_STATE_GENERIC_READ );

        // アスペクト比算出.
        auto aspectRatio = static_cast<FLOAT>(m_width / m_height);

        Vec3f position = Vec3f( m_lightData.position[0].x, m_lightData.position[0].y, m_lightData.position[0].z );
        Vec3f dir = m_lightData.direction[0];

        Mat44f viewMatrix = Mat44f::CreateLookAt( position, dir, Vec3f::YAXIS );

        float w = 1.86523065 * 10;
        float h = 1.86523065 * 10;

        Mat44f projectionMatrix = Mat44f::CreateOrthoLH( -0.5f*w, 0.5f*w, -0.5f*h, 0.5f*h, 1.0f, 100.0f );

        // 定数バッファデータの設定.
        m_lightViewData.size = sizeof( ResConstantBuffer );
        m_lightViewData.world = Mat44f::IDENTITY;
        m_lightViewData.view = viewMatrix;
        m_lightViewData.projection = projectionMatrix;

        m_pLightViewCB->Map( &m_lightViewData, sizeof( m_lightViewData ) );
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
    m_pCommandList.Reset();
    m_pCommandListForShadow.Reset();
    m_pCommandAllocator.Reset();
    m_pCommandQueue.Reset();
    m_pDevice.Reset();

    return true;
}

bool App::TermApp()
{
    m_pPipelineState.Reset();
    m_pRootSignature.Reset();

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

    hr = D3DCompileFromFile( path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, pVSBlob.ReleaseAndGetAddressOf(), nullptr );
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

void App::SetResourceBarrier( ID3D12GraphicsCommandList* pCmdList,
                              ID3D12Resource* pResource,
                              D3D12_RESOURCE_STATES stateBefore,
                              D3D12_RESOURCE_STATES stateAfter )
{
    D3D12_RESOURCE_BARRIER desc = {};
    desc.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    desc.Transition.pResource   = pResource;
    desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    desc.Transition.StateBefore = stateBefore;
    desc.Transition.StateAfter  = stateAfter;

    pCmdList->ResourceBarrier( 1, &desc );
}

void App::Present( unsigned int syncInterval )
{
    // コマンドリストへの記録を終了し，コマンド実行.
    ID3D12CommandList* cmdList[] = {
        m_pCommandList.Get(),
        m_pCommandListForShadow.Get()
    };

    m_pCommandQueue->ExecuteCommandLists( _countof(cmdList), cmdList );

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
    m_pCommandListForShadow->SetDescriptorHeaps( 1, m_pDescHeapForShadow->GetDescHeapAddress() );
    m_pCommandListForShadow->SetGraphicsRootSignature( m_pRootSignatureForShadow.Get() );
    m_pCommandListForShadow->SetGraphicsRootDescriptorTable( 0, m_pDescHeapForShadow->GetGPUHandle() );

    D3D12_VIEWPORT vp;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = static_cast<float>(m_shadowSize.x);
    vp.Height = static_cast<float>(m_shadowSize.y);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    D3D12_RECT sr;
    sr.right = static_cast<long>(vp.Width);
    sr.bottom = static_cast<long>(vp.Height);

    m_pCommandListForShadow->RSSetViewports( 1, &vp );
    m_pCommandListForShadow->RSSetScissorRects( 1, &sr );

    {
        SetResourceBarrier( m_pCommandListForShadow.Get(), m_pShadowMap->GetBuffer(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE );

        // レンダーターゲットのハンドルを取得.
        auto handleDSV = m_pShadowMap->GetHadle();

        // レンダーターゲットの設定.
        m_pCommandListForShadow->OMSetRenderTargets( 0, nullptr, FALSE, &handleDSV );

        float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_pCommandListForShadow->ClearDepthStencilView( handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

        m_pCommandListForShadow->SetPipelineState( m_pPipelineStateForShadow.Get() );

        // プリミティブトポロジーの設定.
        m_pCommandListForShadow->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        // 頂点バッファビューを設定.
        m_pCommandListForShadow->IASetVertexBuffers( 0, 1, &m_pVertexBuffer->GetVertexBV() );
        m_pCommandListForShadow->IASetIndexBuffer( &m_pIndexBuffer->GetIndexBV() );

        // 描画コマンドを生成.
        m_pCommandListForShadow->DrawIndexedInstanced( m_loader.GetIndexCount(), 1, 0, 0, 0 );

        SetResourceBarrier( m_pCommandListForShadow.Get(), m_pShadowMap->GetBuffer(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );

        m_pCommandListForShadow->Close();
    }
}

void App::RenderForwardPass()
{
    m_pCommandList->SetDescriptorHeaps( 1, m_pDescHeapForCB->GetDescHeapAddress() );
    m_pCommandList->SetGraphicsRootSignature( m_pRootSignature.Get() );
    m_pCommandList->SetGraphicsRootDescriptorTable( 0, m_pDescHeapForCB->GetGPUHandle() );

    m_pCommandList->RSSetViewports( 1, &m_viewport );
    m_pCommandList->RSSetScissorRects( 1, &m_scissorRect );

    {
        SetResourceBarrier( m_pCommandList.Get(), m_pRenderTargets[m_swapChainCount]->GetBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET );

        // レンダーターゲットのハンドルを取得.
        auto handleRTV = m_pRenderTargets[m_swapChainCount]->GetHadle();
        auto handleDSV = m_pDSBuffer->GetHadle();

        // レンダーターゲットの設定.
        m_pCommandList->OMSetRenderTargets( 1, &handleRTV, FALSE, &handleDSV );

        float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_pCommandList->ClearRenderTargetView( handleRTV, clearColor, 0, nullptr );
        m_pCommandList->ClearDepthStencilView( handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

        m_pCommandList->SetPipelineState( m_pPipelineState.Get() );

        // プリミティブトポロジーの設定.
        m_pCommandList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        // 頂点バッファビューを設定.
        m_pCommandList->IASetVertexBuffers( 0, 1, &m_pVertexBuffer->GetVertexBV() );
        m_pCommandList->IASetIndexBuffer( &m_pIndexBuffer->GetIndexBV() );

        // 描画コマンドを生成.
        m_pCommandList->DrawIndexedInstanced( m_loader.GetIndexCount(), 1, 0, 0, 0 );

        SetResourceBarrier( m_pCommandList.Get(), m_pRenderTargets[m_swapChainCount]->GetBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT );
        
        m_pCommandList->Close();
    }
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
    HRESULT hr = S_OK;
    // コマンドリストとコマンドアロケータをリセットする.
   hr = m_pCommandAllocator->Reset();
   if (FAILED( hr ))
       Log::Output( Log::LOG_LEVEL_DEBUG, "ID3D12CommandAllocator::Reset Failed" );

    hr = m_pCommandList->Reset( m_pCommandAllocator.Get(), m_pPipelineState.Get() );
    if (FAILED( hr ))
        Log::Output( Log::LOG_LEVEL_DEBUG, "ID3D12GraphicsCommandList::Reset Failed" );

    hr = m_pCommandAllocatorForShadow->Reset();
    if (FAILED( hr ))
        Log::Output( Log::LOG_LEVEL_DEBUG, "ID3D12CommandAllocator::Reset Failed" );

    hr = m_pCommandListForShadow->Reset( m_pCommandAllocatorForShadow.Get(), m_pPipelineStateForShadow.Get() );
    if (FAILED( hr ))
        Log::Output( Log::LOG_LEVEL_DEBUG, "ID3D12GraphicsCommandList::Reset Failed" );
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
