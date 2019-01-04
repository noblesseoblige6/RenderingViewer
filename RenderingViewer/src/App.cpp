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

        hr = m_pDevice->CreateDescriptorHeap( &desc,
                                              IID_ID3D12DescriptorHeap,
                                              (void**)(m_pDescHeapRenderTarget.ReleaseAndGetAddressOf()) );

        m_DescHeapRenderTargetSize = m_pDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
        if (FAILED( hr ))
        {
            return false;
        }

        desc.NumDescriptors = 1;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

        hr = m_pDevice->CreateDescriptorHeap( &desc,
                                              IID_ID3D12DescriptorHeap,
                                              (void**)(m_pDescHeapDepthStencil.ReleaseAndGetAddressOf()) );

        m_DescHeapDepthStencilSize = m_pDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );
        if (FAILED( hr ))
        {
            return false;
        }
    }

    // create render targets from back buffers
    {
        auto handle = m_pDescHeapRenderTarget->GetCPUDescriptorHandleForHeapStart();

        D3D12_RENDER_TARGET_VIEW_DESC desc;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;
        desc.Texture2D.PlaneSlice = 0;

        for (int i = 0; i < 2; ++i)
        {
            // Resources come from back buffers
            hr = m_pSwapChain->GetBuffer( i, IID_ID3D12Resource, (void**)m_pRenderTarget[i].ReleaseAndGetAddressOf() );
            if (FAILED( hr ))
            {
                return false;
            }

            m_pDevice->CreateRenderTargetView( m_pRenderTarget[i].Get(), &desc, handle );
            m_renderTargetHandle[i] = handle;

            // advance hadle pointer
            handle.ptr += m_DescHeapRenderTargetSize;
        }
    }

    // create depth stencil view
    {
     // create resource
     // ヒーププロパティの設定.
        D3D12_HEAP_PROPERTIES prop;
        prop.Type = D3D12_HEAP_TYPE_DEFAULT;
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask = 1;
        prop.VisibleNodeMask = 1;

        // リソースの設定.
        D3D12_RESOURCE_DESC desc;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment = 0;
        desc.Width = m_width;
        desc.Height = m_height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 0;
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        // クリア値の設定.
        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        // リソースを生成.
        hr = m_pDevice->CreateCommittedResource( &prop,
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &desc,
                                                 D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                 &clearValue,
                                                 IID_PPV_ARGS( m_pDepthStencil.ReleaseAndGetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "ID3D12Device::CreateCommittedResource() Failed.");
            return false;
        }
            // create depth stencil view
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

            m_pDevice->CreateDepthStencilView( m_pDepthStencil.Get(),
                                               &dsvDesc,
                                               m_pDescHeapDepthStencil->GetCPUDescriptorHandleForHeapStart() );
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
    HRESULT hr = S_OK;

    // create root sinature
    {
        // ディスクリプタレンジの設定.
        D3D12_DESCRIPTOR_RANGE range;
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = 0;
        range.RegisterSpace = 0;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // ルートパラメータの設定.
        D3D12_ROOT_PARAMETER param;
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        param.DescriptorTable.NumDescriptorRanges = 1;
        param.DescriptorTable.pDescriptorRanges = &range;

        // ルートシグニチャの設定.
        D3D12_ROOT_SIGNATURE_DESC desc;
        desc.NumParameters = 1;
        desc.pParameters = &param;
        desc.NumStaticSamplers = 0;
        desc.pStaticSamplers = nullptr;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
                   | D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        ComPtr<ID3DBlob> pSignature;
        ComPtr<ID3DBlob> pError;

        // シリアライズする.
        hr = D3D12SerializeRootSignature( &desc,
                                          D3D_ROOT_SIGNATURE_VERSION_1,
                                          pSignature.GetAddressOf(),
                                          pError.GetAddressOf() );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "D3D12SerializeRootSignataure() Failed.");
            return false;
        }

        // ルートシグニチャを生成.
        hr = m_pDevice->CreateRootSignature( 0,
                                             pSignature->GetBufferPointer(),
                                             pSignature->GetBufferSize(),
                                             IID_PPV_ARGS( m_pRootSignature.GetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR,  "ID3D12Device::CreateRootSignature() Failed.");
            return false;
        }
    }

    // create pipe-line state
    {
        ComPtr<ID3DBlob> pVSBlob;
        ComPtr<ID3DBlob> pPSBlob;

        // 頂点シェーダのファイルパスを検索.
        std::wstring path;
        if (!SearchFilePath( L"SimpleVS.cso", path ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR,  "File Not Found.");
            return false;
        }

        // コンパイル済み頂点シェーダを読み込む.
        hr = D3DReadFileToBlob( path.c_str(), pVSBlob.GetAddressOf() );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR,  "D3DReadFileToBlob() Failed.");
            return false;
        }

        // ピクセルシェーダのファイルパスを検索.
        if (!SearchFilePath( L"SimplePS.cso", path ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "File %s:  Not Found.", "SimplePS.cso");
            return false;
        }

        // コンパイル済みピクセルシェーダを読み込む.
        hr = D3DReadFileToBlob( path.c_str(), pPSBlob.GetAddressOf() );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR,  "D3DReadFileToBlob() Failed.");
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
        descRS.CullMode = D3D12_CULL_MODE_NONE;
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

        // パイプラインステートの設定.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.InputLayout = { inputElements, _countof( inputElements ) };
        desc.pRootSignature = m_pRootSignature.Get();
        desc.VS = { reinterpret_cast<UINT8*>(pVSBlob->GetBufferPointer()), pVSBlob->GetBufferSize() };
        desc.PS = { reinterpret_cast<UINT8*>(pPSBlob->GetBufferPointer()), pPSBlob->GetBufferSize() };
        desc.RasterizerState = descRS;
        desc.BlendState = descBS;
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.StencilEnable = FALSE;
        desc.SampleMask = UINT_MAX;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;

        // パイプラインステートを生成.
        hr = m_pDevice->CreateGraphicsPipelineState( &desc, IID_PPV_ARGS( m_pPipelineState.GetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR,  "ID3D12Device::CreateGraphicsPipelineState() Failed.");
            return false;
        }
    }

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

            Vec4f col;
            if (i % 3 == 0)
                col = Vec4f( 1, 0, 0, 1 );
            else if (i % 3 == 1)
                col = Vec4f( 0, 1, 0, 1 );
            else if (i % 3 == 2)
                col = Vec4f( 0, 0, 1, 1 );

            v.color = col;

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

        hr = m_pDevice->CreateCommittedResource( &prop,
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &desc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr,
                                                 IID_PPV_ARGS(m_pVertexBuffer.ReleaseAndGetAddressOf()) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "ID3D12Device::CreateCommittedResource() Failed.");
            return false;
        }

        // mapping data on CPU memory to GPU memory
        UINT8* pData;
        hr = m_pVertexBuffer->Map( 0, nullptr, reinterpret_cast<void**>(&pData) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "ID3D12Resource::Map() Failed." );
            return false;
        }

        memcpy( pData, &vertices[0], vertexSize );

        m_pVertexBuffer->Unmap( 0, nullptr );

        // 頂点バッファビューの設定.
        m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof( Vertex );
        m_vertexBufferView.SizeInBytes = vertexSize;
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

        int indexSize = static_cast<int>( sizeof( unsigned short ) * indices.size() );

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

        hr = m_pDevice->CreateCommittedResource( &prop,
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &desc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr,
                                                 IID_PPV_ARGS( m_pIndexBuffer.ReleaseAndGetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "ID3D12Device::CreateCommittedResource() Failed." );
            return false;
        }

        // mapping data on CPU memory to GPU memory
        UINT8* pData;
        hr = m_pIndexBuffer->Map( 0, nullptr, reinterpret_cast<void**>(&pData) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR, "ID3D12Resource::Map() Failed." );
            return false;
        }

        memcpy( pData, &indices[0], indexSize );

        m_pIndexBuffer->Unmap( 0, nullptr );

        // indexバッファビューの設定.
        m_indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        m_indexBufferView.SizeInBytes = indexSize;
    }

    // create descriptor heap for constant buffer
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        hr = m_pDevice->CreateDescriptorHeap( &desc, IID_ID3D12DescriptorHeap, (void**)( m_pDescHeapConstant.ReleaseAndGetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR,  "ID3D12Device::CreateDescriptorHeap() Failed.");
            return false;
        }
    }

    // create constant buffer
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
        desc.Width = sizeof( ResConstantBuffer );
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        // リソースを生成.
        hr = m_pDevice->CreateCommittedResource( &prop,
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &desc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr,
                                                 IID_PPV_ARGS( m_pConstantBuffer.ReleaseAndGetAddressOf() ) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR,  "ID3D12Device::CreateCommittedResource() Failed." );
            return false;
        }

        // 定数バッファビューの設定.
        D3D12_CONSTANT_BUFFER_VIEW_DESC bufferDesc = {};
        bufferDesc.BufferLocation = m_pConstantBuffer->GetGPUVirtualAddress();
        bufferDesc.SizeInBytes = sizeof( ResConstantBuffer );

        // 定数バッファビューを生成.
        m_pDevice->CreateConstantBufferView( &bufferDesc, m_pDescHeapConstant->GetCPUDescriptorHandleForHeapStart() );

        // マップする. アプリケーション終了まで Unmap しない.
        hr = m_pConstantBuffer->Map( 0, nullptr, reinterpret_cast<void**>(&m_pCbvDataBegin) );
        if (FAILED( hr ))
        {
            Log::Output( Log::LOG_LEVEL_ERROR,  "ID3D12Resource::Map() Failed." );
            return false;
        }

        // アスペクト比算出.
        auto aspectRatio = static_cast<FLOAT>(m_width/m_height);

        m_cameraPosition = Vec3f( 0.0f, 0.0f, 1.0f );
        m_cameraLookAt = Vec3f::ZERO;

        m_viewMatrix = Mat44f::CreateLookAt( m_cameraPosition, m_cameraLookAt, Vec3f::YAXIS );
        m_projectionMatrix = Mat44f::CreatePerspectiveFieldOfViewLH( static_cast<float>(kQPI), aspectRatio, 0.1f, 100.0f );

        // Find camera pose matrix from view matrix
        Mat44f transMat(Mat44f::IDENTITY);
        transMat.Translate( m_cameraPosition );

        Mat44f poseMat = m_viewMatrix.Inverse() * transMat.Inverse();
        m_cameraPoseMatrix = poseMat.GetScaleAndRoation();

        // 定数バッファデータの設定.
        m_constantBufferData.size       = sizeof( ResConstantBuffer );
        m_constantBufferData.world      = Mat44f::IDENTITY;
        m_constantBufferData.view       = m_viewMatrix;
        m_constantBufferData.projection = m_projectionMatrix;

        memcpy( m_pCbvDataBegin, &m_constantBufferData, sizeof( m_constantBufferData ) );
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

    for (UINT i = 0; i < 2; ++i)
    {
        m_pRenderTarget[i].Reset();
    }

    m_pDepthStencil.Reset();

    m_pSwapChain.Reset();
    m_pFence.Reset();
    m_pCommandList.Reset();
    m_pCommandAllocator.Reset();
    m_pCommandQueue.Reset();
    m_pDevice.Reset();

    return true;
}

bool App::TermApp()
{
    m_pConstantBuffer->Unmap( 0, nullptr );

    m_pConstantBuffer.Reset();
    m_pDescHeapConstant.Reset();

    m_pVertexBuffer.Reset();
    m_vertexBufferView.BufferLocation = 0;
    m_vertexBufferView.SizeInBytes = 0;
    m_vertexBufferView.StrideInBytes = 0;

    m_pIndexBuffer.Reset();
    m_indexBufferView.BufferLocation = 0;
    m_indexBufferView.SizeInBytes = 0;

    m_pPipelineState.Reset();
    m_pRootSignature.Reset();

    return true;
}

bool App::SearchFilePath( const wchar_t* filePath, std::wstring& result )
{
    if (filePath == nullptr)
    {
        return false;
    }

    if (wcscmp( filePath, L" " ) == 0 || wcscmp( filePath, L"" ) == 0)
    {
        return false;
    }

    wchar_t exePath[520] = { 0 };
    GetModuleFileNameW( nullptr, exePath, 520 );
    exePath[519] = 0; // null終端化.
    PathRemoveFileSpecW( exePath );

    wchar_t dstPath[520] = { 0 };

    wcscpy_s( dstPath, filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"..\\%s", filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"..\\..\\%s", filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"\\res\\%s", filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"%s\\%s", exePath, filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"%s\\..\\%s", exePath, filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"%s\\..\\..\\%s", exePath, filePath );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"%s\\res\\%s", exePath, filePath );
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
    desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    desc.Transition.pResource = pResource;
    desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    desc.Transition.StateBefore = stateBefore;
    desc.Transition.StateAfter = stateAfter;

    pCmdList->ResourceBarrier( 1, &desc );
}

void App::Present( unsigned int syncInterval )
{
    // コマンドリストへの記録を終了し，コマンド実行.
    ID3D12CommandList* cmdList = m_pCommandList.Get();

    m_pCommandList->Close();
    m_pCommandQueue->ExecuteCommandLists( 1, &cmdList );

    m_pSwapChain->Present( syncInterval, 0 );

    WaitDrawCommandDone();

    m_swapChainCount = m_pSwapChain->GetCurrentBackBufferIndex();
}

void App::OnFrameRender()
{
    updateGPUBuffers();

    m_pCommandList->SetDescriptorHeaps( 1, m_pDescHeapConstant.GetAddressOf() );
    m_pCommandList->SetGraphicsRootSignature( m_pRootSignature.Get() );
    m_pCommandList->SetGraphicsRootDescriptorTable( 0, m_pDescHeapConstant->GetGPUDescriptorHandleForHeapStart() );

    m_pCommandList->RSSetViewports( 1, &m_viewport );
    m_pCommandList->RSSetScissorRects( 1, &m_scissorRect );
    
    {
        SetResourceBarrier( m_pCommandList.Get(), m_pRenderTarget[m_swapChainCount].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET );

        // レンダーターゲットのハンドルを取得.
        auto handleRTV = m_pDescHeapRenderTarget->GetCPUDescriptorHandleForHeapStart();
        auto handleDSV = m_pDescHeapDepthStencil->GetCPUDescriptorHandleForHeapStart();
        handleRTV.ptr += (m_swapChainCount * m_DescHeapRenderTargetSize);

        // レンダーターゲットの設定.
        m_pCommandList->OMSetRenderTargets( 1, &handleRTV, FALSE, &handleDSV );

        float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_pCommandList->ClearRenderTargetView( m_renderTargetHandle[m_swapChainCount], clearColor, 0, nullptr );
        m_pCommandList->ClearDepthStencilView( handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

        m_pCommandList->SetPipelineState(m_pPipelineState.Get());

        // プリミティブトポロジーの設定.
        m_pCommandList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        // 頂点バッファビューを設定.
        m_pCommandList->IASetVertexBuffers( 0, 1, &m_vertexBufferView );
        m_pCommandList->IASetIndexBuffer( &m_indexBufferView );

        // 描画コマンドを生成.
        m_pCommandList->DrawIndexedInstanced( m_loader.GetIndexCount(), 1, 0, 0, 0 );

        SetResourceBarrier( m_pCommandList.Get(), m_pRenderTarget[m_swapChainCount].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT );
    }

    Present( 1 );

    ResetFrame();
}

void App::updateGPUBuffers()
{
    if (m_bUpdateCB == false)
        return;

    m_constantBufferData.size = sizeof( ResConstantBuffer );
    m_constantBufferData.world = Mat44f::IDENTITY;
    m_constantBufferData.view = m_viewMatrix;
    m_constantBufferData.projection = m_projectionMatrix;

    memcpy( m_pCbvDataBegin, &m_constantBufferData, sizeof( ResConstantBuffer ) );

    m_bUpdateCB = false;
}

void App::ResetFrame()
{
    // コマンドリストとコマンドアロケータをリセットする.
    m_pCommandAllocator->Reset();
    m_pCommandList->Reset( m_pCommandAllocator.Get(), m_pPipelineState.Get() );
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
        float dx = m_inputManager->GetMouseState().lX / 100.0f;
        float dy = m_inputManager->GetMouseState().lY / 100.0f;

        Vec3f d( dx, -dy, 0.0f );
        if (d.norm() == 0.0f)
            return;

        // update camera position
        m_cameraPoseMatrix.Inverse().Transform( d );

        Vec3f rotAxis = Vec3f::cross( d, m_cameraPoseMatrix.GetRow( 2 ) );
        float rot = rotAxis.norm();
        rotAxis.normalized();

        Quatf q( rot, rotAxis);
        q.Rotate( m_cameraPosition );

        // update camera pose
        Vec3f targetDir = m_cameraLookAt - m_cameraPosition;
        targetDir.normalized();

        Vec3f lookAtDir = Vec3f::ZAXIS;
        Mat33f rotMat = m_cameraPoseMatrix.Inverse();
        rotMat.Transform( lookAtDir );
        lookAtDir.normalized();

        Vec3f cameraRotAxis = Vec3f::cross( lookAtDir, targetDir );
        cameraRotAxis.normalized();
        float theta = acos( Vec3f::dot( targetDir, lookAtDir ) );

        q = Quatf( theta, cameraRotAxis);
        m_cameraPoseMatrix = m_cameraPoseMatrix * q.GetRotationMatrix();

        m_viewMatrix = Mat44f( m_cameraPoseMatrix, m_cameraPosition );
        m_viewMatrix = m_viewMatrix.Inverse();

        m_bUpdateCB = true;
    }

    // Translating screen
    if (m_inputManager->IsPressed( InputManager::MOUSE_BUTTON_CENTER ) ||
        m_inputManager->IsPressing( InputManager::MOUSE_BUTTON_CENTER ))
    {
        float dx = m_inputManager->GetMouseState().lX / 1000.0f;
        float dy = m_inputManager->GetMouseState().lY / 1000.0f;

        Vec3f d( -dx, dy, 0.0f );
        if (d.norm() == 0.0f)
            return;

        // update camera position
        m_cameraPoseMatrix.Inverse().Transform( d );

        m_cameraPosition += d;
        m_cameraLookAt += d;

        m_viewMatrix = Mat44f( m_cameraPoseMatrix, m_cameraPosition );
        m_viewMatrix = m_viewMatrix.Inverse();

        m_bUpdateCB = true;
    }

    // Zoom in/out
    if (m_inputManager->GetMouseState().lZ != 0.0f)
    {
        float dz = m_inputManager->GetMouseState().lZ / 1000.0f;

        // update camera position
        Vec3f dir = Vec3f::normalize( m_cameraLookAt - m_cameraPosition );
        m_cameraPosition += dz*dir;

        m_viewMatrix = Mat44f( m_cameraPoseMatrix, m_cameraPosition );
        m_viewMatrix = m_viewMatrix.Inverse();

        m_bUpdateCB = true;
    }
}
