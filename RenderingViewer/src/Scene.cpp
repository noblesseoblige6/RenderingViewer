Scene::Scene( ID3D12Device *pDevice )
{
    m_pRootNode = make_shared<Node>( pDevice );
}


Scene::~Scene()
{
}
