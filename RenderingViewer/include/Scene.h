#pragma once

class Scene
{
public:
    Scene(ID3D12Device *pDevice);
    ~Scene();

public:
    shared_ptr<Node> GetRootNode() const { return m_pRootNode; }

private:
    shared_ptr<Node> m_pRootNode;
};
