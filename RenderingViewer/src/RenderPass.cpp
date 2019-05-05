RenderPass::RenderPass( ID3D12Device* pDevice )
{
    AC_USE_VAR( pDevice );
}

RenderPass::~RenderPass()
{
}

void RenderPass::SetScene( shared_ptr<Scene> pScene )
{
    m_pScene = pScene;
}

void RenderPass::BindResource( ID3D12Device* pDevice, shared_ptr<Buffer> pResource, Buffer::BUFFER_VIEW_TYPE type )
{
    for (auto pRenderInfo : m_pRenderContexts)
    {
        pResource->CreateBufferView( pDevice, pRenderInfo->GetDescHeap(), type );
    }
}

void RenderPass::Construct( ID3D12Device* pDevice )
{
    m_pRenderContexts.clear();

    auto findNode = [&]( Node::NODE_TYPE type, shared_ptr<RenderContext>& pContext )
    {
        for (auto& pNode : m_pScene->GetRootNode()->GetChildren())
        {
            if (!pNode->IsNodeType( type ))
                continue;

            pNode->BindDescriptorHeap( pDevice, pContext->GetDescHeap() );
        }
    };

    for (auto& pNode : m_pScene->GetRootNode()->GetChildren())
    {
        if (!pNode->IsNodeType( Node::NODE_TYPE_MODEL ))
            continue;

        shared_ptr<RenderContext> pContext = make_shared<RenderContext>( pDevice );

        pContext->SetDescHeap( CreateDescHeap( pDevice ) );
        pContext->SetRootSinature( CreateRootSinature( pDevice ) );
        pContext->SetPipelineState( CreatePipelineState( pDevice, pContext->GetRootSignature() ) );

        // Camera
        findNode( Node::NODE_TYPE_CAMERA, pContext);

        // Light
        findNode( Node::NODE_TYPE_LIGHT, pContext );

        pContext->SetNode( pNode );
        pNode->BindDescriptorHeap( pDevice, pContext->GetDescHeap() );

        m_pRenderContexts.push_back( pContext );
    }

    //Shadow
}

void RenderPass::Draw( const RenderContext::ConstructParams& params )
{
    for (auto pRenderInfo : m_pRenderContexts)
    {
        pRenderInfo->Draw( params );
    }
}

void RenderPass::Render( ID3D12CommandQueue* pCommadnQueue )
{
    ID3D12CommandList* cmdList[1024];

    for (int i = 0; i < m_pRenderContexts.size(); ++i)
    {
        const auto& pCommand = m_pRenderContexts[i]->GetCommandList();
        cmdList[i] = pCommand->GetCommandList();
    }

    pCommadnQueue->ExecuteCommandLists( m_pRenderContexts.size(), cmdList );
}

void RenderPass::Reset()
{
    for (auto pRenderInfo : m_pRenderContexts)
    {
        pRenderInfo->Reset();
    }
}

RenderPassForward::RenderPassForward( ID3D12Device* pDevice )
    : RenderPass( pDevice )
{
    if (!Shader::CompileShader( L"ForwardShading.hlsl", m_pVSBlob, m_pPSBlob ))
    {
        return;
    }

    m_shader.vs = { reinterpret_cast<UINT8*>(m_pVSBlob->GetBufferPointer()), m_pVSBlob->GetBufferSize() };
    m_shader.ps = { reinterpret_cast<UINT8*>(m_pPSBlob->GetBufferPointer()), m_pPSBlob->GetBufferSize() };

    // 入力レイアウトの設定.
    m_element.elements = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "VTX_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

RenderPassForward::~RenderPassForward()
{
}

shared_ptr<DescriptorHeap> RenderPassForward::CreateDescHeap( ID3D12Device* pDevice )
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors             = 4;
    desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    shared_ptr<DescriptorHeap> pDescHeap = make_shared<DescriptorHeap>();
    pDescHeap->Create( pDevice, desc );

    return pDescHeap;
}

shared_ptr<RootSignature> RenderPassForward::CreateRootSinature( ID3D12Device* pDevice )
{
    // ディスクリプタレンジの設定.
    D3D12_DESCRIPTOR_RANGE ranges[2];
    ranges[0].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors                    = 3;
    ranges[0].BaseShaderRegister                = 0;
    ranges[0].RegisterSpace                     = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ルートパラメータの設定.
    D3D12_ROOT_PARAMETER params[1];
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[0].DescriptorTable.NumDescriptorRanges = _countof( ranges );
    params[0].DescriptorTable.pDescriptorRanges = &ranges[0];

    // 静的サンプラーの設定.
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ルートシグニチャの設定.
    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.NumParameters = _countof( params );
    desc.pParameters = params;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;;

    shared_ptr<RootSignature> pRootSignature = make_shared<RootSignature>();
    pRootSignature->Create( pDevice, desc );

    return pRootSignature;
}

shared_ptr<PipelineState> RenderPassForward::CreatePipelineState( ID3D12Device* pDevice, shared_ptr<RootSignature> pRootSignature )
{
    shared_ptr<PipelineState> pPipelineState = make_shared<PipelineState>( m_element, m_shader, pRootSignature );
    pPipelineState->GetDesc().NumRenderTargets = 1;
    pPipelineState->GetDesc().RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    pPipelineState->Create( pDevice );

    return pPipelineState;
}

RenderPassShadow::RenderPassShadow( ID3D12Device* pDevice )
    : RenderPass( pDevice )
{
    if (!Shader::CompileShader( L"Shadow.hlsl", m_pVSBlob, m_pPSBlob ))
    {
        return;
    }

    m_shader.vs = { reinterpret_cast<UINT8*>(m_pVSBlob->GetBufferPointer()), m_pVSBlob->GetBufferSize() };
    m_shader.ps = { reinterpret_cast<UINT8*>(m_pPSBlob->GetBufferPointer()), m_pPSBlob->GetBufferSize() };

    // 入力レイアウトの設定
    m_element.elements = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "VTX_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

RenderPassShadow::~RenderPassShadow()
{
}

shared_ptr<DescriptorHeap> RenderPassShadow::CreateDescHeap( ID3D12Device* pDevice )
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    shared_ptr<DescriptorHeap> pDescHeap = make_shared<DescriptorHeap>();
    pDescHeap->Create( pDevice, desc );

    return pDescHeap;
}

shared_ptr<RootSignature> RenderPassShadow::CreateRootSinature( ID3D12Device* pDevice )
{
    // ディスクリプタレンジの設定.
    D3D12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 1;
    ranges[0].RegisterSpace = 0;
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

    shared_ptr<RootSignature> pRootSignature = make_shared<RootSignature>();
    pRootSignature->Create( pDevice, desc );

    return pRootSignature;
}

shared_ptr<PipelineState> RenderPassShadow::CreatePipelineState( ID3D12Device* pDevice, shared_ptr<RootSignature> pRootSignature )
{
    shared_ptr<PipelineState> pPipelineState = make_shared<PipelineState>( m_element, m_shader, pRootSignature );
    pPipelineState->Create( pDevice );

    return pPipelineState;
}

RenderPassClear::RenderPassClear( ID3D12Device* pDevice )
    : RenderPass( pDevice )
{
}

RenderPassClear::~RenderPassClear()
{
}

void RenderPassClear::Construct( ID3D12Device* pDevice )
{
    shared_ptr<RenderContext> pContext = make_shared<RenderContext>( pDevice );
    m_pRenderContexts.push_back( pContext );
}

void RenderPassClear::Clear( const RenderContext::ConstructParams& params )
{
    for (auto pRenderInfo : m_pRenderContexts)
    {
        pRenderInfo->Clear( params );
    }
}

shared_ptr<DescriptorHeap> RenderPassClear::CreateDescHeap( ID3D12Device* pDevice )
{
    AC_USE_VAR( pDevice );
    return nullptr;
}

shared_ptr<RootSignature> RenderPassClear::CreateRootSinature( ID3D12Device* pDevice )
{
    AC_USE_VAR( pDevice );
    return nullptr;
}

shared_ptr<PipelineState> RenderPassClear::CreatePipelineState( ID3D12Device* pDevice, shared_ptr<RootSignature> pRootSignature )
{
    AC_USE_VAR( pDevice );
    return nullptr;
}