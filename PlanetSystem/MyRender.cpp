#include "MyRender.h"
#include <d3dcompiler.h>

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



	// 1) Генерируем массив вершин/индексов для сферы
	GenerateSphere(1.0f, 20, 20, sphereVerts, sphereIdx); // радиус=1, slices=20, stacks=20

	// 2) Создаём VertexBuffer
	ID3D11Buffer* sphereVB = CreateVertexBuffer(sphereVerts.data(), (UINT)sphereVerts.size());

	// 3) Создаём IndexBuffer
	ID3D11Buffer* sphereIB = CreateIndexBuffer(sphereIdx.data(), (UINT)sphereIdx.size());

	// 4) Сохраняем эти буферы как член класса, если хотите рисовать их каждый кадр
	m_planetVB = sphereVB;
	m_planetIB = sphereIB;

	float width = 800.0f;
	float height = 600.0f;
	m_Projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, width / height, 0.01f, 100.0f);

	//g_orbitCam.Init(Vector3(0.0f, 0.0f, 0.0f), -5.0f);
	m_fpsCamera.Init(Vector3(0.0f, 1.0f, -5.0f), 0.0f, 0.0f);

	m_pImmediateContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pImmediateContext->PSSetShader(m_pPixelShader, NULL, 0);


	return true;
}



bool MyRender::Draw()
{
	Update();

	static float t = 0.0f;
	static DWORD dwTimeStart = 0;
	DWORD dwTimeCur = GetTickCount64();
	if (dwTimeStart == 0)
		dwTimeStart = dwTimeCur;
	t = (dwTimeCur - dwTimeStart) / 1000.0f;

	float dt = CalculateDeltaTime();  // Получили дельту в секундах
	Matrix mPlanetScale = Matrix::CreateScale(0.3f, 0.3f, 0.3f);
	Matrix mSatelliteScale = Matrix::CreateScale(0.1f, 0.1f, 0.1f);
	Matrix mSpin = Matrix::CreateRotationY(-t);
	Matrix mPlanetTranslate1 = Matrix::CreateTranslation(-4.0f, 0.0f, 0.0f);
	Matrix mPlanetTranslate2 = Matrix::CreateTranslation(-8.0f, 0.0f, 0.0f);
	Matrix mPlanetTranslate3 = Matrix::CreateTranslation(-12.0f, 0.0f, 0.0f);
	Matrix mPlanetTranslate4 = Matrix::CreateTranslation(-16.0f, 0.0f, 0.0f);
	Matrix mSatelliteTranslate = Matrix::CreateTranslation(-1.0f, 0.0f, 0.0f);
	Matrix mPlanetOrbit1 = Matrix::CreateRotationY(-t * 2.0f);
	Matrix mPlanetOrbit2 = Matrix::CreateRotationY(t * 2.2f);
	Matrix mPlanetOrbit3 = Matrix::CreateRotationY(-t * 2.4f);
	Matrix mPlanetOrbit4 = Matrix::CreateRotationY(t * 1.5f);
	Matrix mSatelliteOrbit = Matrix::CreateRotationY(-t * 6.0f);

	Matrix m_sunWorld = Matrix::CreateRotationY(t);
	Matrix m_planetWorld1 = mPlanetScale * mSpin * mPlanetTranslate1 * mPlanetOrbit1;
	Matrix m_satelliteWorld1 = mSatelliteScale * mSpin * mSatelliteTranslate * mSatelliteOrbit * mPlanetTranslate1 * mPlanetOrbit1;
	Matrix m_planetWorld2 = mPlanetScale * mSpin * mPlanetTranslate2 * mPlanetOrbit2;
	Matrix m_satelliteWorld2 = mSatelliteScale * mSpin * mSatelliteTranslate * mSatelliteOrbit * mPlanetTranslate2 * mPlanetOrbit2;
	Matrix m_planetWorld3 = mPlanetScale * mSpin * mPlanetTranslate3 * mPlanetOrbit3;
	Matrix m_satelliteWorld3 = mSatelliteScale * mSpin * mSatelliteTranslate * mSatelliteOrbit * mPlanetTranslate3 * mPlanetOrbit3;
	Matrix m_planetWorld4 = mPlanetScale * mSpin * mPlanetTranslate4 * mPlanetOrbit4;
	Matrix m_satelliteWorld4 = mSatelliteScale * mSpin * mSatelliteTranslate * mSatelliteOrbit * mPlanetTranslate4 * mPlanetOrbit4;

	m_objectWorlds[0] = m_sunWorld;
	m_objectWorlds[1] = m_planetWorld1;
	m_objectWorlds[2] = m_planetWorld2;
	m_objectWorlds[2] = m_planetWorld3;
	m_objectWorlds[2] = m_planetWorld4;

	m_planetVB = CreateVertexBuffer(sphereVerts.data(), (UINT)sphereVerts.size());
	m_planetIB = CreateIndexBuffer(sphereIdx.data(), (UINT)sphereIdx.size());
	m_sphereIndexCount = (UINT)sphereIdx.size();

	

	RenderObject(m_planetVB, m_planetIB, m_sunWorld, m_sphereIndexCount);
	RenderObject(m_planetVB, m_planetIB, m_planetWorld1, m_sphereIndexCount);
	RenderObject(m_planetVB, m_planetIB, m_satelliteWorld1, m_sphereIndexCount);
	RenderObject(m_planetVB, m_planetIB, m_planetWorld2, m_sphereIndexCount);
	RenderObject(m_planetVB, m_planetIB, m_satelliteWorld2, m_sphereIndexCount);
	RenderObject(m_planetVB, m_planetIB, m_planetWorld3, m_sphereIndexCount);
	RenderObject(m_planetVB, m_planetIB, m_satelliteWorld3, m_sphereIndexCount);
	RenderObject(m_planetVB, m_planetIB, m_planetWorld4, m_sphereIndexCount);
	RenderObject(m_planetVB, m_planetIB, m_satelliteWorld4, m_sphereIndexCount);

	return true;
}

void MyRender::Update()
{
	//// Храним предыдущую позицию мыши
	//static POINT lastMousePos = { 0,0 };

	//// Текущая позиция курсора (в координатах экрана)
	//POINT currentPos;
	//GetCursorPos(&currentPos);

	//// Вычисляем delta X, delta Y
	//float dx = static_cast<float>(currentPos.x - lastMousePos.x);
	//float dy = static_cast<float>(currentPos.y - lastMousePos.y);

	//// Если зажата левая кнопка мыши, вращаем камеру
	//if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
	//{
	//	// Обычный паттерн – dx отвечает за yaw, dy за pitch
	//	// Знак dy можно инвертировать, если кажется «наоборот»
	//	g_orbitCam.Rotate(dx, -dy);
	//}

	//// Сбрасываем lastMousePos
	//lastMousePos = currentPos;

	//// Пример переключения камеры на первый объект (индекс 0), 
	//// если пользователь нажал клавишу '1'
	//if (GetAsyncKeyState('1') & 0x0001)
	//{
	//	m_currentObject = 0;
	//}

	//// То же самое для второго объекта, по клавише '2'
	//if (GetAsyncKeyState('2') & 0x0001)
	//{
	//	m_currentObject = 1;
	//}

	//// Обновляем камеру
	//Vector3 pos(
	//	m_objectWorlds[m_currentObject]._41,
	//	m_objectWorlds[m_currentObject]._42,
	//	m_objectWorlds[m_currentObject]._43
	//);
	//g_orbitCam.SetTarget(pos);

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
		m_fpsCamera.Rotate(dx, -dy);
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
	m_fpsCamera.Move(forward, strafe, upDown);
}

void MyRender::RenderObject(ID3D11Buffer* vertexBuffer, ID3D11Buffer* indexBuffer, Matrix world, UINT indexCount)
{
	// Обновляем CB, как и раньше
	ConstantBuffer cb;
	cb.world = XMMatrixTranspose(world);
	cb.view = XMMatrixTranspose(m_fpsCamera.GetViewMatrix()); // <-- камера
	//cb.view = XMMatrixTranspose(m_View);
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