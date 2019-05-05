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
