Node::Node( ID3D12Device* pDevice )
{
    AC_USE_VAR( pDevice );
    m_nodeType = NODE_TYPE_NODE;
}


Node::~Node()
{
}

void Node::AddChild( shared_ptr<Node> pNode )
{
    m_pChildren.push_back( pNode );
}

void Node::RemoveChild( shared_ptr<Node> pNode )
{
    for (int i = 0; i < m_pChildren.size(); ++i)
    {
        if (pNode == m_pChildren[i])
        {
            RemoveChild( i );
            break;
        }
    }
}

void Node::RemoveChild( int index )
{
    m_pChildren.erase( m_pChildren.begin() + index );
}

bool Node::BindDescriptorHeap( ID3D12Device* pDevice, shared_ptr<DescriptorHeap> pDescHeap )
{
    AC_USE_VAR( pDevice );
    AC_USE_VAR( pDescHeap );

    return true;
}

void Node::UpdateGPUBuffer()
{
}

bool Node::CreateCB()
{
    return true;
}
