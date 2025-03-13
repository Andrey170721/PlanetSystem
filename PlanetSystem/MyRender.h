#pragma once

#include "D3D11_Framework.h"
#include <directxmath.h>
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include "OrbitCamera.h"
#include "FpsCamera.h"
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
	void RenderObject(ID3D11Buffer* vertexBuffer, ID3D11Buffer* indexBuffer, Matrix world, UINT indexCount);
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
	void SetPlanetCount(int count);

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
	float m_moveSpeed = 10.0f;

	int m_planetCount = 0;

	std::vector<float> m_planetDistances;
	std::vector<float> m_orbitSpeeds;

	std::vector<float> m_planetScales;
};