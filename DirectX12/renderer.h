#include <d3dcompiler.h> // required for compiling shaders on the fly, consider pre-compiling instead
#pragma comment(lib, "d3dcompiler.lib")
#include "d3dx12.h" // official helper file provided by microsoft

void PrintLabeledDebugString(const char* label, const char* toPrint)
{
	std::cout << label << toPrint << std::endl;
#if defined WIN32 //OutputDebugStringA is a windows-only function 
	OutputDebugStringA(label);
	OutputDebugStringA(toPrint);
#endif
}


// Creation, Rendering & Cleanup
class Renderer
{
	// proxy handles
	GW::SYSTEM::GWindow											win;
	GW::GRAPHICS::GDirectX12Surface								d3d;
	GW::INPUT::GInput											ginput;
	GW::INPUT::GController										gcontroller;

	// what we need at a minimum to draw a triangle
	D3D12_VERTEX_BUFFER_VIEW									vertexView;
	D3D12_INDEX_BUFFER_VIEW										indexView;
	Microsoft::WRL::ComPtr<ID3D12Resource>						vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource>						indexBuffer;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>					rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>					pipeline;

	//Matrix Math Proxy
	GW::MATH::GMatrix											gmatrix;

	//Handle to the level data to draw
	Level_Data&													levelHandle;
	//Logger for render debugging
	GW::SYSTEM::GLog&											renderLog;

	//View Matrix for homogeneous position
	GW::MATH::GMATRIXF											viewMatrix;
	//Projection Matrix for homogeneous position
	GW::MATH::GMATRIXF											projectionMatrix;

	//Struct of Scene Data for GPU
	struct SCENE_DATA { 
		//Sun Light settings and Camera Position
		GW::MATH::GVECTORF sunDirection, sunColor, sunAmbiet, camPos;
		//Combined view and projection matrices for homogenization
		GW::MATH::GMATRIXF viewProjection; 
	};

	//Struct of Mesh Data for GPU
	struct MESH_DATA { 
		//Indices for color and position
		unsigned materialIndex, transformIndexStart;
	};

	//Instance of Scene Data to send to GPU
	SCENE_DATA													sceneDataForGPU;
	//Instance of Mesh Data to send to GPU
	MESH_DATA													meshDataForGPU;

	//The vector of transforms to update/send to gpu
	std::vector<GW::MATH::GMATRIXF>								transformsForGPU;
	//Number of buffers in the swapchain
	unsigned int												maxActiveFrames;
	
	//All Transforms in the level - GPU Resource
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>			transformStructuredBuffer;
	//All Materials in the level - GPU Resource
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>			materialStructuredBuffer;

	//Descriptor Heap for Structured Buffers
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>				descriptorHeap;

	//*HARD CODED* sun settings
	GW::MATH::GVECTORF											sunLightDir = { -1, -1, 2 },
																sunLightColor = { 0.9f, 0.9f, 1, 1 },
																sunLightAmbient = { 0.75f, 0.9f, 0.9f, 0 };

	//Level Swap Tracking Bools
	bool														level1 = true,
																level2 = false;

	float														deltaTime;
	std::chrono::steady_clock::time_point						lastUpdate;


	//What we need for music
	GW::AUDIO::GAudio											gAudio;
	GW::AUDIO::GMusic											gMusic;
	const char*													musicPath = "../Audio/MusicTrack.wav";
	float														timeBtwPauseOrPlay = 0;

	//What we need for the 3D sound effect
	GW::AUDIO::GAudio3D											gAudio3D;
	GW::AUDIO::GSound3D											gSound3D;
	const char*													dogBarkPath = "../Audio/DogBark.wav";
	GW::MATH::GVECTORF											dogPos;



public:

	//For Level Swapping
	std::vector<char*> gameLevelPaths;
	std::vector<char*> levelModelPaths;

	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GDirectX12Surface _d3d,
			 Level_Data& _handle, GW::SYSTEM::GLog& _log) : levelHandle(_handle) , renderLog(_log)
	{
		win = _win;
		d3d = _d3d;

		IDXGISwapChain4* swapChain = nullptr;
		d3d.GetSwapchain4((void**)&swapChain);
		DXGI_SWAP_CHAIN_DESC desc;
		swapChain->GetDesc(&desc);
		maxActiveFrames = desc.BufferCount;

		swapChain->Release();
	
		gmatrix.Create();
		gAudio.Create();
		gMusic.Create(musicPath, gAudio, .2f);
		gAudio3D.Create();
		gSound3D.Create(dogBarkPath, 5, 25, GW::AUDIO::GATTENUATION::LINEAR, gAudio3D, 0.6f);
		//dog transform index is 26
		dogPos = levelHandle.levelTransforms[26].row4;
		gSound3D.UpdatePosition(dogPos);

		gAudio.PlayMusic();

		ginput.Create(win);
		gcontroller.Create();

		InitializeViewMatrix();
	
		InitializeProjectionMatrix();

		InitializeSceneDataForGPU();

		InitializeGraphics();
	}

private:
	
	void InitializeGraphics()
	{
		ID3D12Device* creator;
		d3d.GetDevice((void**)&creator);
		InitializeVertexBuffer(creator);
		InitializeIndexBuffer(creator);

		transformStructuredBuffer.resize(maxActiveFrames);
		materialStructuredBuffer.resize(maxActiveFrames);
		InitializeDescriptorHeap(creator);
		InitializeStructuredBuffersAndViews(creator);

		InitializeGraphicsPipeline(creator);

		// free temporary handle
		creator->Release();
	}

	void InitializeViewMatrix()
	{
		GW::MATH::GVECTORF eye = { 0.25f, 6.5f, -0.25f, 0 };
		GW::MATH::GVECTORF at = { 0, 0, 0, 0 };
		GW::MATH::GVECTORF up = { 0, 1, 0, 0 };
		GW::MATH::GMatrix::LookAtLHF(eye, at, up, viewMatrix);
	}

	void InitializeProjectionMatrix()
	{
		float aspectRatio;
		d3d.GetAspectRatio(aspectRatio);
		GW::MATH::GMatrix::ProjectionDirectXLHF(G_DEGREE_TO_RADIAN_F(65), aspectRatio, 0.1f, 100, projectionMatrix);
	}

	void InitializeVertexBuffer(ID3D12Device* creator)
	{	
		CreateVertexBuffer(creator, sizeof(H2B::VERTEX) * levelHandle.levelVertices.size());
		WriteToVertexBuffer(levelHandle.levelVertices.data(), sizeof(H2B::VERTEX) * levelHandle.levelVertices.size());
		CreateVertexView(sizeof(H2B::VERTEX), sizeof(H2B::VERTEX) * levelHandle.levelVertices.size());
	}

	void CreateVertexBuffer(ID3D12Device* creator, unsigned int sizeInBytes)
	{
		creator->CreateCommittedResource( // using UPLOAD heap for simplicity
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // DEFAULT recommend  
			D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(vertexBuffer.ReleaseAndGetAddressOf()));
	}

	void WriteToVertexBuffer(const void* dataToWrite, unsigned int sizeInBytes)
	{
		UINT8* transferMemoryLocation;
		vertexBuffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&transferMemoryLocation));
		memcpy(transferMemoryLocation, dataToWrite, sizeInBytes);
		vertexBuffer->Unmap(0, nullptr);
	}

	void CreateVertexView(unsigned int strideInBytes, unsigned int sizeInBytes)
	{
		vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexView.StrideInBytes = strideInBytes;
		vertexView.SizeInBytes = sizeInBytes;
	}

	void InitializeIndexBuffer(ID3D12Device* creator)
	{
		CreateIndexBuffer(creator, sizeof(unsigned) * levelHandle.levelIndices.size());
		WriteToIndexBuffer(levelHandle.levelIndices.data(), sizeof(unsigned) * levelHandle.levelIndices.size());
		CreateIndexView(sizeof(unsigned) * levelHandle.levelIndices.size());
	}

	void CreateIndexBuffer(ID3D12Device* creator, unsigned int sizeInBytes)
	{
		creator->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(indexBuffer.GetAddressOf()));
	}

	void WriteToIndexBuffer(const void* dataToWrite, unsigned int sizeInBytes)
	{
		UINT8* transferMemoryLocation;
		indexBuffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&transferMemoryLocation));
		memcpy(transferMemoryLocation, dataToWrite, sizeInBytes);
		indexBuffer->Unmap(0, nullptr);
	}

	void CreateIndexView(unsigned int sizeInBytes)
	{
		indexView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
		indexView.Format = DXGI_FORMAT_R32_UINT;
		indexView.SizeInBytes = sizeInBytes;
	}

	void InitializeSceneDataForGPU()
	{
		//Scene Variables that currently Don't change throughout the program
		sceneDataForGPU.sunColor = sunLightColor;
		sceneDataForGPU.sunDirection = sunLightDir;
		sceneDataForGPU.sunAmbiet = sunLightAmbient;

		//Transform Init
		for (int i = 0; i < levelHandle.levelTransforms.size(); i++)
		{
			transformsForGPU.push_back(levelHandle.levelTransforms[i]);
		}
	}

	void InitializeDescriptorHeap(ID3D12Device* creator)
	{
		UINT numberOfStructuredBuffers = maxActiveFrames * 2;
		UINT numberOfDescriptors = numberOfStructuredBuffers;

		D3D12_DESCRIPTOR_HEAP_DESC cBufferHeapDesc = {};
		cBufferHeapDesc.NumDescriptors = numberOfDescriptors;
		cBufferHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cBufferHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		creator->CreateDescriptorHeap(&cBufferHeapDesc, IID_PPV_ARGS(descriptorHeap.ReleaseAndGetAddressOf()));
	}

	void InitializeStructuredBuffersAndViews(ID3D12Device* creator)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
		for (int i = 0; i < maxActiveFrames; i++)
		{
			unsigned structureBufferSize = sizeof(GW::MATH::GMATRIXF) * levelHandle.levelTransforms.size();
			creator->CreateCommittedResource( // using UPLOAD heap for simplicity
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // DEFAULT recommend  
				D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(structureBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(transformStructuredBuffer[i].GetAddressOf()));

			UINT8* transferMemoryLocation;
			transformStructuredBuffer[i]->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&transferMemoryLocation));
			memcpy(transferMemoryLocation, levelHandle.levelTransforms.data(), sizeof(GW::MATH::GMATRIXF) * levelHandle.levelTransforms.size());
			transformStructuredBuffer[i]->Unmap(0, nullptr);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Buffer.NumElements = levelHandle.levelTransforms.size();
			srvDesc.Buffer.StructureByteStride = sizeof(GW::MATH::GMATRIXF);
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			creator->CreateShaderResourceView(transformStructuredBuffer[i].Get(), &srvDesc, handle);
			handle.Offset(1, sizeof(levelHandle.levelTransforms));
		}

		for (int i = 0; i < maxActiveFrames; i++)
		{
			unsigned structureBufferSize = sizeof(H2B::ATTRIBUTES) * levelHandle.levelMaterials.size();
			creator->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(structureBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(materialStructuredBuffer[i].GetAddressOf()));

			UINT8* transferMemoryLocation;
			materialStructuredBuffer[i]->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&transferMemoryLocation));
			for (int j = 0; j < levelHandle.levelMaterials.size(); j++)
			{
				memcpy(transferMemoryLocation, &levelHandle.levelMaterials[j].attrib, sizeof(H2B::ATTRIBUTES));
				transferMemoryLocation += sizeof(H2B::ATTRIBUTES);
			}			
			materialStructuredBuffer[i]->Unmap(0, nullptr);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Buffer.NumElements = levelHandle.levelMaterials.size();
			srvDesc.Buffer.StructureByteStride = sizeof(H2B::ATTRIBUTES);
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			creator->CreateShaderResourceView(materialStructuredBuffer[i].Get(), &srvDesc, handle);
			handle.Offset(1, sizeof(levelHandle.levelMaterials));
		}
	}

	void UpdateTransformsForGPU(int curFrameBufferIndex)
	{
		UINT8* transferMemoryLocation = nullptr;
		transformStructuredBuffer[curFrameBufferIndex]->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&transferMemoryLocation));
		memcpy(transferMemoryLocation, transformsForGPU.data(), sizeof(GW::MATH::GMATRIXF) * transformsForGPU.size());
		transformStructuredBuffer[curFrameBufferIndex]->Unmap(0, nullptr);
		
	}

	void HandleLevelSwapping()
	{
		float KeyState1 = 0, keyState2 = 0;
		ginput.GetState(G_KEY_1, KeyState1);
		ginput.GetState(G_KEY_2, keyState2);
		if (KeyState1 != 0 && !level1)
		{
			ID3D12Device* creator;
			d3d.GetDevice((void**)&creator);
			levelHandle.UnloadLevel();
			
			levelHandle.LoadLevel(gameLevelPaths[0], levelModelPaths[0], renderLog);
			level1 = !level1;
			level2 = !level2;
			renderLog.Log("Switched Levels");

			vertexBuffer.Reset();
			indexBuffer.Reset();
			for (int i = 0; i < maxActiveFrames; i++)
			{
				transformStructuredBuffer[i].Reset();
				materialStructuredBuffer[i].Reset();
			}

			InitializeVertexBuffer(creator);
			InitializeIndexBuffer(creator);
			InitializeStructuredBuffersAndViews(creator);

			creator->Release();
		}
		else if (keyState2 != 0 && !level2)
		{
			ID3D12Device* creator;
			d3d.GetDevice((void**)&creator);
			levelHandle.UnloadLevel();

			levelHandle.LoadLevel(gameLevelPaths[1], levelModelPaths[1], renderLog);
			level1 = !level1;
			level2 = !level2;
			renderLog.Log("Switched Levels");

			vertexBuffer.Reset();
			indexBuffer.Reset();
			for (int i = 0; i < maxActiveFrames; i++)
			{
				transformStructuredBuffer[i].Reset();
				materialStructuredBuffer[i].Reset();
			}

			InitializeVertexBuffer(creator);
			InitializeIndexBuffer(creator);
			InitializeStructuredBuffersAndViews(creator);

			creator->Release();
		}
	}

	void PauseAndPlayMusic()
	{
		float pKeyState = 0;
		ginput.GetState(G_KEY_P, pKeyState);
		timeBtwPauseOrPlay += deltaTime;
		if (pKeyState != 0)
		{	
			bool musicPlaying;
			gMusic.isPlaying(musicPlaying);
			
			if (timeBtwPauseOrPlay > 0.3f)
			{
				if (musicPlaying)
					gMusic.Pause();
				else
					gMusic.Resume();

				timeBtwPauseOrPlay = 0;
			}
		}
	}

	void PlayDogBark()
	{
		float bKeyState = 0;
		ginput.GetState(G_KEY_B, bKeyState);
		bool isPlaying;
		gSound3D.isPlaying(isPlaying);
		if (bKeyState != 0 && !isPlaying && level1)
		{
			gSound3D.Play();
		}
	}

	void HandleAudio()
	{
		PauseAndPlayMusic();
		PlayDogBark();
	}

	void LinkChildrenToParent()
	{
		for (int i = 0; i < levelHandle.blenderObjects.size(); i++)
		{
			if (levelHandle.blenderObjects[i].parentTransformIndex != -1)
			{
				GW::MATH::GMatrix::MultiplyMatrixF(levelHandle.levelTransforms[levelHandle.blenderObjects[i].transformIndex],
					transformsForGPU[levelHandle.blenderObjects[i].parentTransformIndex],
					transformsForGPU[levelHandle.blenderObjects[i].transformIndex]);
			}
		}
	}

	void RotateObjectY(unsigned blenderObjIndex, float degrees)
	{
		float radians = G_DEGREE_TO_RADIAN_F(degrees) * deltaTime;

		GW::MATH::GMatrix::RotateYLocalF(transformsForGPU[levelHandle.blenderObjects[blenderObjIndex].transformIndex], radians,
			transformsForGPU[levelHandle.blenderObjects[blenderObjIndex].transformIndex]);
	}


	void InitializeGraphicsPipeline(ID3D12Device* creator)
	{
		UINT compilerFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
		compilerFlags |= D3DCOMPILE_DEBUG;
#endif
		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob = CompileVertexShader(creator, compilerFlags);
		Microsoft::WRL::ComPtr<ID3DBlob> psBlob = CompilePixelShader(creator, compilerFlags);
		CreateRootSignature(creator);
		CreatePipelineState(vsBlob, psBlob, creator);
	}

	Microsoft::WRL::ComPtr<ID3DBlob> CompileVertexShader(ID3D12Device* creator, UINT compilerFlags)
	{
		std::string vertexShaderSource = ReadFileIntoString("../Shaders/VertexShader.hlsl");

		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, errors;

		HRESULT compilationResult =
			D3DCompile(vertexShaderSource.c_str(), vertexShaderSource.length(),
				nullptr, nullptr, nullptr, "main", "vs_5_1", compilerFlags, 0,
				vsBlob.GetAddressOf(), errors.GetAddressOf());

		if (FAILED(compilationResult))
		{
			PrintLabeledDebugString("Vertex Shader Errors:\n", (char*)errors->GetBufferPointer());
			abort();
			return nullptr;
		}

		return vsBlob;
	}

	Microsoft::WRL::ComPtr<ID3DBlob> CompilePixelShader(ID3D12Device* creator, UINT compilerFlags)
	{
		std::string pixelShaderSource = ReadFileIntoString("../Shaders/PixelShader.hlsl");

		Microsoft::WRL::ComPtr<ID3DBlob> psBlob, errors;

		HRESULT compilationResult =
			D3DCompile(pixelShaderSource.c_str(), pixelShaderSource.length(),
				nullptr, nullptr, nullptr, "main", "ps_5_1", compilerFlags, 0,
				psBlob.GetAddressOf(), errors.GetAddressOf());

		if (FAILED(compilationResult))
		{
			PrintLabeledDebugString("Pixel Shader Errors:\n", (char*)errors->GetBufferPointer());
			abort();
			return nullptr;
		}

		return psBlob;
	}

	void CreateRootSignature(ID3D12Device* creator)
	{
		Microsoft::WRL::ComPtr<ID3DBlob> signature, errors;
		CD3DX12_ROOT_PARAMETER rootParams[4] = {};
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;

		rootParams[0].InitAsConstants(32, 0);
		rootParams[1].InitAsConstants(2, 1);
		rootParams[2].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParams[3].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		rootSignatureDesc.Init(ARRAYSIZE(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);

		creator->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	}

	void CreatePipelineState(Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, Microsoft::WRL::ComPtr<ID3DBlob> psBlob, ID3D12Device* creator)
	{
		// Create Input Layout
		D3D12_INPUT_ELEMENT_DESC formats[3]; 
		formats[0].SemanticName = "POSITION";
		formats[0].SemanticIndex = 0;
		formats[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		formats[0].InputSlot = 0;
		formats[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		formats[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		formats[0].InstanceDataStepRate = 0;

		formats[1].SemanticName = "UVW";
		formats[1].SemanticIndex = 0;
		formats[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		formats[1].InputSlot = 0;
		formats[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		formats[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		formats[1].InstanceDataStepRate = 0;

		formats[2].SemanticName = "NORMAL";
		formats[2].SemanticIndex = 0;
		formats[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		formats[2].InputSlot = 0;
		formats[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		formats[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		formats[2].InstanceDataStepRate = 0;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psDesc;
		ZeroMemory(&psDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

		psDesc.InputLayout = { formats, ARRAYSIZE(formats) };
		psDesc.pRootSignature = rootSignature.Get();
		psDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
		psDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
		psDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psDesc.SampleMask = UINT_MAX;
		psDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psDesc.NumRenderTargets = 1;
		psDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psDesc.SampleDesc.Count = 1;

		creator->CreateGraphicsPipelineState(&psDesc, IID_PPV_ARGS(&pipeline));
	}


public:
	void Render()
	{
		HandleLevelSwapping();
		HandleAudio();
	
		PipelineHandles curHandles = GetCurrentPipelineHandles();
		SetUpPipeline(curHandles);

		UINT curFrame = 0;
		d3d.GetSwapChainBufferIndex(curFrame);
		UpdateTransformsForGPU(curFrame);

		curHandles.commandList->SetGraphicsRoot32BitConstants(0, 32, &sceneDataForGPU, 0);
		curHandles.commandList->SetGraphicsRootShaderResourceView(2, transformStructuredBuffer[curFrame]->GetGPUVirtualAddress());
		curHandles.commandList->SetGraphicsRootShaderResourceView(3, materialStructuredBuffer[curFrame]->GetGPUVirtualAddress());

		for (int instance = 0; instance < levelHandle.levelInstances.size(); instance++)
		{
			int model = levelHandle.levelInstances[instance].modelIndex;
			for (int mesh = levelHandle.levelModels[model].meshStart;
				mesh < levelHandle.levelModels[model].meshStart + levelHandle.levelModels[model].meshCount; mesh++)
			{
				meshDataForGPU.materialIndex = mesh;
				meshDataForGPU.transformIndexStart = levelHandle.levelInstances[instance].transformStart;
				curHandles.commandList->SetGraphicsRoot32BitConstants(1, 2, &meshDataForGPU, 0);

				curHandles.commandList->DrawIndexedInstanced(levelHandle.levelMeshes[mesh].drawInfo.indexCount, levelHandle.levelInstances[instance].transformCount,
					levelHandle.levelModels[model].indexStart + levelHandle.levelMeshes[mesh].drawInfo.indexOffset, levelHandle.levelModels[model].vertexStart, 0);
			}
		}

		curHandles.commandList->Release();
	}

	void Update()
	{	
		auto now = std::chrono::steady_clock::now();
		deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(now - lastUpdate).count() / 1000000.0f;
		lastUpdate = now;

		GW::MATH::GMATRIXF cameraMatrix;
		GW::MATH::GMatrix::InverseF(viewMatrix, cameraMatrix);
		float aspectRatio;
		d3d.GetAspectRatio(aspectRatio);
		cameraMatrix = CameraMovement::Get().GetCameraMatrixFromInput(cameraMatrix, aspectRatio, win, ginput, gcontroller);
		GW::MATH::GMatrix::InverseF(cameraMatrix, viewMatrix);

		GW::MATH::GMatrix::ProjectionDirectXLHF(G_DEGREE_TO_RADIAN_F(65), aspectRatio, 0.1f, 100, projectionMatrix);
		GW::MATH::GMatrix::MultiplyMatrixF(viewMatrix, projectionMatrix, sceneDataForGPU.viewProjection);
		sceneDataForGPU.camPos = cameraMatrix.row4;

		GW::MATH::GQUATERNIONF orientation;
		GW::MATH::GMatrix::GetRotationF(cameraMatrix, orientation);
		gAudio3D.Update3DListener(cameraMatrix.row4, orientation);

		RotateObjectY(31, 90);

		LinkChildrenToParent();
	}

private:
	struct PipelineHandles
	{
		ID3D12GraphicsCommandList* commandList;
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView;
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView;
	};

	PipelineHandles GetCurrentPipelineHandles()
	{
		PipelineHandles retval;
		d3d.GetCommandList((void**)&retval.commandList);
		d3d.GetCurrentRenderTargetView((void**)&retval.renderTargetView);
		d3d.GetDepthStencilView((void**)&retval.depthStencilView);
		return retval;
	}

	void SetUpPipeline(PipelineHandles handles)
	{
		handles.commandList->SetGraphicsRootSignature(rootSignature.Get());
		handles.commandList->SetDescriptorHeaps(1, descriptorHeap.GetAddressOf());
		handles.commandList->OMSetRenderTargets(1, &handles.renderTargetView, FALSE, &handles.depthStencilView);
		handles.commandList->SetPipelineState(pipeline.Get());
		handles.commandList->IASetVertexBuffers(0, 1, &vertexView);
		handles.commandList->IASetIndexBuffer(&indexView);
		handles.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}


public:
	~Renderer()
	{
		// ComPtr will auto release so nothing to do here yet 
	}
};
