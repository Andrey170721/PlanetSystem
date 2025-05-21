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

using namespace D3D11Framework;
using namespace DirectX::SimpleMath;
struct SimpleVertex;

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
	void RenderObject(
		ID3D11Buffer* vb,
		ID3D11Buffer* ib,
		ID3D11ShaderResourceView* texSRV,
		bool useGradient,
		const DirectX::SimpleMath::Matrix& world,
		UINT indexCount);
	ID3D11Buffer* CreateVertexBuffer(const SimpleVertex* vertices, UINT vertexCount);
	ID3D11Buffer* CreateIndexBuffer(const WORD* indices, UINT indexCount);
	void Close();

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
	std::vector<GameObject>   m_objects;        // всё, что лежит на полу
	KatamariBall              m_ball;
	float                     m_moveSpeed = 8.f;

	ID3D11Buffer* m_planeVB = nullptr;
	ID3D11Buffer* m_planeIB = nullptr;
	UINT            m_planeIndexCount = 0;
	void            CreatePlane();

	ID3D11PixelShader* m_pPSGradient = nullptr;
	ID3D11PixelShader* m_pPSTextured = nullptr;
	ID3D11Buffer* m_gradCB = nullptr;  // буфер gradBottom/top/height
	float              m_gradHeight = 100.0f;    // например размер плоскости
	DirectX::XMFLOAT4             m_gradBottomColor = {0.1f,0.3f,0.1f,1}; // тёмно-зелёный
	DirectX::XMFLOAT4             m_gradTopColor = { 0.8f,1.0f,0.8f,1 }; // светло-зелёный
	ID3D11SamplerState* m_samplerState = nullptr;
};