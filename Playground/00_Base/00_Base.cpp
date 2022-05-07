//Interfaces
#include "../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../Common_3/OS/Interfaces/IScreenshot.h"
#include "../../../Common_3/OS/Interfaces/IScripting.h"
#include "../../../Common_3/OS/Interfaces/IFont.h"
#include "../../../Common_3/OS/Interfaces/IUI.h"

//Renderer
#include "../../../Common_3/Renderer/IRenderer.h"
#include "../../../Common_3/Renderer/IResourceLoader.h"


//***********************************************************************************//
//*                                 Device Resources                                *//
//***********************************************************************************//
const uint32_t	gImageCount = 3;
ProfileToken	gGpuProfileToken;

uint32_t		gFrameIndex = 0;
Renderer*		pRenderer = NULL;

Queue*			pGraphicsQueue = NULL;
CmdPool*		pCmdPools[gImageCount];
Cmd*			pCmds[gImageCount];

SwapChain*		pSwapChain = NULL;
Fence*			pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*		pImageAcquiredSemaphore = NULL;
Semaphore*		pRenderCompleteSemaphores[gImageCount] = { NULL };
//***********************************************************************************//

//***********************************************************************************//
//*                                 Forge Resources                                *//
//***********************************************************************************//
uint32_t		gFontID = 0;
FontDrawDesc	gFrameTimeDraw;
//***********************************************************************************//

class Base : public IApp
{
public:
	bool Init() override;
	void Exit() override;

	bool Load() override;
	void Unload() override;

	void Update(float deltaTime) override;
	void Draw() override;

	const char* GetName() override { return "00_Base"; }

private:
	void createSamplers() {}
	void createShaders() {}
	void createRootSignatures() {}
	void createResources() {}
	void createUniformBuffers() {}
	void createDescriptorSets() {}

	bool addSwapChain();
	bool addRenderTargets() { return true; }
	bool addDepthBuffer() { return true; }
	void addPipelines() {}

	void updateUniformBuffers() {}
};

DEFINE_APPLICATION_MAIN(Base)

bool Base::Init()
{
	// File paths
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,	"Shaders");
	fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SHADER_BINARIES,	"CompiledShaders");
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,		"GPUCfg");
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,			"Textures");
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES,			"Meshes");
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,			"Fonts");
	fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS,		"Screenshots");
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS,			"Scripts");

	// Window and renderer setup
	RendererDesc settings;
	memset(&settings, 0, sizeof(settings));
	initRenderer(GetName(), &settings, &pRenderer);

	QueueDesc queueDesc = {};
	queueDesc.mType = QUEUE_TYPE_GRAPHICS;
	queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;

	addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		CmdPoolDesc cmdPoolDesc = {};
		cmdPoolDesc.pQueue = pGraphicsQueue;
		addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
		CmdDesc cmdDesc = {};
		cmdDesc.pPool = pCmdPools[i];
		addCmd(pRenderer, &cmdDesc, &pCmds[i]);
	}

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		addFence(pRenderer, &pRenderCompleteFences[i]);
		addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
	}
	addSemaphore(pRenderer, &pImageAcquiredSemaphore);

	initResourceLoaderInterface(pRenderer);

	//*****************************************************************************//
	//*                              USER TODO                                    *//
	//*****************************************************************************//


	//*****************************************************************************//

	// Load fonts
	FontDesc font = {};
	font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
	fntDefineFonts(&font, 1, &gFontID);

	FontSystemDesc fontRenderDesc = {};
	fontRenderDesc.pRenderer = pRenderer;
	if (!initFontSystem(&fontRenderDesc))
		return false; // report?

	// Initialize Forge User Interface Rendering
	UserInterfaceDesc uiRenderDesc = {};
	uiRenderDesc.pRenderer = pRenderer;
	initUserInterface(&uiRenderDesc);

	// Initialize micro profiler and its UI.
	ProfilerDesc profiler = {};
	profiler.pRenderer = pRenderer;
	profiler.mWidthUI = mSettings.mWidth;
	profiler.mHeightUI = mSettings.mHeight;
	initProfiler(&profiler);

	// Gpu profiler can only be added after initProfile.
	gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

	waitForAllResourceLoads();

	InputSystemDesc inputDesc = {};
	inputDesc.pRenderer = pRenderer;
	inputDesc.pWindow = pWindow;
	if (!initInputSystem(&inputDesc))
		return false;

	// App Actions
	InputActionDesc actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
	addInputAction(&actionDesc);
	actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
	addInputAction(&actionDesc);
	InputActionCallback onUIInput = [](InputActionContext* ctx)
	{
		bool capture = uiOnInput(ctx->mBinding, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
		if (ctx->mBinding != InputBindings::FLOAT_LEFTSTICK)
			setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
		return true;
	};
	actionDesc = { InputBindings::BUTTON_ANY, onUIInput, this };
	addInputAction(&actionDesc);

	return true;
}

void Base::Exit()
{
	exitInputSystem();

	exitProfiler();

	exitUserInterface();

	exitFontSystem();

	//*****************************************************************************//
	//*                              USER TODO                                    *//
	//*****************************************************************************//


	//*****************************************************************************//

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeFence(pRenderer, pRenderCompleteFences[i]);
		removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
	}
	removeSemaphore(pRenderer, pImageAcquiredSemaphore);

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeCmd(pRenderer, pCmds[i]);
		removeCmdPool(pRenderer, pCmdPools[i]);
	}

	exitResourceLoaderInterface(pRenderer);
	removeQueue(pRenderer, pGraphicsQueue);
	exitRenderer(pRenderer);
	pRenderer = NULL;
}

bool Base::Load()
{
	if (!addSwapChain())
		return false;

	if (!addRenderTargets())
		return false;

	if (!addDepthBuffer())
		return false;

	// LOAD USER INTERFACE
	RenderTarget* ppPipelineRenderTargets[] = {
		pSwapChain->ppRenderTargets[0],
	};

	if (!addFontSystemPipelines(ppPipelineRenderTargets, 1, NULL))
		return false;

	if (!addUserInterfacePipelines(ppPipelineRenderTargets[0]))
		return false;

	addPipelines();

	return true;
}

void Base::Unload()
{
	waitQueueIdle(pGraphicsQueue);

	removeUserInterfacePipelines();

	removeFontSystemPipelines();

	//*****************************************************************************//
	//*                     USER TODO :  Remove Pipelines                         *//
	//*****************************************************************************//

	//*****************************************************************************//

	removeSwapChain(pRenderer, pSwapChain);


	//*****************************************************************************//
	//*                    USER TODO :  Remove Render Targets                     *//
	//*****************************************************************************//


	//*****************************************************************************//
}

void Base::Update(float deltaTime)
{
	updateInputSystem(mSettings.mWidth, mSettings.mHeight);

	//*****************************************************************************//
	//*                              USER TODO                                    *//
	//*****************************************************************************//


	//*****************************************************************************//
}

void Base::Draw()
{
	if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
	{
		waitQueueIdle(pGraphicsQueue);
		::toggleVSync(pRenderer, &pSwapChain);
	}

	uint32_t swapchainImageIndex;
	acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

	// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
	Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
	FenceStatus fenceStatus;
	getFenceStatus(pRenderer, pNextFence, &fenceStatus);
	if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		waitForFences(pRenderer, 1, &pNextFence);

	resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

	//*****************************************************************************//
	//*                     USER TODO : Update Uniform Buffers                    *//
	//*****************************************************************************//
	updateUniformBuffers();
	//*****************************************************************************//

	Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
	Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

	Cmd* cmd = pCmds[gFrameIndex];
	beginCmd(cmd);
	cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

	//*****************************************************************************//
	//*                              USER TODO                                    *//
	//*****************************************************************************//

	RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

	RenderTargetBarrier barriers[] =    // wait for resource transition
	{
		{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
	};
	cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

	LoadActionsDesc loadActions = {};
	loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
	loadActions.mClearColorValues[0].r = 0.0F;
	loadActions.mClearColorValues[0].g = 0.0F;
	loadActions.mClearColorValues[0].b = 0.0F;
	loadActions.mClearColorValues[0].a = 0.0F;
	cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

	//*****************************************************************************//

	{
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");

		LoadActionsDesc loadActionsUI = {};
		loadActionsUI.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActionsUI, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontSize = 18.0f;
		gFrameTimeDraw.mFontID = 0;
		float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
		cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

		cmdDrawUserInterface(cmd);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
	}

	cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

	RenderTargetBarrier finalBarriers = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
	cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &finalBarriers);
	cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
	endCmd(cmd);

	QueueSubmitDesc submitDesc = {};
	submitDesc.mCmdCount = 1;
	submitDesc.mSignalSemaphoreCount = 1;
	submitDesc.mWaitSemaphoreCount = 1;
	submitDesc.ppCmds = &cmd;
	submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
	submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
	submitDesc.pSignalFence = pRenderCompleteFence;
	queueSubmit(pGraphicsQueue, &submitDesc);
	QueuePresentDesc presentDesc = {};
	presentDesc.mIndex = swapchainImageIndex;
	presentDesc.mWaitSemaphoreCount = 1;
	presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
	presentDesc.pSwapChain = pSwapChain;
	presentDesc.mSubmitDone = true;
	queuePresent(pGraphicsQueue, &presentDesc);

	flipProfiler();

	gFrameIndex = (gFrameIndex + 1) % gImageCount;
}

bool Base::addSwapChain()
{
	SwapChainDesc swapChainDesc = {};
	swapChainDesc.mWindowHandle = pWindow->handle;
	swapChainDesc.mPresentQueueCount = 1;
	swapChainDesc.ppPresentQueues = &pGraphicsQueue;
	swapChainDesc.mWidth = mSettings.mWidth;
	swapChainDesc.mHeight = mSettings.mHeight;
	swapChainDesc.mImageCount = gImageCount;

	// This unit test does manual tone mapping
	swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, false);
	swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
	::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

	return pSwapChain != NULL;
}
