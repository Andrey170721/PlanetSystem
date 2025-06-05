#pragma once

#include "D3D11_Framework.h"
#include <directxmath.h>
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include "OrbitCamera.h"
#include "FpsCamera.h"
#include "GameObject.h"
#include <iostream>
#include <vector>
#include <chrono>

constexpr int MAX_POINT_LIGHTS = 32;

using namespace D3D11Framework;
using namespace DirectX::SimpleMath;
struct SimpleVertex;

struct Light {
	DirectX::XMFLOAT3 direction;
	float              pad1;
	DirectX::XMFLOAT3 color;
	float              pad2;
};

struct Material {
	DirectX::XMFLOAT3 ambient;
	float              pad1;
	DirectX::XMFLOAT3 diffuse;
	float              pad2;
	DirectX::XMFLOAT3 specular;
	float              specPower;
};

struct PointLight
{
	DirectX::XMFLOAT3 position; float range;      // (w) Ц радиус затухани€
	DirectX::XMFLOAT3 color;    float intensity;  // (w) Ц сила
};

struct LightBufferType
{
	Light             dirLight;   // 32
	Material          mat;        // 48
	DirectX::XMFLOAT3 viewPos; float pad0;  // 16  ?  96

	int               pointCount;
	float             padPoint[3];          // добиваем до 16  ? 112

	PointLight        pLights[MAX_POINT_LIGHTS]; // 32*32 = 1024
	// 112 + 1024 = **1136**
};

class MyRender : public Render
{
public:
	MyRender();
	bool Init(HWND hwnd);
	bool Draw();
	void Update();
	void    SpawnScene();       // разместить модели случайно
	void    LoadPlaceholderMeshes(); // временно сферы/кубы, пока нет real-fbx
	void    UpdateKatamari(float dt);
	void    Attach(GameObject& obj);
	void RenderObject(ID3D11Buffer* vertexBuffer, ID3D11Buffer* indexBuffer, Matrix world, UINT indexCount);
	void RenderObject(MeshGPU mesh, Matrix world);
	ID3D11Buffer* CreateVertexBuffer(const SimpleVertex* vertices, UINT vertexCount);
	ID3D11Buffer* CreateIndexBuffer(const WORD* indices, UINT indexCount);
	void Close();
	DirectX::XMFLOAT3 HSVtoRGB(const DirectX::XMFLOAT3& hsv);

	void GenerateSphere(float radius, unsigned int slices, unsigned int stacks, std::vector<SimpleVertex>& outVertices, std::vector<WORD>& outIndices);

	void* operator new(size_t i)
	{
		return _aligned_malloc(i, 16);
	}

	void operator delete(void* p)
	{
		_aligned_free(p);
	}
private:
	HRESULT m_compileshaderfromfile(const WCHAR* FileName, LPCSTR EntryPoint, LPCSTR ShaderModel, ID3DBlob** ppBlobOut);
	static float CalculateDeltaTime();

	ID3D11InputLayout* m_pVertexLayout;
	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader* m_pPixelShader;

	ID3D11Buffer* m_pIndexBuffer;
	ID3D11Buffer* constantBuffer;

	Matrix m_World;
	Matrix m_View;
	Matrix m_Projection;

	ID3D11Buffer* m_planetVB = nullptr;
	ID3D11Buffer* m_planetIB = nullptr;
	UINT m_sphereIndexCount = 0;

	OrbitCamera g_orbitCam;

	std::vector<SimpleVertex> sphereVerts;
	std::vector<WORD> sphereIdx;

    std::vector<Matrix> m_objectWorlds{9};
	int m_currentObject = 0;

	FpsCamera m_fpsCamera;

	std::vector<MeshGPU>      m_placeholders;   // простые примитивы
	std::vector<GameObject>   m_objects;        // всЄ, что лежит на полу
	KatamariBall              m_ball;
	float                     m_moveSpeed = 8.f;

	ID3D11Buffer* m_planeVB = nullptr;
	ID3D11Buffer* m_planeIB = nullptr;

	std::vector<MeshGPU> m_modelPool;
	ID3D11SamplerState* m_samplerState = nullptr;
	MeshGPU    m_ballMesh;
	ID3D11ShaderResourceView* m_planeTexture;

	UINT            m_planeIndexCount = 0;
	void            CreatePlane();

	ID3D11Buffer* m_lightBuffer = nullptr;
	LightBufferType lb = {};
};