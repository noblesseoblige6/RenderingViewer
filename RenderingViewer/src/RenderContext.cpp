RenderContext::RenderContext( ID3D12Device* pDevice )
{
    m_pCommandList = make_shared<CommandList>( pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT );
}

RenderContext::~RenderContext()
{
}

void RenderContext::Reset()
{
    m_pCommandList->Reset( m_pPipelineState );
}

bool RenderContext::Clear( const ConstructParams& params )
{
    if (params.bDSOnly)
    {
        m_pCommandList->Begin( params.depthStencil, params.targetStateSrc, params.targetStateDst );
        {
            auto hadleDS = params.hadleDS;

            m_pCommandList->SetTargets( nullptr, &hadleDS );

            m_pCommandList->ClearTargets( nullptr, nullptr, &hadleDS, D3D12_CLEAR_FLAG_DEPTH, params.clearVal );
        }
        m_pCommandList->End();
    }
    else
    {
        m_pCommandList->Begin( params.renderTarget, params.targetStateSrc, params.targetStateDst );
        {
            auto handleRTV = params.hadleRT;
            auto handleDSV = params.hadleDS;

            m_pCommandList->SetTargets( &handleRTV, &handleDSV );

            float clearColor[] = { params.clearColor.x, params.clearColor.y, params.clearColor.z, 1.0f };
            m_pCommandList->ClearTargets( &handleRTV, clearColor, &handleDSV, D3D12_CLEAR_FLAG_DEPTH, params.clearVal );
        }
        m_pCommandList->End();
    }

    return true;
}

bool RenderContext::Draw( const ConstructParams& params )
{
    if (m_pNode == nullptr)
        return false;

    m_pCommandList->SetRootSignature( m_pRootSignature );
    m_pCommandList->SetDescriptorHeaps( 1, m_pDescHeap );
    m_pCommandList->SetPipelineState( m_pPipelineState );

    m_pCommandList->SetViewport( params.viewport );

    shared_ptr<Model> pModel = static_pointer_cast<Model>(m_pNode);

    if (params.bDSOnly)
    {
        m_pCommandList->Begin( params.depthStencil, params.targetStateSrc, params.targetStateDst );
        {
            auto hadleDS = params.hadleDS;

            m_pCommandList->SetTargets( nullptr, &hadleDS );

            m_pCommandList->Draw( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, pModel->GetVertexBuffer(), pModel->GetIndexBuffer(), pModel->GetIndexCount() );
        }
        m_pCommandList->End();
    }
    else
    {
        m_pCommandList->Begin( params.renderTarget, params.targetStateSrc, params.targetStateDst );
        {
            auto handleRTV = params.hadleRT;
            auto handleDSV = params.hadleDS;

            m_pCommandList->SetTargets( &handleRTV, &handleDSV );

            m_pCommandList->Draw( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, pModel->GetVertexBuffer(), pModel->GetIndexBuffer(), pModel->GetIndexCount() );
        }
        m_pCommandList->End();
    }

    return true;
}
void RenderContext::SetNode( shared_ptr<Node> pNode )
{
    m_pNode = pNode;
}
