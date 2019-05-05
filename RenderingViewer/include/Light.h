#pragma once

class Light : public Node
{
public:
    struct ResLightData
    {
        // Directinal Light
        static const int DIRECTIONAL_LIGHT_NUM = 1;
        Vec4f position[DIRECTIONAL_LIGHT_NUM];
        Vec4f color[DIRECTIONAL_LIGHT_NUM];

        Mat44f view[DIRECTIONAL_LIGHT_NUM];
        Mat44f projection[DIRECTIONAL_LIGHT_NUM];

        Vec3f direction[DIRECTIONAL_LIGHT_NUM];
        Vec3f intensity[DIRECTIONAL_LIGHT_NUM];
        //Mat44f lightVP[DIRECTIONAL_LIGHT_NUM];

        DWORD size;
    };

public:
    Light( ID3D12Device* pDevice );
    ~Light();

public:
    virtual bool BindDescriptorHeap( ID3D12Device* pDevice, shared_ptr<DescriptorHeap> pDescHeap );

    virtual void UpdateGPUBuffer();

public:
    ResLightData & GetBufferData() { return m_lightBufferData; }
    const ResLightData& GetBufferData() const { return m_lightBufferData; }

protected:
    virtual bool CreateCB( ID3D12Device* pDevice );

private:
    ResLightData               m_lightBufferData;
    shared_ptr<ConstantBuffer> m_pLightCB;
};
