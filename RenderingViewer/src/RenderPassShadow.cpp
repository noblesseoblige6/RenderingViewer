RenderPassShadow::RenderPassShadow( ID3D12Device* pDevice )
    : RenderPass( pDevice )
{
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

void RenderPassShadow::Construct( ID3D12Device * pDevice )
{
    RenderPass::Construct( pDevice );

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

        // Light
        findNode( Node::NODE_TYPE_LIGHT, pContext );

        pContext->SetNode( pNode );

        m_pRenderContexts.push_back( pContext );
    }
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

shared_ptr<PipelineState> RenderPassShadow::CreatePipelineState( ID3D12Device* pDevice, shared_ptr<RootSignature> pRootSignature, shared_ptr<Node> pNode )
{
    // TODO: Shader determined by node's material
    ComPtr<ID3DBlob> pVSBlob;
    ComPtr<ID3DBlob> pPSBlob;

    PipelineState::ShaderCode   shader;
    if (!Shader::CompileShader( L"Shadow.hlsl", pVSBlob, pPSBlob ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "Loading Shader Failed." );
    }
    else
    {
        shader.vs = { reinterpret_cast<UINT8*>(pVSBlob->GetBufferPointer()), pVSBlob->GetBufferSize() };
        shader.ps = { reinterpret_cast<UINT8*>(pPSBlob->GetBufferPointer()), pPSBlob->GetBufferSize() };
    }

    shared_ptr<PipelineState> pPipelineState = make_shared<PipelineState>( m_element, shader, pRootSignature );
    pPipelineState->Create( pDevice );

    return pPipelineState;
}
