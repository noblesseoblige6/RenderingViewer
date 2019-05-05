Model::Model( ID3D12Device* pDevice )
    : Node( pDevice )
    , m_indexCount( 0 )
    , m_sourcePath("")
{
    m_nodeType = NODE_TYPE_MODEL;
}

Model::~Model()
{
    Release();
}

void Model::Release()
{
}

bool Model::BindAsset( ID3D12Device* pDevice, const string& sourcePath )
{
    acObjLoader loader;
    acModelLoader::LoadOption option;
    loader.SetLoadOption( option );

    m_sourcePath = sourcePath;
    loader.Load( m_sourcePath );

    CreateVertexBuffer( pDevice, loader );
    
    CreateIndexBuffer( pDevice, loader );

    CreateBoundingBox(loader);

    CreateMaterial( pDevice );

    return true;
}

bool Model::BindDescriptorHeap( ID3D12Device* pDevice, shared_ptr<DescriptorHeap> pDescHeap )
{
    m_pMaterialCB->CreateBufferView( pDevice, pDescHeap, Buffer::BUFFER_VIEW_TYPE_CONSTANT);

    return true;
}

void Model::CreateVertexBuffer( ID3D12Device* pDevice, const acObjLoader& loader )
{
    vector<Vertex> vertices;
    for (int i = 0; i < loader.GetVertexCount(); ++i)
    {
        Vertex v;
        v.position = loader.GetVertex( i );

        if (i < loader.GetNormalCount())
            v.normal = loader.GetNormal( i );

        if (i < loader.GetTexCoordCount())
            v.texCoord = loader.GetTexCoord( i );

        // TODO: Vertex color
        //if (i < loader.GetColor())
        //    v.color = loader.GetColor( i );

        vertices.push_back( v );
    }

    int vertexSize = static_cast<int>(sizeof( Vertex ) * vertices.size());

    m_pVertexBuffer = make_shared<VertexBuffer>();
    m_pVertexBuffer->SetDataStride( sizeof( Vertex ) );
    m_pVertexBuffer->Create( pDevice, vertexSize );
    m_pVertexBuffer->CreateBufferView( pDevice, nullptr, Buffer::BUFFER_VIEW_TYPE_VERTEX );
    m_pVertexBuffer->Map( &vertices[0], vertexSize );
    m_pVertexBuffer->Unmap();
}

void Model::CreateIndexBuffer( ID3D12Device* pDevice, const acObjLoader& loader )
{
    m_indexCount = loader.GetIndexCount();

    vector<unsigned short> indices;
    for (int i = 0; i < loader.GetIndexCount(); ++i)
    {
        unsigned short index;
        index = static_cast<unsigned short>(loader.GetIndex( i ));

        indices.push_back( index );
    }

    int indexSize = static_cast<int>(sizeof( unsigned short ) * indices.size());

    m_pIndexBuffer = make_shared<IndexBuffer>();
    m_pIndexBuffer->SetDataFormat( DXGI_FORMAT_R16_UINT );
    m_pIndexBuffer->Create( pDevice, indexSize );
    m_pIndexBuffer->CreateBufferView( pDevice, nullptr, Buffer::BUFFER_VIEW_TYPE_INDEX );
    m_pIndexBuffer->Map( &indices[0], indexSize );
    m_pIndexBuffer->Unmap();
}

void Model::CreateBoundingBox( const acObjLoader& loader )
{
    Vec3f& maxPoint = m_boundingBox.maxPoint;
    Vec3f& minPoint = m_boundingBox.minPoint;

    for (int i = 0; i < loader.GetVertexCount(); ++i)
    {
        const Vec3f& pos = loader.GetVertex( i );

        maxPoint.x = max( pos.x, maxPoint.x );
        maxPoint.y = max( pos.y, maxPoint.y );
        maxPoint.z = max( pos.z, maxPoint.z );

        minPoint.x = min( pos.x, maxPoint.x );
        minPoint.y = min( pos.y, maxPoint.y );
        minPoint.z = min( pos.z, maxPoint.z );
    }
}

void Model::CreateMaterial( ID3D12Device* pDevice )
{
    m_pMaterialCB = make_shared<ConstantBuffer>();
    m_pMaterialCB->Create( pDevice, sizeof(ResMaterialData) );

    // 定数バッファデータの設定.
    m_materialData.size = sizeof( ResMaterialData );
    m_materialData.ka = Vec4f::ZERO;
    m_materialData.kd = Vec4f( 0.5f );
    m_materialData.ks = Vec4f( 1.0f, 1.0f, 1.0, 50.0f );

    m_pMaterialCB->Map( &m_materialData, sizeof( m_materialData ) );

}
