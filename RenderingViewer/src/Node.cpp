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
    return true;
}

void Node::UpdateGPUBuffer()
{
}

bool Node::CreateCB()
{
    return true;
}

Camera::Camera( ID3D12Device* pDevice )
    : Node( pDevice )
{
    m_nodeType = NODE_TYPE_CAMERA;

    CreateCB( pDevice );
}

Camera::~Camera()
{
}

void Camera::CreateViewMatrix()
{
    m_viewMatrix = Mat44f::CreateLookAt( m_position, m_lookAt, Vec3f::YAXIS );
}

bool Camera::BindDescriptorHeap( ID3D12Device* pDevice, shared_ptr<DescriptorHeap> pDescHeap )
{
    m_pCameraCB->CreateBufferView( pDevice, pDescHeap, Buffer::BUFFER_VIEW_TYPE_CONSTANT );

    return true;
}

void Camera::UpdateGPUBuffer()
{
    m_transformBufferData.world      = Mat44f::IDENTITY;
    m_transformBufferData.view       = m_viewMatrix;
    m_transformBufferData.projection = m_projectionMatrix;

    m_pCameraCB->Map( &m_transformBufferData, sizeof( m_transformBufferData ) );
}

bool Camera::CreateCB( ID3D12Device* pDevice )
{
    m_pCameraCB = make_shared<ConstantBuffer>();
    m_pCameraCB->Create( pDevice, sizeof( ResTransformBuffer ) );

    m_transformBufferData.size       = sizeof( ResTransformBuffer );

    UpdateGPUBuffer();

    return true;
}

Light::Light( ID3D12Device* pDevice )
    : Node( pDevice )
{
    m_nodeType = NODE_TYPE_LIGHT;

    CreateCB( pDevice );
}

Light::~Light()
{
}

bool Light::BindDescriptorHeap( ID3D12Device* pDevice, shared_ptr<DescriptorHeap> pDescHeap )
{
    m_pLightCB->CreateBufferView( pDevice, pDescHeap, Buffer::BUFFER_VIEW_TYPE_CONSTANT );

    return true;
}

void Light::UpdateGPUBuffer()
{
    // 定数バッファデータの設定.
    m_lightBufferData.size = sizeof( ResLightData );
    m_lightBufferData.position[0] = Vec4f( 0.0f, 5.0f, 0.0f, 1.0f );
    m_lightBufferData.color[0] = Vec4f( 1.0f, 1.0f, 1.0f, 1.0f );
    Vec3f lightDir = Vec3f( 0.0f, -1.0f, -1.0f );
    lightDir.normalized();
    m_lightBufferData.direction[0] = lightDir;
    m_lightBufferData.intensity[0] = Vec3f::ONE;

    Vec3f position = Vec3f( m_lightBufferData.position[0].x, m_lightBufferData.position[0].y, m_lightBufferData.position[0].z );
    Vec3f dir = m_lightBufferData.direction[0];

    Mat44f viewMatrix = Mat44f::CreateLookAt( position, dir, Vec3f::YAXIS );

    float w = 1.86523065f * 5;
    float h = 1.86523065f * 5;

    Mat44f projectionMatrix = Mat44f::CreateOrthoLH( -0.5f*w, 0.5f*w, -0.5f*h, 0.5f*h, 1.0f, 100.0f );
    m_lightBufferData.view[0] = viewMatrix;
    m_lightBufferData.projection[0] = projectionMatrix;

    m_pLightCB->Map( &m_lightBufferData, sizeof( m_lightBufferData ) );
}

bool Light::CreateCB( ID3D12Device* pDevice )
{
    m_pLightCB = make_shared<ConstantBuffer>();
    m_pLightCB->Create( pDevice, sizeof( ResLightData ) );

    UpdateGPUBuffer();

    return true;
}