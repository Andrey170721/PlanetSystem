#include "MyRender.h"
#include <d3dcompiler.h>
#include "WICTextureLoader.h"
#include "ModelLoader.h"

static auto previousTime = std::chrono::high_resolution_clock::now();

struct SimpleVertex {
	Vector3 Pos;
	Vector3 Normal;
	Vector2 UV;
};

struct ConstantBuffer {
	Matrix world;
	Matrix view;
	Matrix projection;
};

// -----------------------------------------------------------------------------
//  Создание ground plane — пара треугольников 200×200 на y=0
// -----------------------------------------------------------------------------
void MyRender::CreatePlane()
{
	const float S = 100.0f;
	SimpleVertex verts[] = {
	  {{-S,0,-S}, {0,1,0}, {0,0}},
	  {{ S,0,-S}, {0,1,0}, {1,0}},
	  {{ S,0, S}, {0,1,0}, {1,1}},
	  {{-S,0, S}, {0,1,0}, {0,1}},
	};
	m_planeVB = CreateVertexBuffer(verts, _countof(verts));

	WORD idx[] = { 0,2,1, 0,3,2 };
	m_planeIB = CreateIndexBuffer(idx, _countof(idx));
	m_planeIndexCount = _countof(idx);
}


MyRender::MyRender()
{
	m_pVertexShader = nullptr;
	m_pPixelShader = nullptr;
	m_pVertexLayout = nullptr;

	m_pIndexBuffer = nullptr;

	constantBuffer = nullptr;
}

HRESULT MyRender::m_compileshaderfromfile(const WCHAR* FileName, LPCSTR EntryPoint, LPCSTR ShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD ShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	ShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DCompileFromFile(FileName,
		nullptr /*macros*/,
		nullptr /*include*/,
		EntryPoint,
		ShaderModel,
		ShaderFlags,
		0,
		ppBlobOut,
		&pErrorBlob);

	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			// Выводим сообщение об ошибке
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
			MessageBoxA(nullptr, (char*)pErrorBlob->GetBufferPointer(),
				"Shader Compile Error", MB_OK | MB_ICONERROR);
			Log::Get()->Err((char*)pErrorBlob->GetBufferPointer());
			pErrorBlob->Release();
		}
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();
	return S_OK;
}

float MyRender::CalculateDeltaTime()
{
	// Замеряем текущее время
	auto currentTime = std::chrono::high_resolution_clock::now();

	// Вычисляем разницу во времени (в секундах) как float
	std::chrono::duration<float> elapsed = currentTime - previousTime;
	float deltaTime = elapsed.count();

	// Запоминаем текущее время, чтобы стать «предыдущим» на след. кадре
	previousTime = currentTime;

	return deltaTime;
}

bool MyRender::Init(HWND hwnd)
{
	HRESULT hr = S_OK;
	ID3DBlob* pVSBlob = NULL;
	hr = m_compileshaderfromfile(L"shader.hlsl", "VSMain", "vs_5_0", &pVSBlob);
	if (FAILED(hr))
	{
		Log::Get()->Err("Невозможно скомпилировать файл shader.hlsl");
		return false;
	}

	hr = m_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pVertexShader);
	if (FAILED(hr))
	{
		_RELEASE(pVSBlob);
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
	  { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,D3D11_INPUT_PER_VERTEX_DATA, 0 },
	  { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,D3D11_INPUT_PER_VERTEX_DATA, 0 },
	  { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	hr = m_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pVertexLayout);
	_RELEASE(pVSBlob);
	if (FAILED(hr))
		return false;

	m_pImmediateContext->IASetInputLayout(m_pVertexLayout);

	ID3DBlob* pPSBlob = NULL;
	hr = m_compileshaderfromfile(L"shader.hlsl", "PSMain", "ps_5_0", &pPSBlob);
	if (FAILED(hr))
	{
		Log::Get()->Err("Невозможно скомпилировать файл shader.hlsl");
		return false;
	}

	hr = m_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPixelShader);
	_RELEASE(pPSBlob);
	if (FAILED(hr))
		return false;


	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(ConstantBuffer);
	cbDesc.Usage = D3D11_USAGE_DEFAULT;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = 0;

	m_pd3dDevice->CreateBuffer(&cbDesc, nullptr, &constantBuffer);

	// —– создаём линейный сэмплер для текстур —–
	D3D11_SAMPLER_DESC sd = {};
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sd.MinLOD = 0;
	sd.MaxLOD = D3D11_FLOAT32_MAX;
	m_pd3dDevice->CreateSamplerState(&sd, &m_samplerState);

	// Загружаем пул моделей
	ModelLoader loader(m_pd3dDevice, m_pImmediateContext);

	auto barrel = loader.LoadModel(L"Models\\Barrel_FBX.fbx", L"Models\\Textures\\barrel_BaseColor.png");

	auto ballMesh = loader.LoadModel(
		L"Models\\soccer_ball.fbx",
		L"Models\\Textures\\soccer_ball_mat_bcolor.png");
	ballMesh.sampler = m_samplerState;
	m_ballMesh = ballMesh;

	barrel.sampler = m_samplerState;  // привязываем сэмплер

	m_modelPool.push_back(barrel);

	DirectX::CreateWICTextureFromFile(
		m_pd3dDevice, m_pImmediateContext,
		L"Models\\Textures\\grass.png",
		nullptr, &m_planeTexture);

	m_World = DirectX::XMMatrixIdentity();

	float width = 1920.0f;
	float height = 1080.0f;
	m_Projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, width / height, 0.01f, 100.0f);

	//g_orbitCam.Init(Vector3(0.0f, 0.0f, 0.0f), -5.0f);
	//m_fpsCamera.Init(Vector3(0.0f, 1.0f, -5.0f), 0.0f, 0.0f);

	m_pImmediateContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pImmediateContext->PSSetShader(m_pPixelShader, NULL, 0);

	CreatePlane();

	//LoadPlaceholderMeshes();   // создаём один юнит-куб и одну юнит-сферу

	SpawnScene();              // раскидываем 150 объектов

// 1) Устанавливаем визуальный радиус и коллизию
	m_ball.visualRadius = 0.8f;
	m_ball.bs = DirectX::BoundingSphere(Vector3(0, m_ball.visualRadius, 0),
		m_ball.visualRadius);

	// ориентация по умолчанию
	m_ball.orientation = Quaternion::Identity;

	// 2) Камера «привязана» к шару, дистанция 20 юнитов
	g_orbitCam.Init(m_ball.bs.Center, 5.0f);

	return true;
}



bool MyRender::Draw()
{
	float dt = CalculateDeltaTime();
	Update();              // старая камера + клавиатура
	UpdateKatamari(dt);    // новая логика

	m_pImmediateContext->PSSetShaderResources(0, 1, &m_planeTexture);
	m_pImmediateContext->PSSetSamplers(0, 1, &m_samplerState);
	RenderObject(m_planeVB, m_planeIB, Matrix::Identity, m_planeIndexCount);

	// --- шар ---
	RenderObject(m_ballMesh, m_ball.world);

	// --- свободные + присоединённые объекты ---
	for (auto& obj : m_objects)
	{
		const Matrix world = obj.attached ? obj.local * m_ball.world
			: obj.local;
		RenderObject(obj.mesh, world);
	}
	return true;
}

void MyRender::Update()
{
	float dt = CalculateDeltaTime();

	// ------ ОБРАБОТКА МЫШИ ------
	static POINT lastMousePos = { 0,0 };

	POINT currentPos;
	GetCursorPos(&currentPos);
	float dx = static_cast<float>(currentPos.x - lastMousePos.x);
	float dy = static_cast<float>(currentPos.y - lastMousePos.y);

	if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
	{
		// вращаем FPS-камеру (dx, dy)
		g_orbitCam.Rotate(dx, -dy);
	}

	lastMousePos = currentPos;

	// ------ ОБРАБОТКА КЛАВИШ ------
	float forward = 0.0f;
	float strafe = 0.0f;
	float upDown = 0.0f;

	// W / S
	if (GetAsyncKeyState('W') & 0x8000)  forward += 1.0f;
	if (GetAsyncKeyState('S') & 0x8000)  forward -= 1.0f;

	// A / D
	if (GetAsyncKeyState('A') & 0x8000)  strafe -= 1.0f;
	if (GetAsyncKeyState('D') & 0x8000)  strafe += 1.0f;

	// Пример подъёма/спуска на пробел / Ctrl (или Shift) — опционально
	if (GetAsyncKeyState(VK_SPACE) & 0x8000)  upDown += 1.0f;
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) upDown -= 1.0f;

	// Масштабируем движение на dt, чтобы скорость не зависела от FPS
	forward *= m_moveSpeed * dt;
	strafe *= m_moveSpeed * dt;
	upDown *= m_moveSpeed * dt;

	// Двигаем камеру
	//m_fpsCamera.Move(forward, strafe, upDown);
}

void MyRender::RenderObject(ID3D11Buffer* vertexBuffer, ID3D11Buffer* indexBuffer, Matrix world, UINT indexCount)
{
	// Обновляем CB, как и раньше
	ConstantBuffer cb;
	cb.world = XMMatrixTranspose(world);
	cb.view = XMMatrixTranspose(g_orbitCam.GetViewMatrix()); // <-- камера
	cb.projection = XMMatrixTranspose(m_Projection);

	m_pImmediateContext->UpdateSubresource(constantBuffer, 0, NULL, &cb, 0, 0);

	m_pImmediateContext->VSSetConstantBuffers(0, 1, &constantBuffer);

	// Привязка вершинного и индексного буфера
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	m_pImmediateContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	m_pImmediateContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Рисуем
	m_pImmediateContext->DrawIndexed(indexCount, 0, 0);
}

void MyRender::RenderObject(MeshGPU mesh, Matrix world)
{
	// Обновляем CB, как и раньше
	ConstantBuffer cb;
	cb.world = XMMatrixTranspose(world);
	cb.view = XMMatrixTranspose(g_orbitCam.GetViewMatrix()); // <-- камера
	cb.projection = XMMatrixTranspose(m_Projection);

	m_pImmediateContext->UpdateSubresource(constantBuffer, 0, NULL, &cb, 0, 0);

	m_pImmediateContext->VSSetConstantBuffers(0, 1, &constantBuffer);

	// Привязка вершинного и индексного буфера
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	m_pImmediateContext->IASetVertexBuffers(0, 1, &mesh.vb, &stride, &offset);
	m_pImmediateContext->IASetIndexBuffer(mesh.ib, DXGI_FORMAT_R32_UINT, 0);

	// привязываем текстуру и сэмплер
	m_pImmediateContext->PSSetShaderResources(0, 1, &mesh.texture);
	m_pImmediateContext->PSSetSamplers(0, 1, &mesh.sampler);

	// Рисуем
	m_pImmediateContext->DrawIndexed(mesh.indexCount, 0, 0);
}

ID3D11Buffer* MyRender::CreateVertexBuffer(const SimpleVertex* vertices, UINT vertexCount)
{
	// Описываем буфер
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * vertexCount;  // Размер под vertexCount вершин
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	// Данные для инициализации
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = vertices;

	// Создаем буфер
	ID3D11Buffer* vBuffer = nullptr;
	HRESULT hr = m_pd3dDevice->CreateBuffer(&bd, &initData, &vBuffer);
	if (FAILED(hr))
	{
		// Обработать ошибку (например, вывести в лог)
		return nullptr;
	}
	return vBuffer;
}

ID3D11Buffer* MyRender::CreateIndexBuffer(const WORD* indices, UINT indexCount)
{
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * indexCount; // Под indexCount индексов
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = indices;

	ID3D11Buffer* iBuffer = nullptr;
	HRESULT hr = m_pd3dDevice->CreateBuffer(&bd, &initData, &iBuffer);
	if (FAILED(hr))
	{
		// Обработать ошибку
		return nullptr;
	}
	return iBuffer;
}

void MyRender::Close()
{
	_RELEASE(m_pIndexBuffer);
	_RELEASE(m_pVertexLayout);
	_RELEASE(m_pVertexShader);
	_RELEASE(m_pPixelShader);
	_RELEASE(constantBuffer);
	_RELEASE(m_planetVB);
	_RELEASE(m_planetIB);
}

#include <random>

//----------------------------------------------------------------------
// 2.  Случайно наполняем сцену объектами-«мусором»
//----------------------------------------------------------------------
void MyRender::SpawnScene() {
	std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> posDist(-50.f, 50.f);
	std::uniform_real_distribution<float> sclDist(0.5f, 1.5f);
	std::uniform_int_distribution<int>    meshDist(0, (int)m_modelPool.size() - 1);

	const int objectCount = 150;
	m_objects.clear();
	m_objects.reserve(objectCount);

	for (int i = 0; i < objectCount; ++i) {
		GameObject obj;
		// берём случайную модель из пула
		obj.mesh = m_modelPool[meshDist(rng)];

		float scale = sclDist(rng);
		float x = posDist(rng);
		float z = posDist(rng);

		obj.bs = DirectX::BoundingSphere(Vector3::Zero, obj.mesh.bsRadius * scale);
		float y = obj.bs.Radius;
		obj.local = Matrix::CreateScale(scale) *
			Matrix::CreateTranslation(x, y, z);
		obj.attached = false;

		m_objects.push_back(obj);
	}
}

//----------------------------------------------------------------------
// 3.  Движение шара + попытка «прилипнуть» объекты
//----------------------------------------------------------------------
void MyRender::UpdateKatamari(float dt)
{
	// 1) получаем WASD
	float forward = 0, strafe = 0;
	if (GetAsyncKeyState('W') & 0x8000) forward += 1;
	if (GetAsyncKeyState('S') & 0x8000) forward -= 1;
	if (GetAsyncKeyState('A') & 0x8000) strafe -= 1;
	if (GetAsyncKeyState('D') & 0x8000) strafe += 1;

	// 2) вычисляем локальные векторы камеры
	Vector3 camPos = g_orbitCam.GetPosition();
	Vector3 toBall = m_ball.bs.Center - camPos;
	toBall.y = 0;
	if (toBall.LengthSquared() > 0) toBall.Normalize();

	// создаём «вверх» и «вперёд» векторы
	Vector3 up(0.0f, 1.0f, 0.0f);
	Vector3 forwardVec = toBall;  // уже нормализован

	Vector3 rightVec = up.Cross(forwardVec);
	rightVec.Normalize();

	// 3) итоговый вектор движения
	Vector3 move = toBall * forward + rightVec * strafe;
	if (move.LengthSquared() > 0) move.Normalize();
	move *= m_moveSpeed * dt;

	// 4) сдвигаем центр шарика (коллизии)
	m_ball.bs.Center.x += move.x;
	m_ball.bs.Center.z += move.z;
	m_ball.bs.Center.y = m_ball.visualRadius;

	// 5) считаем ролл-вращение
	if (move.LengthSquared() > 0.0f)
	{
		Vector3 moveDir = move;
		moveDir.Normalize();

		// Задаём вектор «вверх»
		Vector3 up(0, 1, 0);

		// Ось вращения = up × moveDir  (не moveDir × up!)
		Vector3 spinAxis = up.Cross(moveDir);
		if (spinAxis.LengthSquared() > 0.0f)
			spinAxis.Normalize();

		// Пройденное расстояние = |move|, угол = distance / R
		float travel = move.Length();
		float spinAngle = travel / m_ball.visualRadius; // в радианах

		// Дельта-кватернион по оси spinAxis
		Quaternion delta = Quaternion::CreateFromAxisAngle(spinAxis, spinAngle);

		// Накручиваем новое вращение *после* старого
		m_ball.orientation = m_ball.orientation * delta;
		m_ball.orientation.Normalize();
	}

	// 6) собираем world-матрицу
	float scale = m_ball.bs.Radius / m_ballMesh.bsRadius;
m_ball.world =
    Matrix::CreateScale(scale) *
    Matrix::CreateFromQuaternion(m_ball.orientation) *
    Matrix::CreateTranslation(m_ball.bs.Center);



	// 7) коллизии (как было), но радиус теперь внутри m_ball.bs.Radius
	for (auto& obj : m_objects)
	{
		if (obj.attached) continue;
		DirectX::BoundingSphere objBS = obj.bs;
		objBS.Transform(objBS, obj.local);
		if (objBS.Intersects(m_ball.bs) &&
			objBS.Radius <= m_ball.bs.Radius + 0.05f)
		{
			Attach(obj);
		}
	}

	// 8) обновляем цель камеры
	g_orbitCam.SetTarget(m_ball.bs.Center);
}


//----------------------------------------------------------------------
// 4.  Приклеивание предмета к шару
//----------------------------------------------------------------------
void MyRender::Attach(GameObject& obj)
{
	obj.attached = true;

	// переводим объект в пространство шара
	obj.local = obj.local * Matrix::CreateTranslation(-m_ball.bs.Center.x,
		-m_ball.bs.Center.y,
		-m_ball.bs.Center.z
	);

	// растим катамари
	m_ball.bs.Radius += obj.bs.Radius * m_ball.growRate;

	// поднять центр, пересобрать world-матрицу
	/*m_ball.bs.Center.y = m_ball.bs.Radius;
	m_ball.world = Matrix::CreateScale(m_ball.bs.Radius) *
		Matrix::CreateTranslation(m_ball.bs.Center);*/
}