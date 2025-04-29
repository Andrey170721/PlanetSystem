// ModelLoader.cpp
#include "ModelLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <WICTextureLoader.h> // из DirectXTK, для загрузки текстур

ModelLoader::ModelLoader(ID3D11Device* dev, ID3D11DeviceContext* ctx)
    : m_dev(dev), m_ctx(ctx) {
}

std::vector<MeshGPU> ModelLoader::LoadModel(const std::wstring& filePath)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        std::string(filePath.begin(), filePath.end()),
        aiProcess_Triangulate |
        aiProcess_CalcTangentSpace |
        aiProcess_ConvertToLeftHanded
    );
    if (!scene || !scene->HasMeshes()) return {};

    // Базовый путь для текстур
    wchar_t drive[_MAX_DRIVE], dir[_MAX_DIR];
    _wsplitpath_s(filePath.c_str(), drive, dir, nullptr, 0, nullptr, 0, nullptr, 0);
    std::wstring baseDir = std::wstring(drive) + std::wstring(dir);

    std::vector<MeshGPU> result;
    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        aiMesh* mesh = scene->mMeshes[m];
        std::vector<TexturedVertex> verts(mesh->mNumVertices);
        for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
            verts[i].Pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            verts[i].Normal = { mesh->mNormals[i].x,  mesh->mNormals[i].y,  mesh->mNormals[i].z };
            if (mesh->HasTextureCoords(0))
                verts[i].UV = { mesh->mTextureCoords[0][i].x,
                                mesh->mTextureCoords[0][i].y };
            else
                verts[i].UV = { 0,0 };
        }
        // Индексы
        std::vector<UINT> idx;
        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            aiFace& face = mesh->mFaces[f];
            for (unsigned j = 0; j < face.mNumIndices; ++j)
                idx.push_back(face.mIndices[j]);
        }

        // Создаём VB
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(TexturedVertex) * (UINT)verts.size();
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init = { verts.data(), 0,0 };
        ID3D11Buffer* vb = nullptr;
        m_dev->CreateBuffer(&bd, &init, &vb);

        // Создаём IB
        bd.ByteWidth = sizeof(UINT) * (UINT)idx.size();
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        init.pSysMem = idx.data();
        ID3D11Buffer* ib = nullptr;
        m_dev->CreateBuffer(&bd, &init, &ib);

        // Загружаем текстуру материала
        ID3D11ShaderResourceView* srv = nullptr;
        if (scene->HasMaterials()) {
            aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            aiString texPath;
            if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                std::wstring full = baseDir + std::wstring(texPath.C_Str(), texPath.length);
                // CreateWICTextureFromFile из DirectXTK
                DirectX::CreateWICTextureFromFile(m_dev, m_ctx,
                    full.c_str(), nullptr, &srv);
            }
        }

        MeshGPU gpu;
        gpu.vb = vb;
        gpu.ib = ib;
        gpu.indexCount = (UINT)idx.size();
        gpu.texture = srv;
        result.push_back(gpu);
    }
    return result;
}
