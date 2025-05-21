#include "MyRender.h"
#include <d3dcompiler.h>
#include "ModelLoader.h"

static auto previousTime = std::chrono::high_resolution_clock::now();

struct SimpleVertex
{
	Vector3 Pos;
	Vector4 Color;
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
	// четыре угла квадрата
	const float S = 100.0f;
	SimpleVertex verts[] = {
		{{ -S, 0.0f, -S }, {0.3f,0.3f,0.3f,1}},  // темно-серый
		{{  S, 0.0f, -S }, {0.3f,0.3f,0.3f,1}},
		{{  S, 0.0f,  S }, {0.3f,0.3f,0.3f,1}},
		{{ -S, 0.0f,  S }, {0.3f,0.3f,0.3f,1}},
	};
	WORD idx[] = {
		0,2,1,
		0,3,2
	};
	m_planeVB = CreateVertexBuffer(verts, _countof(verts));
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

	if (FAILED(hr) && pErrorBlob != NULL)
		OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());

	_RELEASE(pErrorBlob);
	return hr;
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
		Log::Get()->Err("Невозможно скомпилировать файл shader.fx. Пожалуйста, запустите данную программу из папки, содержащей этот файл");
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
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
		Log::Get()->Err("Невозможно скомпилировать файл shader.fx. Пожалуйста, запустите данную программу из папки, содержащей этот файл");
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

	m_World = DirectX::XMMatrixIdentity();

	float width = 1920.0f;
	float height = 1080.0f;
	m_Projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, width / height, 0.01f, 100.0f);

	//g_orbitCam.Init(Vector3(0.0f, 0.0f, 0.0f), -5.0f);
	//m_fpsCamera.Init(Vector3(0.0f, 1.0f, -5.0f), 0.0f, 0.0f);

	m_pImmediateContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pImmediateContext->PSSetShader(m_pPixelShader, NULL, 0);

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT hr = m_pd3dDevice->CreateSamplerState(&sampDesc, &m_samplerState);
	if (FAILED(hr)) {
		Log::Get()->Err("Ошибка создания SamplerState");
		return false;
	}

	CreatePlane();

	LoadPlaceholderMeshes();   // создаём один юнит-куб и одну юнит-сферу

	ModelLoader loader(m_pd3dDevice, m_pImmediateContext);
	m_placeholders = loader.LoadModel(L"Models\\katamari_scene.fbx");

	SpawnScene();              // раскидываем 150 объектов
	// (можно подправить количество)

// 1) Устанавливаем визуальный радиус и коллизию
	m_ball.visualRadius = 0.8f;
	m_ball.bs = DirectX::BoundingSphere(Vector3(0, m_ball.visualRadius, 0),
		m_ball.visualRadius);

	// ориентация по умолчанию
	m_ball.orientation = Quaternion::Identity;

	// 2) Камера «привязана» к шару, дистанция 20 юнитов
	g_orbitCam.Init(m_ball.bs.Center, 5.0f);

	// После создания m_pPixelShader (старого PSMain):
	ID3DBlob* pGradPS = nullptr;
	m_compileshaderfromfile(L"shader.hlsl", "PSGradient", "ps_5_0", &pGradPS);
	m_pd3dDevice->CreatePixelShader(pGradPS->GetBufferPointer(), pGradPS->GetBufferSize(), nullptr, &m_pPSGradient);
	_RELEASE(pGradPS);

	ID3DBlob* pTexPS = nullptr;
	m_compileshaderfromfile(L"shader.hlsl", "PSTextured", "ps_5_0", &pTexPS);
	m_pd3dDevice->CreatePixelShader(pTexPS->GetBufferPointer(), pTexPS->GetBufferSize(), nullptr, &m_pPSTextured);
	_RELEASE(pTexPS);

	// Создаём буфер GradCB:
	struct GradCB { DirectX::XMFLOAT4 bottom, top; float height; float pad; } gradInit = {
		{ m_gradBottomColor.x, m_gradBottomColor.y, m_gradBottomColor.z, m_gradBottomColor.w },
		{ m_gradTopColor.x, m_gradTopColor.y, m_gradTopColor.z, m_gradTopColor.w },
		m_gradHeight,
		0
	};
	D3D11_BUFFER_DESC bd = {}; bd.ByteWidth = sizeof(gradInit); bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	D3D11_SUBRESOURCE_DATA sd = { &gradInit,0,0 };
	m_pd3dDevice->CreateBuffer(&bd, &sd, &m_gradCB);

	return true;
}



bool MyRender::Draw()
{
	float dt = CalculateDeltaTime();
	Update();              // старая камера + клавиатура
	UpdateKatamari(dt);    // новая логика

	// рисуем пол
	RenderObject(m_planeVB, m_planeIB, nullptr, true,
		Matrix::Identity, m_planeIndexCount);

	// рисуем шарик
	RenderObject(m_planetVB, m_planetIB, nullptr, true,
		m_ball.world, m_sphereIndexCount);

	// рисуем объекты из реальных моделей
	for (auto& obj : m_objects) {
		RenderObject(obj.mesh.vb, obj.mesh.ib, obj.mesh.texture, false,
			obj.local * (obj.attached ? m_ball.world : Matrix::Identity),
			obj.mesh.indexCount);
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

void MyRender::RenderObject(
	ID3D11Buffer* vb,
	ID3D11Buffer* ib,
	ID3D11ShaderResourceView* texSRV,
	bool useGradient,
	const DirectX::SimpleMath::Matrix& world,
	UINT indexCount)
{
	// Обновляем CB, как и раньше
	ConstantBuffer cb;
	cb.world = XMMatrixTranspose(world);
	cb.view = XMMatrixTranspose(g_orbitCam.GetViewMatrix()); // <-- камера
	cb.projection = XMMatrixTranspose(m_Projection);

	m_pImmediateContext->UpdateSubresource(constantBuffer, 0, NULL, &cb, 0, 0);

	m_pImmediateContext->VSSetConstantBuffers(0, 1, &constantBuffer);

	m_pImmediateContext->VSSetShader(m_pVertexShader, nullptr, 0);

	if (useGradient) {
		m_pImmediateContext->PSSetShader(m_pPSGradient, nullptr, 0);
		// bind gradCB → slot b1
		m_pImmediateContext->PSSetConstantBuffers(1, 1, &m_gradCB);
	}
	else {
		m_pImmediateContext->PSSetShader(m_pPSTextured, nullptr, 0);
		m_pImmediateContext->PSSetShaderResources(0, 1, &texSRV);
		m_pImmediateContext->PSSetSamplers(0, 1, &m_samplerState);
	}

	UINT stride = sizeof(SimpleVertex), offset = 0;
	m_pImmediateContext->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
	m_pImmediateContext->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
	m_pImmediateContext->DrawIndexed(indexCount, 0, 0);
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

// Генерация сферы радиусом radius,
// с количеством делений по широте (stacks) и долготе (slices)
void MyRender::GenerateSphere(
	float radius,
	unsigned int slices,
	unsigned int stacks,
	std::vector<SimpleVertex>& outVertices,
	std::vector<WORD>& outIndices)
{
	using namespace DirectX;

	outVertices.clear();
	outIndices.clear();

	// Перебираем stacks (параллели) от 0 до stacks
	// phi = угол от -90° (южный полюс) до +90° (северный)
	for (unsigned int i = 0; i <= stacks; i++)
	{
		float phi = XM_PI * (float)i / (float)stacks - XM_PI / 2;
		float y = radius * sinf(phi);       // от -R до +R
		float r = radius * cosf(phi);       // «горизонтальный» радиус круга на данной широте

		// Перебираем slices (долготы) от 0 до slices
		// theta = угол от 0..2PI вокруг оси
		for (unsigned int j = 0; j <= slices; j++)
		{
			float theta = 2.0f * XM_PI * (float)j / (float)slices;
			float x = r * cosf(theta);
			float z = r * sinf(theta);

			// Добавим вершину
			SimpleVertex v;
			v.Pos = Vector3(x, y, z);
			// Покрасим, например, в зависимости от phi, theta,
			// или сделаем единый цвет. Для примера пусть будет
			// что-то, зависящее от координат:
			v.Color = Vector4(
				(x / radius + 1.0f) * 0.5f,
				(y / radius + 1.0f) * 0.5f,
				(z / radius + 1.0f) * 0.5f,
				1.0f
			);
			outVertices.push_back(v);
		}
	}

	// Теперь генерируем индексы.
	// Каждый «квадратик» на сфере составит 2 треугольника:
	// stack шаг i, slice шаг j
	unsigned int stride = slices + 1; // кол-во вершин в «строке»
	for (unsigned int i = 0; i < stacks; i++)
	{
		for (unsigned int j = 0; j < slices; j++)
		{
			// Индексы четырёх углов «квадратика» (i, i+1 по вертикали; j, j+1 по горизонтали)
			WORD i0 = (WORD)(i * stride + j);
			WORD i1 = (WORD)(i * stride + j + 1);
			WORD i2 = (WORD)((i + 1) * stride + j);
			WORD i3 = (WORD)((i + 1) * stride + j + 1);

			// 1) Треугольник (i0, i1, i2)
			outIndices.push_back(i0);
			outIndices.push_back(i1);
			outIndices.push_back(i2);

			// 2) Треугольник (i1, i3, i2)
			outIndices.push_back(i1);
			outIndices.push_back(i3);
			outIndices.push_back(i2);
		}
	}
}

#include <random>

//----------------------------------------------------------------------
// 1.  Загружаем / генерируем простые меши-заглушки
//----------------------------------------------------------------------
void MyRender::LoadPlaceholderMeshes()
{
	// ---------- 1) Сфера (катамари и мелкие шарики) ----------
	{
		std::vector<SimpleVertex> v;
		std::vector<WORD>         i;
		GenerateSphere(1.0f, 16, 16, v, i);          // радиус 1

		MeshGPU m;
		m.vb = CreateVertexBuffer(v.data(), (UINT)v.size());
		m.ib = CreateIndexBuffer(i.data(), (UINT)i.size());
		m.indexCount = (UINT)i.size();
		m_placeholders.push_back(m);
	}

	// ---------- 2) Куб (коробки, ящики …) ----------
	{
		// упрощённый unit-cube (позиция + цвет)
		const SimpleVertex verts[] =
		{
			{{-0.5f,-0.5f,-0.5f},{1,0,0,1}}, {{-0.5f, 0.5f,-0.5f},{0,1,0,1}},
			{{ 0.5f, 0.5f,-0.5f},{0,0,1,1}}, {{ 0.5f,-0.5f,-0.5f},{1,1,0,1}},
			{{-0.5f,-0.5f, 0.5f},{1,0,1,1}}, {{-0.5f, 0.5f, 0.5f},{0,1,1,1}},
			{{ 0.5f, 0.5f, 0.5f},{1,1,1,1}}, {{ 0.5f,-0.5f, 0.5f},{0,0,0,1}}
		};
		const WORD idx[] =
		{
			0,1,2, 0,2,3,   // -Z
			4,6,5, 4,7,6,   // +Z
			4,5,1, 4,1,0,   // -X
			3,2,6, 3,6,7,   // +X
			1,5,6, 1,6,2,   // +Y
			4,0,3, 4,3,7    // -Y
		};

		MeshGPU m;
		m.vb = CreateVertexBuffer(verts, _countof(verts));
		m.ib = CreateIndexBuffer(idx, _countof(idx));
		m.indexCount = _countof(idx);
		m_placeholders.push_back(m);
	}
}

//----------------------------------------------------------------------
// 2.  Случайно наполняем сцену объектами-«мусором»
//----------------------------------------------------------------------
void MyRender::SpawnScene()
{
	std::mt19937                rng{ std::random_device{}() };
	std::uniform_real_distribution<float> posDist(-50.f, 50.f);  // XZ
	std::uniform_real_distribution<float> sclDist(0.3f, 1.2f);
	std::uniform_int_distribution<int>    meshDist(0, (int)m_placeholders.size() - 1);

	const int objectCount = 150;
	m_objects.reserve(objectCount);

	for (int n = 0; n < objectCount; ++n)
	{
		GameObject obj;
		obj.mesh = m_placeholders[meshDist(rng)];

		float scale = sclDist(rng);
		float x = posDist(rng);
		float z = posDist(rng);

		// локальная BoundingSphere для куба или сферы-placeholder’а
		obj.bs = DirectX::BoundingSphere(Vector3::Zero, scale);

		// предмет стоит на полу ⇒ y = radius
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
	m_ball.world =
		Matrix::CreateFromQuaternion(m_ball.orientation)
		* Matrix::CreateTranslation(m_ball.bs.Center);



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