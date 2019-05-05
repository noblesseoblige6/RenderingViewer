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