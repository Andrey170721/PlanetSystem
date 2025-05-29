// ModelLoader.cpp
#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "WICTextureLoader.h"   // из DirectXTK

ModelLoader::ModelLoader(ID3D11Device* device, ID3D11DeviceContext* context)
    : m_device(device), m_context(context)
{
}

MeshGPU ModelLoader::LoadModel(const std::wstring& modelPath, const std::wstring& texturePath) {
    // 1) Импорт сцены
    Assimp::Importer importer;
    std::string path(modelPath.begin(), modelPath.end());
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GenNormals |
        aiProcess_OptimizeMeshes);

    if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
        throw std::runtime_error("Assimp error: " + std::string(importer.GetErrorString()));

    // 2) Собираем вершины и индексы из всех мешей
    std::vector<Vertex>     vertices;
    std::vector<uint32_t>   indices;
    for (UINT i = 0; i < scene->mNumMeshes; ++i)
        ProcessMesh(scene->mMeshes[i], scene, vertices, indices);

    // 3) Создаём VB
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = UINT(sizeof(Vertex) * vertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initVB{ vertices.data(), 0, 0 };
    ID3D11Buffer* vb = nullptr;
    m_device->CreateBuffer(&bd, &initVB, &vb);

    // 4) Создаём IB
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = UINT(sizeof(uint32_t) * indices.size());
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initIB{ indices.data(), 0, 0 };
    ID3D11Buffer* ib = nullptr;
    m_device->CreateBuffer(&ibd, &initIB, &ib);

    // 5) Загружаем текстуру
    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT hr = DirectX::CreateWICTextureFromFile(
        m_device, m_context, texturePath.c_str(), nullptr, &srv);
    if (FAILED(hr))
        throw std::runtime_error("Failed to load texture");

    // 6) Собираем MeshGPU
    MeshGPU mesh;
    mesh.vb = vb;
    mesh.ib = ib;
    mesh.indexCount = (UINT)indices.size();
    mesh.bsRadius = CalculateBoundingSphere(vertices);
    mesh.texture = srv;
    // sampler будем привязывать централизованно (m_samplerState из MyRender)
    return mesh;
}

void ModelLoader::ProcessMesh(aiMesh* mesh,
    const aiScene* /*scene*/,
    std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices)
{
    UINT baseVert = (UINT)vertices.size();
    // вершины
    for (UINT i = 0; i < mesh->mNumVertices; ++i) {
        Vertex v;
        v.Pos.x = mesh->mVertices[i].x;
        v.Pos.y = mesh->mVertices[i].y;
        v.Pos.z = mesh->mVertices[i].z;
        v.Normal.x = mesh->mNormals[i].x;
        v.Normal.y = mesh->mNormals[i].y;
        v.Normal.z = mesh->mNormals[i].z;
        if (mesh->HasTextureCoords(0))
            v.UV = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
        else
            v.UV = { 0, 0 };
        vertices.push_back(v);
    }
    // индексы
    for (UINT f = 0; f < mesh->mNumFaces; ++f) {
        aiFace& face = mesh->mFaces[f];
        for (UINT j = 0; j < face.mNumIndices; ++j)
            indices.push_back(face.mIndices[j] + baseVert);
    }
}

float ModelLoader::CalculateBoundingSphere(const std::vector<Vertex>& verts) {
    DirectX::XMFLOAT3 center{ 0,0,0 };
    float maxD2 = 0;
    for (auto& v : verts) {
        float d2 = (v.Pos.x * v.Pos.x + v.Pos.y * v.Pos.y + v.Pos.z * v.Pos.z);
        if (d2 > maxD2) maxD2 = d2;
    }
    return sqrtf(maxD2);
}