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

#include "../../../Common_3/ThirdParty/OpenSource/cgltf/GLTFLoader.h"

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

RenderTarget*	pDepthBuffer = NULL;
//***********************************************************************************//

//***********************************************************************************//
//*                                 Forge Resources                                *//
//***********************************************************************************//
uint32_t		gFontID = 0;
FontDrawDesc	gFrameTimeDraw = {};

UIComponent*	pGuiGraphics = NULL;
//***********************************************************************************//

//***********************************************************************************//
//*                              Pipeline Resources                                 *//
//***********************************************************************************//
// Samplers
Sampler*			pBaseColorSampler = NULL;

// Shaders
Shader*				pBasicShader = NULL;

// Root Signatures
RootSignature*		pBasicRootSignature = NULL;

// Textures
Texture*			pBaseColorMap = NULL;

// Geometry
VertexLayout		gVertexLayout = {};
Geometry*			pGeometry = NULL;

// DescriptorSets
DescriptorSet*		pBasicDescriptorSets[DESCRIPTOR_UPDATE_FREQ_COUNT];

// Pipelines
Pipeline*			pBasicPipeline;
//***********************************************************************************//

//***********************************************************************************//
//*                                Scene Resources                                  *//
//***********************************************************************************//
ICameraController*	pCameraController = NULL;

const uint32_t		gLightCount = 3;
const uint32_t		gTotalLightCount = gLightCount + 1;

struct GlobalConstants
{
	CameraMatrix mViewProjectionMatrix;
	vec4 mCameraPosition;
	vec4 mLightColor[gTotalLightCount];
	vec4 mLightDirection[gLightCount];
};
GlobalConstants		gGlobalConstantsData;
Buffer*				pGlobalConstantsBuffer[gImageCount] = { NULL };

static float4		gLightColor[gTotalLightCount] = { float4(1.f), float4(1.f), float4(1.f), float4(1.f, 1.f, 1.f, 0.25f) };
static float		gLightColorIntensity[gTotalLightCount] = { 0.1f, 0.2f, 0.2f, 0.25f };
static float2		gLightDirection = { -122.0f, 222.0f };

struct MeshConstants
{
	mat4 mModelMatrix;
};
MeshConstants		gMeshConstants;
Buffer*				pMeshConstantsBuffer = NULL;
GLTFContainer*		pGLTFContainer = NULL;
mat4*				gNodeTransforms = NULL;

struct MaterialConstants
{
	vec4 mBaseColorFactor;
};
MaterialConstants	gMaterialConstants;
Buffer*				pMaterialConstantsBuffer = NULL;
//***********************************************************************************//

class MeshViewer : public IApp
{
public:
	bool Init() override;
	void Exit() override;

	bool Load() override;
	void Unload() override;

	void Update(float deltaTime) override;
	void Draw() override;

	const char* GetName() override { return "01_MeshViewer"; }

private:
	void createSamplers();
	void createShaders();
	void createRootSignatures();
	void createResources();
	void createConstants();
	void createDescriptorSets();
	void createScene();
	void createGUI();

	bool addSwapChain();
	bool addRenderTargets();
	bool addDepthBuffer();
	void addPipelines();

	void updateUniformBuffers();
};

DEFINE_APPLICATION_MAIN(MeshViewer)

bool MeshViewer::Init()
{
	// File paths
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
	fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SHADER_BINARIES, "CompiledShaders");
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
	fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

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

	createSamplers();
	createShaders();
	createRootSignatures();
	createResources();
	createConstants();

	waitForAllResourceLoads();

	createDescriptorSets();
	createScene();
	createGUI();

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
	actionDesc = { InputBindings::FLOAT_LEFTSTICK, onUIInput, this, 20.0f, 200.0f, 1.0f };
	addInputAction(&actionDesc);
	typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
	static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
	{
		if (*ctx->pCaptured)
		{
			float2 val = uiIsFocused() ? float2(0.0f) : ctx->mFloat2;
			index ? pCameraController->onRotate(val) : pCameraController->onMove(val);
		}
		return true;
	};
	actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 0.25f };
	addInputAction(&actionDesc);
	actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 0.25f };
	addInputAction(&actionDesc);
	actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
	addInputAction(&actionDesc);

	return true;
}

void MeshViewer::Exit()
{
	exitInputSystem();

	exitCameraController(pCameraController);

	exitProfiler();

	exitUserInterface();

	exitFontSystem();

	//*****************************************************************************//
	//*                              USER TODO                                    *//
	//*****************************************************************************//
	// Remove Descriptor Sets
	removeDescriptorSet(pRenderer, pBasicDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_DRAW]);
	removeDescriptorSet(pRenderer, pBasicDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);

	// Remove Resources
	removeResource(pMeshConstantsBuffer);
	removeResource(pMaterialConstantsBuffer);
	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeResource(pGlobalConstantsBuffer[i]);
	}
	removeResource(pBaseColorMap);
	removeResource(pGeometry);
	gltfUnloadContainer(pGLTFContainer);
	pGLTFContainer = NULL;
	gNodeTransforms = NULL;

	// Remove Root Signatures
	removeRootSignature(pRenderer, pBasicRootSignature);

	// Remove Shaders
	removeShader(pRenderer, pBasicShader);

	// Remove Samplers
	removeSampler(pRenderer, pBaseColorSampler);
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

bool MeshViewer::Load()
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
		pDepthBuffer,
	};

	if (!addFontSystemPipelines(ppPipelineRenderTargets, 2, NULL))
		return false;

	if (!addUserInterfacePipelines(ppPipelineRenderTargets[0]))
		return false;

	addPipelines();

	return true;
}

void MeshViewer::Unload()
{
	waitQueueIdle(pGraphicsQueue);

	removeUserInterfacePipelines();

	removeFontSystemPipelines();

	//*****************************************************************************//
	//*                     USER TODO :  Remove Pipelines                         *//
	//*****************************************************************************//

	removePipeline(pRenderer, pBasicPipeline);

	//*****************************************************************************//

	removeSwapChain(pRenderer, pSwapChain);


	//*****************************************************************************//
	//*                    USER TODO :  Remove Render Targets                     *//
	//*****************************************************************************//

	removeRenderTarget(pRenderer, pDepthBuffer);

	//*****************************************************************************//
}

void MeshViewer::Update(float deltaTime)
{
	updateInputSystem(mSettings.mWidth, mSettings.mHeight);

	pCameraController->update(deltaTime);

	//*****************************************************************************//
	//*                              USER TODO                                    *//
	//*****************************************************************************//
	// Scene Update
	mat4 viewMat = pCameraController->getViewMatrix();

	const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
	const float horizontal_fov = PI / 2.0f;
	CameraMatrix projMat = CameraMatrix::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
	CameraMatrix projViewMat = projMat * viewMat;

	gGlobalConstantsData.mViewProjectionMatrix = projViewMat;
	gGlobalConstantsData.mCameraPosition = vec4(pCameraController->getViewPosition(), 1.0f);

	for (uint i = 0; i < gTotalLightCount; ++i)
	{
		gGlobalConstantsData.mLightColor[i] = gLightColor[i].toVec4();

		gGlobalConstantsData.mLightColor[i].setW(gLightColorIntensity[i]);
	}

	float Azimuth = (PI / 180.0f) * gLightDirection.x;
	float Elevation = (PI / 180.0f) * (gLightDirection.y - 180.0f);

	vec3 sunDirection = normalize(vec3(cosf(Azimuth)*cosf(Elevation), sinf(Elevation), sinf(Azimuth)*cosf(Elevation)));

	gGlobalConstantsData.mLightDirection[0] = vec4(sunDirection, 0.0f);
	// generate 2nd, 3rd light from the main light
	gGlobalConstantsData.mLightDirection[1] = vec4(-sunDirection.getX(), sunDirection.getY(), -sunDirection.getZ(), 0.0f);
	gGlobalConstantsData.mLightDirection[2] = vec4(-sunDirection.getX(), -sunDirection.getY(), -sunDirection.getZ(), 0.0f);

	// Animation

	//*****************************************************************************//
}

void MeshViewer::Draw()
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

	{
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Mesh");

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0F;
		loadActions.mClearColorValues[0].g = 0.0F;
		loadActions.mClearColorValues[0].b = 0.0F;
		loadActions.mClearColorValues[0].a = 0.0F;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		cmdBindPipeline(cmd, pBasicPipeline);
		cmdBindDescriptorSet(cmd, gFrameIndex, pBasicDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
		cmdBindVertexBuffer(cmd, 1, &pGeometry->pVertexBuffers[0], pGeometry->mVertexStrides, (uint64_t*)NULL);
		cmdBindIndexBuffer(cmd, pGeometry->pIndexBuffer, pGeometry->mIndexType, (uint64_t)NULL);

		for (uint32_t n = 0; n < pGLTFContainer->mNodeCount; ++n)
		{
			GLTFNode& node = pGLTFContainer->pNodes[n];
			if (node.mMeshIndex != UINT_MAX)
			{
				gMeshConstants.mModelMatrix = gNodeTransforms[n];
				gMaterialConstants.mBaseColorFactor = vec4(1.0f);
				cmdBindDescriptorSet(cmd, 0, pBasicDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_DRAW]);

				for (uint32_t i = 0; i < node.mMeshCount; ++i)
				{
					GLTFMesh& mesh = pGLTFContainer->pMeshes[node.mMeshIndex + i];
					cmdDrawIndexed(cmd, mesh.mIndexCount, mesh.mStartIndex, 0);
				}
			}
		}

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
	}

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

void MeshViewer::createSamplers()
{
	SamplerDesc samplerDesc = {};
	samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
	samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
	samplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
	samplerDesc.mMinFilter = FILTER_NEAREST;
	samplerDesc.mMagFilter = FILTER_LINEAR;
	samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
	addSampler(pRenderer, &samplerDesc, &pBaseColorSampler);
}

void MeshViewer::createShaders()
{
	ShaderLoadDesc basicShader = {};
	basicShader.mStages[0] = { "basic.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_NONE };
	basicShader.mStages[1] = { "basic.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_NONE };
	addShader(pRenderer, &basicShader, &pBasicShader);
}

void MeshViewer::createRootSignatures()
{
	RootSignatureDesc rootDesc = {};
	rootDesc.mStaticSamplerCount = 0;
	rootDesc.mShaderCount = 1;
	rootDesc.ppShaders = &pBasicShader;
	addRootSignature(pRenderer, &rootDesc, &pBasicRootSignature);
}

void MeshViewer::createResources()
{
	// Load Textures
	{
		TextureLoadDesc baseColorMapDesc = {};
		baseColorMapDesc.pFileName = "DuckCM";
		baseColorMapDesc.ppTexture = &pBaseColorMap;
		// Textures representing color should be stored in SRGB or HDR format
		baseColorMapDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
		addResource(&baseColorMapDesc, NULL);
	}

	// Load Models
	{
		gVertexLayout.mAttribCount = 3;
		gVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		gVertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayout.mAttribs[0].mBinding = 0;
		gVertexLayout.mAttribs[0].mLocation = 0;
		gVertexLayout.mAttribs[0].mOffset = 0;
		gVertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		gVertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayout.mAttribs[1].mBinding = 0;
		gVertexLayout.mAttribs[1].mLocation = 1;
		gVertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
		gVertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		gVertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		gVertexLayout.mAttribs[2].mBinding = 0;
		gVertexLayout.mAttribs[2].mLocation = 2;
		gVertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

		GeometryLoadDesc loadDesc = {};
		loadDesc.pFileName = "Duck.gltf";
		loadDesc.pVertexLayout = &gVertexLayout;
		loadDesc.ppGeometry = &pGeometry;
		addResource(&loadDesc, NULL);

		uint32_t res = gltfLoadContainer("Duck.gltf", NULL, GLTF_FLAG_CALCULATE_BOUNDS, &pGLTFContainer);
	}
}

void MeshViewer::createConstants()
{
	BufferLoadDesc globalConstantsDesc = {};
	globalConstantsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	globalConstantsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	globalConstantsDesc.mDesc.mSize = sizeof(GlobalConstants);
	globalConstantsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	globalConstantsDesc.pData = &gGlobalConstantsData;
	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		globalConstantsDesc.ppBuffer = &pGlobalConstantsBuffer[i];
		addResource(&globalConstantsDesc, NULL);
	}

	BufferLoadDesc meshConstantsDesc = {};
	meshConstantsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	meshConstantsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	meshConstantsDesc.mDesc.mSize = sizeof(MeshConstants);
	meshConstantsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	meshConstantsDesc.pData = &gMeshConstants;
	meshConstantsDesc.ppBuffer = &pMeshConstantsBuffer;
	addResource(&meshConstantsDesc, NULL);

	BufferLoadDesc materialConstantsDesc = {};
	materialConstantsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	materialConstantsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	materialConstantsDesc.mDesc.mSize = sizeof(MaterialConstants);
	materialConstantsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	materialConstantsDesc.pData = &gMaterialConstants;
	materialConstantsDesc.ppBuffer = &pMaterialConstantsBuffer;
	addResource(&materialConstantsDesc, NULL);
}

void MeshViewer::createDescriptorSets()
{
	DescriptorSetDesc setDesc = { pBasicRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, 1 };
	addDescriptorSet(pRenderer, &setDesc, &pBasicDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_DRAW]);
	setDesc = { pBasicRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, 3 };
	addDescriptorSet(pRenderer, &setDesc, &pBasicDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);

	DescriptorData params[4] = {};
	params[0].pName = "meshConstants";
	params[0].ppBuffers = &pMeshConstantsBuffer;
	params[1].pName = "materialConstants";
	params[1].ppBuffers = &pMaterialConstantsBuffer;
	params[2].pName = "baseColorMap";
	params[2].ppTextures = &pBaseColorMap;
	params[3].pName = "baseColorSampler";
	params[3].ppSamplers = &pBaseColorSampler;
	updateDescriptorSet(pRenderer, 0, pBasicDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_DRAW], 4, params);

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		params[0].pName = "globalConstants";
		params[0].ppBuffers = &pGlobalConstantsBuffer[i];
		updateDescriptorSet(pRenderer, i, pBasicDescriptorSets[DESCRIPTOR_UPDATE_FREQ_PER_FRAME], 1, params);
	}
}

void MeshViewer::createScene()
{
	CameraMotionParameters cmp{ 1.0f, 120.0f, 40.0f };
	vec3                   camPos{ 3.0f, 2.5f, 4.0f };
	vec3                   lookAt{ 0.0f, 0.4f, 0.0f };

	//pCameraController = initFpsCameraController(normalize(camPos) * 3.0f, lookAt);
	pCameraController = initFpsCameraController(camPos, lookAt);
	pCameraController->setMotionParameters(cmp);

	typedef void(*UpdateTransformHandler)(GLTFContainer* pData, size_t nodeIndex, mat4* nodeTransforms, bool* nodeTransformsInited);
	static UpdateTransformHandler UpdateTransform = [](GLTFContainer* pData, size_t nodeIndex, mat4* nodeTransforms, bool* nodeTransformsInited)
	{
		if (nodeTransformsInited[nodeIndex]) { return; }

		if (pData->pNodes[nodeIndex].mScale.getX() != pData->pNodes[nodeIndex].mScale.getY() ||
			pData->pNodes[nodeIndex].mScale.getX() != pData->pNodes[nodeIndex].mScale.getZ())
		{
			LOGF(LogLevel::eWARNING, "Node %llu has a non-uniform scale and will have an incorrect normal when rendered.", (uint64_t)nodeIndex);
		}

		mat4 matrix = pData->pNodes[nodeIndex].mMatrix;
		if (pData->pNodes[nodeIndex].mParentIndex != UINT_MAX)
		{
			UpdateTransform(pData, (size_t)pData->pNodes[nodeIndex].mParentIndex, nodeTransforms, nodeTransformsInited);
			matrix = nodeTransforms[pData->pNodes[nodeIndex].mParentIndex] * matrix;
		}
		nodeTransforms[nodeIndex] = matrix;
		nodeTransformsInited[nodeIndex] = true;
	};

	if (pGLTFContainer->mNodeCount)
	{
		bool* nodeTransformsInited = (bool*)alloca(sizeof(bool) * pGLTFContainer->mNodeCount);
		memset(nodeTransformsInited, 0, sizeof(bool) * pGLTFContainer->mNodeCount);

		gNodeTransforms = (mat4*)malloc(sizeof(mat4) * pGLTFContainer->mNodeCount); //-V630

		for (uint32_t i = 0; i < pGLTFContainer->mNodeCount; ++i)
		{
			UpdateTransform(pGLTFContainer, i, gNodeTransforms, nodeTransformsInited);
		}

		// Scale and centre the model.

		Point3 modelBounds[2] = { Point3(FLT_MAX), Point3(-FLT_MAX) };
		size_t nodeIndex = 0;
		for (uint32_t n = 0; n < pGLTFContainer->mNodeCount; ++n)
		{
			GLTFNode& node = pGLTFContainer->pNodes[n];

			if (node.mMeshIndex != UINT_MAX)
			{
				for (uint32_t i = 0; i < node.mMeshCount; ++i)
				{
					Point3 minBound = pGLTFContainer->pMeshes[node.mMeshIndex + i].mMin;
					Point3 maxBound = pGLTFContainer->pMeshes[node.mMeshIndex + i].mMax;
					Point3 localPoints[] = {
						Point3(minBound.getX(), minBound.getY(), minBound.getZ()),
						Point3(minBound.getX(), minBound.getY(), maxBound.getZ()),
						Point3(minBound.getX(), maxBound.getY(), minBound.getZ()),
						Point3(minBound.getX(), maxBound.getY(), maxBound.getZ()),
						Point3(maxBound.getX(), minBound.getY(), minBound.getZ()),
						Point3(maxBound.getX(), minBound.getY(), maxBound.getZ()),
						Point3(maxBound.getX(), maxBound.getY(), minBound.getZ()),
						Point3(maxBound.getX(), maxBound.getY(), maxBound.getZ()),
					};
					for (size_t j = 0; j < 8; j += 1)
					{
						vec4 worldPoint = gNodeTransforms[nodeIndex] * localPoints[j];
						modelBounds[0] = minPerElem(modelBounds[0], Point3(worldPoint.getXYZ()));
						modelBounds[1] = maxPerElem(modelBounds[1], Point3(worldPoint.getXYZ()));
					}
				}
			}
			nodeIndex += 1;
		}

		const float targetSize = 1.0;

		vec3 modelSize = modelBounds[1] - modelBounds[0];
		float largestDim = max(modelSize.getX(), max(modelSize.getY(), modelSize.getZ()));
		Point3 modelCentreBase = Point3(
			0.5f * (modelBounds[0].getX() + modelBounds[1].getX()),
			modelBounds[0].getY(),
			0.5f * (modelBounds[0].getZ() + modelBounds[1].getZ()));
		Vector3 scaleVector = Vector3(targetSize / largestDim);
		scaleVector.setZ(-scaleVector.getZ());
		mat4 translateScale = mat4::scale(scaleVector) * mat4::translation(-Vector3(modelCentreBase));

		for (uint32_t i = 0; i < pGLTFContainer->mNodeCount; ++i)
		{
			gNodeTransforms[i] = translateScale * gNodeTransforms[i];
		}
	}
}

void MeshViewer::createGUI()
{
	UIComponentDesc guiDesc = {};
	guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.25f);
	uiCreateComponent("Graphics Options", &guiDesc, &pGuiGraphics);

	SeparatorWidget separator;
	uiCreateComponentWidget(pGuiGraphics, "", &separator, WIDGET_TYPE_SEPARATOR);

	CollapsingHeaderWidget LightWidgets;
	LightWidgets.mDefaultOpen = false;
	uiSetCollapsingHeaderWidgetCollapsed(&LightWidgets, false);

	SliderFloatWidget azimuthSlider;
	azimuthSlider.pData = &gLightDirection.x;
	azimuthSlider.mMin = float(-180.0f);
	azimuthSlider.mMax = float(180.0f);
	azimuthSlider.mStep = float(0.001f);
	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light Azimuth", &azimuthSlider, WIDGET_TYPE_SLIDER_FLOAT);

	SliderFloatWidget elevationSlider;
	elevationSlider.pData = &gLightDirection.y;
	elevationSlider.mMin = float(210.0f);
	elevationSlider.mMax = float(330.0f);
	elevationSlider.mStep = float(0.001f);
	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light Elevation", &elevationSlider, WIDGET_TYPE_SLIDER_FLOAT);

	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

	CollapsingHeaderWidget LightColor1Picker;
	ColorPickerWidget light1Picker;
	light1Picker.pData = &gLightColor[0];
	uiCreateCollapsingHeaderSubWidget(&LightColor1Picker, "Main Light Color", &light1Picker, WIDGET_TYPE_COLOR_PICKER);
	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Main Light Color", &LightColor1Picker, WIDGET_TYPE_COLLAPSING_HEADER);

	CollapsingHeaderWidget LightColor1Intensity;
	SliderFloatWidget lightIntensitySlider;
	lightIntensitySlider.pData = &gLightColorIntensity[0];
	lightIntensitySlider.mMin = 0.0f;
	lightIntensitySlider.mMax = 5.0f;
	lightIntensitySlider.mStep = 0.001f;
	uiCreateCollapsingHeaderSubWidget(&LightColor1Intensity, "Main Light Intensity", &lightIntensitySlider, WIDGET_TYPE_SLIDER_FLOAT);
	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Main Light Intensity", &LightColor1Intensity, WIDGET_TYPE_COLLAPSING_HEADER);

	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

	CollapsingHeaderWidget LightColor2Picker;
	ColorPickerWidget light2Picker;
	light2Picker.pData = &gLightColor[1];
	uiCreateCollapsingHeaderSubWidget(&LightColor2Picker, "Light2 Color", &light2Picker, WIDGET_TYPE_COLOR_PICKER);
	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light2 Color", &LightColor2Picker, WIDGET_TYPE_COLLAPSING_HEADER);

	CollapsingHeaderWidget LightColor2Intensity;
	SliderFloatWidget light2IntensitySlider;
	light2IntensitySlider.pData = &gLightColorIntensity[1];
	light2IntensitySlider.mMin = 0.0f;
	light2IntensitySlider.mMax = 5.0f;
	light2IntensitySlider.mStep = 0.001f;
	uiCreateCollapsingHeaderSubWidget(&LightColor2Intensity, "Light2 Intensity", &light2IntensitySlider, WIDGET_TYPE_SLIDER_FLOAT);
	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light2 Intensity", &LightColor2Intensity, WIDGET_TYPE_COLLAPSING_HEADER);

	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

	CollapsingHeaderWidget LightColor3Picker;
	ColorPickerWidget light3Picker;
	light3Picker.pData = &gLightColor[2];
	uiCreateCollapsingHeaderSubWidget(&LightColor3Picker, "Light3 Color", &light3Picker, WIDGET_TYPE_COLOR_PICKER);
	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light3 Color", &LightColor3Picker, WIDGET_TYPE_COLLAPSING_HEADER);

	CollapsingHeaderWidget LightColor3Intensity;
	SliderFloatWidget light3IntensitySlider;
	light3IntensitySlider.pData = &gLightColorIntensity[2];
	light3IntensitySlider.mMin = 0.0f;
	light3IntensitySlider.mMax = 5.0f;
	light3IntensitySlider.mStep = 0.001f;
	uiCreateCollapsingHeaderSubWidget(&LightColor3Intensity, "Light3 Intensity", &light3IntensitySlider, WIDGET_TYPE_SLIDER_FLOAT);
	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light3 Intensity", &LightColor3Intensity, WIDGET_TYPE_COLLAPSING_HEADER);

	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

	CollapsingHeaderWidget AmbientLightColorPicker;
	ColorPickerWidget ambientPicker;
	ambientPicker.pData = &gLightColor[3];
	uiCreateCollapsingHeaderSubWidget(&AmbientLightColorPicker, "Ambient Light Color", &ambientPicker, WIDGET_TYPE_COLOR_PICKER);
	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Ambient Light Color", &AmbientLightColorPicker, WIDGET_TYPE_COLLAPSING_HEADER);

	CollapsingHeaderWidget AmbientColorIntensity;
	SliderFloatWidget ambientIntensitySlider;
	ambientIntensitySlider.pData = &gLightColorIntensity[3];
	ambientIntensitySlider.mMin = 0.0f;
	ambientIntensitySlider.mMax = 5.0f;
	ambientIntensitySlider.mStep = 0.001f;
	uiCreateCollapsingHeaderSubWidget(&AmbientColorIntensity, "Ambient Light Intensity", &ambientIntensitySlider, WIDGET_TYPE_SLIDER_FLOAT);
	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Ambient Light Intensity", &AmbientColorIntensity, WIDGET_TYPE_COLLAPSING_HEADER);

	uiCreateCollapsingHeaderSubWidget(&LightWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

	uiCreateComponentWidget(pGuiGraphics, "Light Options", &LightWidgets, WIDGET_TYPE_COLLAPSING_HEADER);
}

bool MeshViewer::addSwapChain()
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

bool MeshViewer::addRenderTargets()
{
	return true;
}

bool MeshViewer::addDepthBuffer()
{
	RenderTargetDesc depthRT = {};
	depthRT.mArraySize = 1;
	depthRT.mClearValue.depth = 0.0f;
	depthRT.mClearValue.stencil = 0;
	depthRT.mDepth = 1;
	depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
	depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
	depthRT.mWidth = mSettings.mWidth;
	depthRT.mHeight = mSettings.mHeight;
	depthRT.mSampleCount = SAMPLE_COUNT_1;
	depthRT.mSampleQuality = 0;
	depthRT.mFlags = TEXTURE_CREATION_FLAG_NONE;
	addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

	return pDepthBuffer != NULL;
}

void MeshViewer::addPipelines()
{
	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthTest = true;
	depthStateDesc.mDepthWrite = true;
	depthStateDesc.mDepthFunc = CMP_LEQUAL;

	PipelineDesc desc = {};
	desc.mType = PIPELINE_TYPE_GRAPHICS;
	GraphicsPipelineDesc& basicPipelineSettings = desc.mGraphicsDesc;
	basicPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	basicPipelineSettings.mRenderTargetCount = 1;
	basicPipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
	basicPipelineSettings.pDepthState = &depthStateDesc;
	basicPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
	basicPipelineSettings.mSampleCount = pDepthBuffer->mSampleCount;
	basicPipelineSettings.mSampleQuality = pDepthBuffer->mSampleQuality;
	basicPipelineSettings.pRootSignature = pBasicRootSignature;
	basicPipelineSettings.pRasterizerState = &rasterizerStateDesc;
	basicPipelineSettings.pShaderProgram = pBasicShader;
	basicPipelineSettings.pVertexLayout = &gVertexLayout;
	addPipeline(pRenderer, &desc, &pBasicPipeline);
}

void MeshViewer::updateUniformBuffers()
{
	BufferUpdateDesc globalConstantsBufferCbv = { pGlobalConstantsBuffer[gFrameIndex] };
	beginUpdateResource(&globalConstantsBufferCbv);
	*(GlobalConstants*)globalConstantsBufferCbv.pMappedData = gGlobalConstantsData;
	endUpdateResource(&globalConstantsBufferCbv, NULL);

	BufferUpdateDesc meshConstantsBufferCbv = { pMeshConstantsBuffer };
	beginUpdateResource(&meshConstantsBufferCbv);
	*(MeshConstants*)meshConstantsBufferCbv.pMappedData = gMeshConstants;
	endUpdateResource(&meshConstantsBufferCbv, NULL);

	BufferUpdateDesc materialConstantsBufferCbv = { pMaterialConstantsBuffer };
	beginUpdateResource(&materialConstantsBufferCbv);
	*(MaterialConstants*)materialConstantsBufferCbv.pMappedData = gMaterialConstants;
	endUpdateResource(&materialConstantsBufferCbv, NULL);
}
