#include "..\include\Model.h"
Model::Model()
    : m_indexCount( 0 )
    , m_sourcePath("")
{
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
    m_pVertexBuffer->Create( pDevice, prop, desc, D3D12_RESOURCE_STATE_GENERIC_READ );
    m_pVertexBuffer->CreateBufferView( pDevice, desc, nullptr, Buffer::BUFFER_VIEW_TYPE_VERTEX );
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
    m_pIndexBuffer->SetDataFormat( DXGI_FORMAT_R16_UINT );
    m_pIndexBuffer->Create( pDevice, prop, desc, D3D12_RESOURCE_STATE_GENERIC_READ );
    m_pIndexBuffer->CreateBufferView( pDevice, desc, nullptr, Buffer::BUFFER_VIEW_TYPE_INDEX );
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
