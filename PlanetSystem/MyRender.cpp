#include "MyRender.h"
#include <d3dcompiler.h>
#include "WICTextureLoader.h"
#include "ModelLoader.h"
#include <DirectXMath.h>
using namespace DirectX;

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
	hr = m_compileshaderfromfile(L"shader1.hlsl", "VSMain", "vs_5_0", &pVSBlob);
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
	hr = m_compileshaderfromfile(L"shader1.hlsl", "PSMain", "ps_5_0", &pPSBlob);
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

	// создаём буфер для LightBufferType
	D3D11_BUFFER_DESC lbDesc = {};
	lbDesc.Usage = D3D11_USAGE_DEFAULT;
	lbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	// гарантированно кратно 16
	lbDesc.ByteWidth = sizeof(LightBufferType);          // теперь 1136
	lbDesc.ByteWidth = (lbDesc.ByteWidth + 15) & ~15;    // запас на всякий случай

	hr = m_pd3dDevice->CreateBuffer(&lbDesc, nullptr, &m_lightBuffer);
	if (FAILED(hr))
	{
		MessageBoxA(hwnd, "Failed to create light constant buffer", "D3D11 error", MB_OK);
		return false;
	}

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
	//auto chair = loader.LoadModel(L"Models\\old_chair.fbx", L"Models\\Textures\\old_chair_bcolor.png");
	//auto pic_table = loader.LoadModel(L"Models\\picnic_table.fbx", L"Models\\Textures\\picnic_table_mat_bcolor.png");
	//auto sword = loader.LoadModel(L"Models\\SlothSword.fbx", L"Models\\Textures\\Material_BaseColor.png");
	auto table = loader.LoadModel(L"Models\\table.fbx", L"Models\\Textures\\wooden_top_bcolor.png");

	auto ballMesh = loader.LoadModel(
		L"Models\\soccer_ball.fbx",
		L"Models\\Textures\\soccer_ball_mat_bcolor.png");
	ballMesh.sampler = m_samplerState;
	m_ballMesh = ballMesh;

	barrel.sampler = m_samplerState;  // привязываем сэмплер
	//chair.sampler = m_samplerState;
	//pic_table.sampler = m_samplerState;
	//sword.sampler = m_samplerState;
	table.sampler = m_samplerState;

	m_modelPool.push_back(barrel);
	//m_modelPool.push_back(chair);
	//m_modelPool.push_back(pic_table);
	//m_modelPool.push_back(sword);
	m_modelPool.push_back(table);

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

	// направленный свет «сверху»:
	lb.dirLight.direction = { -0.5f, -1.0f, -0.3f };
	lb.dirLight.color = { 1.0f, 1.0f, 1.0f };

	lb.pointCount = MAX_POINT_LIGHTS;
	for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
	{
		float ang = XM_2PI * i / MAX_POINT_LIGHTS;
		lb.pLights[i].range = 1.2f;
		lb.pLights[i].intensity = 1.0f;

		// пастельная «радуга» вокруг мяча
		DirectX::XMFLOAT3 hsv = DirectX::XMFLOAT3(i / (float)MAX_POINT_LIGHTS, 0.6f, 1.0f);
		DirectX::XMFLOAT3 rgb = HSVtoRGB(hsv);
		lb.pLights[i].color = { rgb.x, rgb.y, rgb.z };
	}


	// материалы:
	lb.mat.ambient = { 0.1f, 0.1f, 0.1f };
	lb.mat.diffuse = { 1.0f, 1.0f, 1.0f };
	lb.mat.specular = { 1.0f, 1.0f, 1.0f };
	lb.mat.specPower = 32.0f;

	CreatePlane();

	//LoadPlaceholderMeshes();   // создаём один юнит-куб и одну юнит-сферу

	SpawnScene();              // раскидываем 150 объектов

// 1) Устанавливаем визуальный радиус и коллизию
	m_ball.visualRadius = 0.8f;
	m_ball.bs = DirectX::BoundingSphere(Vector3(0, m_ball.visualRadius, 0),
		m_ball.visualRadius);

	m_ball.yVelocity = 0.0f;
	m_ball.jumpsRemaining = 2;
	m_ball.apexWindowActive = false;
	m_ball.apexTimer = 0.0f;

	// ориентация по умолчанию
	m_ball.orientation = Quaternion::Identity;

	// 2) Камера «привязана» к шару, дистанция 20 юнитов
	g_orbitCam.Init(m_ball.bs.Center, 5.0f);

	return true;
}



bool MyRender::Draw()
{
	// получаем камеру из вашего OrbitCamera
	auto camPos = g_orbitCam.GetPosition();
	lb.viewPos = DirectX::XMFLOAT3(camPos.x, camPos.y, camPos.z);

	XMFLOAT3 ball = { m_ball.bs.Center.x,
				  m_ball.bs.Center.y,
				  m_ball.bs.Center.z };

	for (int i = 0; i < lb.pointCount; ++i)
	{
		float ang = XM_2PI * i / lb.pointCount;
		float ringR = m_ball.visualRadius * 2.0f;        // радиус кольца огней
		lb.pLights[i].position = {
			ball.x + cosf(ang) * ringR,
			ball.y + 0.3f,                               // чуть выше пола
			ball.z + sinf(ang) * ringR
		};
	}

	// записываем в GPU
	m_pImmediateContext->UpdateSubresource(m_lightBuffer, 0, nullptr, &lb, 0, 0);

	// привязываем в слот b1
	m_pImmediateContext->PSSetConstantBuffers(1, 1, &m_lightBuffer);

	float dt = CalculateDeltaTime();
	Update();              // старая камера + клавиатура
	UpdateKatamari(dt);    // новая логика

	m_pImmediateContext->PSSetShaderResources(0, 1, &m_planeTexture);
	m_pImmediateContext->PSSetSamplers(0, 1, &m_samplerState);
	m_pImmediateContext->PSSetShader(m_pPixelShader, nullptr, 0);
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
	std::uniform_real_distribution<float> sclDist(0.3f, 0.8f);
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

		auto rot = Matrix::CreateRotationX(-XM_PIDIV2);

		obj.local = Matrix::CreateScale(scale) * rot *
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
	// ───────────────────────────────────────────────────────
	// 0. Ввод: детектируем нажатие «пробел» по фронту
	// ───────────────────────────────────────────────────────
	static bool prevSpace = false;
	bool  spaceNow = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
	bool  spaceEdge = spaceNow && !prevSpace;   // нажат прямо сейчас
	prevSpace = spaceNow;

	// ───────── константы «физики» (подберите по вкусу) ────────
	const float g = -25.0f;   // ускорение свободного падения
	const float jumpImpulse = 10.0f;    // начальная V-y прыжка
	const float apexWindow = 0.25f;    // время, когда разрешён 2-ой прыжок

	// ───────────────────────────────────────────────────────
	// 1. Горизонтальное движение (как было раньше)
	// ───────────────────────────────────────────────────────
	float forward = 0, strafe = 0;
	if (GetAsyncKeyState('W') & 0x8000) forward += 1;
	if (GetAsyncKeyState('S') & 0x8000) forward -= 1;
	if (GetAsyncKeyState('A') & 0x8000) strafe -= 1;
	if (GetAsyncKeyState('D') & 0x8000) strafe += 1;

	Vector3 camPos = g_orbitCam.GetPosition();
	Vector3 toBall = m_ball.bs.Center - camPos;  toBall.y = 0;
	if (toBall.LengthSquared() > 0) toBall.Normalize();

	Vector3 rightVec = Vector3::UnitY.Cross(toBall);  rightVec.Normalize();

	Vector3 move = toBall * forward + rightVec * strafe;
	if (move.LengthSquared() > 0) { move.Normalize(); move *= m_moveSpeed * dt; }

	m_ball.bs.Center.x += move.x;
	m_ball.bs.Center.z += move.z;

	// ───────────────────────────────────────────────────────
	// 2. Прыжки и вертикальная физика
	// ───────────────────────────────────────────────────────
	// 2.1 Запрос прыжка
	if (spaceEdge)
	{
		bool grounded = (m_ball.bs.Center.y <= m_ball.visualRadius + 0.001f);

		if (grounded && m_ball.jumpsRemaining == 2)          // 1-ый прыжок
		{
			m_ball.yVelocity = jumpImpulse;
			m_ball.jumpsRemaining = 1;
		}
		else if (m_ball.apexWindowActive && m_ball.jumpsRemaining == 1) // 2-ой
		{
			m_ball.yVelocity = jumpImpulse;
			m_ball.jumpsRemaining = 0;
			m_ball.apexWindowActive = false;
		}
	}

	// 2.2 Применяем гравитацию
	m_ball.yVelocity += g * dt;
	m_ball.bs.Center.y += m_ball.yVelocity * dt;

	// 2.3 Детект апекса (смена направления движения вверх/вниз)
	if (!m_ball.apexWindowActive && m_ball.jumpsRemaining == 1 &&
		m_ball.yVelocity <= 0.0f)      // достиг вершины
	{
		m_ball.apexWindowActive = true;
		m_ball.apexTimer = 0.0f;
	}

	// 2.4 Отсчитываем окно для второго прыжка
	if (m_ball.apexWindowActive)
	{
		m_ball.apexTimer += dt;
		if (m_ball.apexTimer > apexWindow)
			m_ball.apexWindowActive = false;
	}

	// 2.5 Столкновение с землёй
	if (m_ball.bs.Center.y < m_ball.visualRadius)
	{
		m_ball.bs.Center.y = m_ball.visualRadius;
		m_ball.yVelocity = 0.0f;
		m_ball.jumpsRemaining = 2;          // снова разрешены оба прыжка
		m_ball.apexWindowActive = false;
	}

	// ───────────────────────────────────────────────────────
	// 3. Вращение катамари и world-матрица (как было)
	// ───────────────────────────────────────────────────────
	if (move.LengthSquared() > 0.0f)
	{
		Vector3 moveDir = move;  moveDir.Normalize();
		Vector3 spinAxis = Vector3::UnitY.Cross(moveDir);   spinAxis.Normalize();

		float spinAngle = move.Length() / m_ball.visualRadius;
		Quaternion delta = Quaternion::CreateFromAxisAngle(spinAxis, spinAngle);

		m_ball.orientation = m_ball.orientation * delta;
		m_ball.orientation.Normalize();
	}

	float constantScale = m_ball.visualRadius / m_ballMesh.bsRadius;
	m_ball.world =
		Matrix::CreateScale(constantScale) *
		Matrix::CreateFromQuaternion(m_ball.orientation) *
		Matrix::CreateTranslation(m_ball.bs.Center);

	// ───────────────────────────────────────────────────────
	// 4. «Приклеиваем» объекты (без изменений)
	// ───────────────────────────────────────────────────────
	for (auto& obj : m_objects)
	{
		if (obj.attached) continue;
		BoundingSphere objBS = obj.bs;  objBS.Transform(objBS, obj.local);
		if (objBS.Intersects(m_ball.bs))
			Attach(obj);
	}

	// камера продолжает следить за центром мяча
	g_orbitCam.SetTarget(m_ball.bs.Center);
}



//----------------------------------------------------------------------
// 4.  Приклеивание предмета к шару
//----------------------------------------------------------------------
void MyRender::Attach(GameObject& obj)
{
	// 1. Отмечаем, что объект приклеен
	obj.attached = true;
	
	// 2. Вычисляем обратную матрицу мира шара
	//    (в SimpleMath есть метод Invert()):
	Matrix invBallWorld = m_ball.world.Invert();
	
	// 3. Переводим текущую world-матрицу объекта
	//    в локальные координаты шара
	obj.local = obj.local * invBallWorld;

	// растим катамари
	//m_ball.bs.Radius += obj.bs.Radius * m_ball.growRate;

	// поднять центр, пересобрать world-матрицу
	/*m_ball.bs.Center.y = m_ball.bs.Radius;
	m_ball.world = Matrix::CreateScale(m_ball.bs.Radius) *
		Matrix::CreateTranslation(m_ball.bs.Center);*/
}

DirectX::XMFLOAT3 MyRender::HSVtoRGB(const DirectX::XMFLOAT3& hsv)
{
	float H = hsv.x * 360.0f;  // [0,1] → [0,360]
	float S = hsv.y;
	float V = hsv.z;

	float C = V * S;
	float X = C * (1.0f - fabsf(fmodf(H / 60.0f, 2.0f) - 1.0f));
	float m = V - C;

	float r, g, b;

	if (H < 60) { r = C; g = X; b = 0; }
	else if (H < 120) { r = X; g = C; b = 0; }
	else if (H < 180) { r = 0; g = C; b = X; }
	else if (H < 240) { r = 0; g = X; b = C; }
	else if (H < 300) { r = X; g = 0; b = C; }
	else { r = C; g = 0; b = X; }

	return XMFLOAT3(r + m, g + m, b + m);
}