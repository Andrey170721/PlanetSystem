// ModelLoader.h
#pragma once
#include <vector>
#include <string>
#include <SimpleMath.h>
#include <d3d11.h>

struct TexturedVertex {
    DirectX::SimpleMath::Vector3 Pos;
    DirectX::SimpleMath::Vector3 Normal;
    DirectX::SimpleMath::Vector2 UV;
};

struct MeshGPU {
    ID3D11Buffer* vb = nullptr;
    ID3D11Buffer* ib = nullptr;
    UINT                       indexCount = 0;
    ID3D11ShaderResourceView* texture = nullptr;
};

class ModelLoader {
public:
    // device/context ��� �������� ������� � �������
    ModelLoader(ID3D11Device* dev, ID3D11DeviceContext* ctx);
    // ��������� .obj ��� .fbx � ���������� ������ �����
    std::vector<MeshGPU> LoadModel(const std::wstring& filePath);
private:
    ID3D11Device* m_dev;
    ID3D11DeviceContext* m_ctx;
};
