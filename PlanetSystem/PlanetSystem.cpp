#include "MyRender.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

int main()
{
	Framework framework;

	MyRender* render = new MyRender();

	FrameworkDesc desc;
	desc.wnd.width = 800;
	desc.wnd.height = 600;
	desc.render = render;

	framework.Init(desc);

	framework.Run();

	framework.Close();

	return 0;
}