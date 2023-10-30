// Simple basecode showing how to create a window and attatch a d3d12surface
#define GATEWARE_ENABLE_CORE // All libraries need this
#define GATEWARE_ENABLE_SYSTEM // Graphics libs require system level libraries
#define GATEWARE_ENABLE_GRAPHICS // Enables all Graphics Libraries
// Ignore some GRAPHICS libraries we aren't going to use
#define GATEWARE_DISABLE_GDIRECTX11SURFACE // we have another template for this
#define GATEWARE_DISABLE_GOPENGLSURFACE // we have another template for this
#define GATEWARE_DISABLE_GVULKANSURFACE // we have another template for this
#define GATEWARE_DISABLE_GRASTERSURFACE // we have another template for this

#define GATEWARE_ENABLE_MATH

#define GATEWARE_ENABLE_INPUT

#define GATEWARE_ENABLE_AUDIO
// With what we want & what we don't defined we can include the API
#include "../gateware-main/Gateware.h"


#include "FileIntoString.h" 
#include "lvlData.h"
#include "CameraMovement.h"
#include "renderer.h"
// open some namespaces to compact the code a bit
using namespace GW;
using namespace CORE;
using namespace SYSTEM;
using namespace GRAPHICS;
// lets pop a window and use D3D12 to clear to a jade colored screen
int main()
{
	GWindow win;
	GEventResponder msgs;
	GDirectX12Surface d3d12;
	if (+win.Create(0, 0, 800, 600, GWindowStyle::WINDOWEDBORDERED))
	{
		// TODO: Part 1a
		win.SetWindowName("Bjorn Nordenflycht Level Renderer D3D12");

		float clr[] = { 0, 168/255.0f, 107/255.0f, 1 }; // start with a jade color
		msgs.Create([&](const GW::GEvent& e) {
			GW::SYSTEM::GWindow::Events q;
			if (+e.Read(q) && q == GWindow::Events::RESIZE)
				clr[0] += 0.01f; // move towards a orange as they resize
		});
		win.Register(msgs);
		if (+d3d12.Create(win, GW::GRAPHICS::DEPTH_BUFFER_SUPPORT))
		{
			//Instantiating our level
			GLog log;
			log.Create("LogOutput.txt");
			log.EnableConsoleLogging(true);
			Level_Data myLevel;

			bool loadedLevel = myLevel.LoadLevel("../Level1/GameLevel.txt", "../Level1/Models", log);

			Renderer renderer(win, d3d12, myLevel, log); // init

			while (+win.ProcessWindowEvents())
			{
				if (+d3d12.StartFrame())
				{
					ID3D12GraphicsCommandList* cmd;
					D3D12_CPU_DESCRIPTOR_HANDLE rtv;
					D3D12_CPU_DESCRIPTOR_HANDLE dsv;
					if (+d3d12.GetCommandList((void**)&cmd) && 
						+d3d12.GetCurrentRenderTargetView((void**)&rtv) &&
						+d3d12.GetDepthStencilView((void**)&dsv))
					{
						cmd->ClearRenderTargetView(rtv, clr, 0, nullptr);
						cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
						
						renderer.Update();
						renderer.Render(); // draw
						d3d12.EndFrame(false);
						cmd->Release();
					}
				}
			}// clean-up when renderer falls off stack
		}
	}
	return 0; // that's all folks
}