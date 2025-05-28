//// ModelLoader.h
#pragma once
#include <d3d11.h>
#include <string>
#include "GameObject.h"

struct aiMesh;
struct aiScene;

// ¬ершина должна совпадать с входом вашего шейдера (POSITION, TEXCOORD0, NORMAL)
struct Vertex {
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 UV;
};

class ModelLoader {
public:
    ModelLoader(ID3D11Device* device, ID3D11DeviceContext* context);
    // modelPath Ч .obj/.fbx, texturePath Ч путь до диффузной текстуры
    MeshGPU LoadModel(const std::wstring& modelPath, const std::wstring& texturePath);

private:
    void ProcessMesh(aiMesh* mesh,
        const aiScene* scene,
        std::vector<Vertex>& vertices,
        std::vector<uint32_t>& indices);

    float CalculateBoundingSphere(const std::vector<Vertex>& verts);

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
};
