#include<d3d12.h>
#include<dxgi1_6.h>
#include<vector>
#include<string>
#include<DirectXMath.h>
#include<d3dcompiler.h>
#define DIRECTINPUT_VERSION 0x0800//DirectInputのバージョン指定
#include<dinput.h>
#include<DirectXTex.h>
#include<wrl.h>
#include<d3dx12.h>
#include "Input.h"
#include "WinApp.h"

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

const int spriteSRVCount = 512;

// gomen

//定数バッファ用データ構造体
struct ConstBufferData {
	XMFLOAT4 color;//色
	XMMATRIX mat;//3D変換行列
};

//頂点データ構造体
struct Vertex {
	XMFLOAT3 pos;//xyz座標
	XMFLOAT3 normal;//法線ベクトル
	XMFLOAT2 uv;//uv座標
};

struct VertexPosUv {
	XMFLOAT3 pos;
	XMFLOAT2 uv;
};

struct PipelineSet {
	ComPtr<ID3D12PipelineState>pipelinestate;

	ComPtr<ID3D12RootSignature>rootsignature;
};

struct object3d {
	//定数バッファ
	ID3D12Resource* constBUff;
	//定数バッファビューのハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandleCBV;
	//定数バッファビューのハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleCBV;

	ComPtr<ID3D12Resource> texBuff;

	ComPtr<ID3D12DescriptorHeap> descHeap;

	UINT descriptorHandleIncrementSize = 0;

	XMFLOAT3 scale = { 1,1,1 };
	XMFLOAT3 rotation = { 0,0,0 };
	XMFLOAT3 position = { 0,0,0 };
	XMMATRIX matWorld;
	object3d* parent = nullptr;
};

struct Sprite {
	//頂点バッファ
	ComPtr<ID3D12Resource> vertBuff = nullptr;
	//頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vbView{};
	//定数バッファ
	ComPtr<ID3D12Resource>constBuff = nullptr;
	//Z軸回りの回転角
	float rotation = 0.0f;
	//座標
	XMFLOAT3 position = { 0,0,0 };
	//ワールド行列
	XMMATRIX matWorld;
	//色(RGBA)
	XMFLOAT4 color = { 1,1,1,1 };
	//テクスチャ番号
	UINT texNumber = 0;
	//大きさ
	XMFLOAT2 size = { 100,100 };
	//アンカーポイント
	XMFLOAT2 anchorpoint = { 0.5f,0.5f };
	//左右反転
	bool isFlipX = false;
	//上下反転
	bool isFlipY = false;
	//テクスチャ左上座標
	XMFLOAT2 texLeftTop = { 0,0 };
	//テクスチャ切り出しサイズ
	XMFLOAT2 texSize = { 100,100 };
	//非表示状態
	bool isInvisible = false;
};

struct SpriteCommon {
	//パイプラインセット
	PipelineSet pipeineset;
	//射影行列
	XMMATRIX matProjection{};
	//テクスチャ用デスクリプタヒープの生成
	ComPtr<ID3D12DescriptorHeap> descHeap;
	//テクスチャリソース(テクスチャ)の配列
	ComPtr<ID3D12Resource>texBuff[spriteSRVCount];
};
void InitializeObject3d(object3d* object, int index, ComPtr<ID3D12Device> dev, ComPtr<ID3D12DescriptorHeap> descHeap) {
	HRESULT result;
	//定数バッファのヒープ設定
	// 頂点バッファの設定
	D3D12_HEAP_PROPERTIES heapprop{};   // ヒープ設定
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD; // GPUへの転送用

	D3D12_RESOURCE_DESC resdesc{};  // リソース設定
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = (sizeof(ConstBufferData) + 0xff) & ~0xff; // 頂点データ全体のサイズ
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.SampleDesc.Count = 1;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	result = dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&object->constBUff)
	);

	UINT descHandleIncrementSize =
		dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	object->cpuDescHandleCBV = descHeap->GetCPUDescriptorHandleForHeapStart();
	object->cpuDescHandleCBV.ptr += index * descHandleIncrementSize;

	object->gpuDescHandleCBV = descHeap->GetGPUDescriptorHandleForHeapStart();
	object->gpuDescHandleCBV.ptr += index * descHandleIncrementSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
	cbvDesc.BufferLocation = object->constBUff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)object->constBUff->GetDesc().Width;
	dev->CreateConstantBufferView(&cbvDesc, object->cpuDescHandleCBV);
}
void UpdateObject3d(object3d* object, XMMATRIX& matview, XMMATRIX& matProjection, XMFLOAT4& color) {
	XMMATRIX matScale, matRot, matTrans;

	//スケール、回転、平行移動行列の計算
	matScale = XMMatrixScaling(object->scale.x, object->scale.y, object->scale.z);
	matRot = XMMatrixIdentity();
	matRot *= XMMatrixRotationZ(XMConvertToRadians(object->rotation.z));
	matRot *= XMMatrixRotationX(XMConvertToRadians(object->rotation.x));
	matRot *= XMMatrixRotationY(XMConvertToRadians(object->rotation.y));
	matTrans = XMMatrixTranslation(object->position.x, object->position.y, object->position.z);
	//ワールド行列の合成
	object->matWorld = XMMatrixIdentity();//変形をリセット
	object->matWorld *= matScale;//ワールド行列にスケーリングを反映
	object->matWorld *= matRot;//ワールド行列に回転を反映
	object->matWorld *= matTrans;//ワールド行列に平行移動を反映
	//親オブジェクトがあれば
	if (object->parent != nullptr) {
		//親オブジェクトのワールド行列を掛ける
		object->matWorld = object->parent->matWorld;
	}
	//定数バッファへデータ転送
	ConstBufferData* constMap = nullptr;
	if (SUCCEEDED(object->constBUff->Map(0, nullptr, (void**)&constMap))) {
		constMap->color = color;
		constMap->mat = object->matWorld * matview * matProjection;
		object->constBUff->Unmap(0, nullptr);
	}
}

PipelineSet object3dCreateGrphicsPipeline(ID3D12Device* dev) {
#pragma region 

	HRESULT result;
	ComPtr<ID3DBlob> vsBlob = nullptr; // 頂点シェーダオブジェクト
	ComPtr<ID3DBlob> psBlob = nullptr; // ピクセルシェーダオブジェクト
	ComPtr<ID3DBlob> errorBlob = nullptr; // エラーオブジェクト

	// 頂点シェーダの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/Shaders/BasicVS.hlsl",  // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&vsBlob, &errorBlob);

	if (FAILED(result)) {
		// errorBlobからエラー内容をstring型にコピー
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());

		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errstr.begin());
		errstr += "\n";
		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(errstr.c_str());
		exit(1);
	}

	// ピクセルシェーダの読み込みとコンパイル
	result = D3DCompileFromFile(
		L"Resources/Shaders/BasicPS.hlsl",   // シェーダファイル名
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
		"main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
		0,
		&psBlob, &errorBlob);

	if (FAILED(result)) {
		// errorBlobからエラー内容をstring型にコピー
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());

		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errstr.begin());
		errstr += "\n";
		// エラー内容を出力ウィンドウに表示
		OutputDebugStringA(errstr.c_str());
		exit(1);
	}


	//頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{//xyz座標
			"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
		{//法線ベクトル
			"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
		{//uv座標
			"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
	};

#pragma endregion
#pragma region パイプライン

	//グラフィックスパイプライン設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};

	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());

	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;//標準設定

	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	//デプステンシルステートの設定
	gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	//レンダーターゲットのブレンド設定
	D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
	blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;//標準設定
	blenddesc.BlendEnable = true;//ブレンドを有効にする



	////加算
	blenddesc.BlendOp = D3D12_BLEND_OP_ADD;
	blenddesc.SrcBlend = D3D12_BLEND_ONE;//ソースの値を100%使う
	blenddesc.DestBlend = D3D12_BLEND_ONE;//デストの値を100%使う

	blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;//加算
	blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE;//ソースの値を100%使う
	blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO;//テストの値を0%使う
	////減算
	//blenddesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	//blenddesc.SrcBlend = D3D12_BLEND_ONE;//ソースの値を100%使う
	//blenddesc.DestBlend = D3D12_BLEND_ONE;//デストの値を100%使う

	////反転
	//blenddesc.BlendOp = D3D12_BLEND_OP_ADD;
	//blenddesc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;//1.0fデストカラーの値
	//blenddesc.DestBlend = D3D12_BLEND_ZERO;//使わない

	//半透明
	//blenddesc.BlendOp = D3D12_BLEND_OP_ADD;//加算
	//blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;//ソースのα値
	//blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;//1.0fソースの値

	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);

	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpipeline.NumRenderTargets = 1;//描画対象は1つ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;//0～255指定のRGBA
	gpipeline.SampleDesc.Count = 1;//1ぴくせるにつき1回サンプリング


	PipelineSet pipelineSet;

	CD3DX12_DESCRIPTOR_RANGE descRangeCBV, descRangeSRV;
	descRangeCBV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	descRangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER rootparm[2];
	rootparm[0].InitAsConstantBufferView(0);
	rootparm[1].InitAsDescriptorTable(1, &descRangeSRV);

#pragma endregion
#pragma region シグネチャー
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);

	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//繰り返し
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//繰り返し
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//奥行繰り返し
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//ボーダーの時は黒
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//補完しない
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;//ミニマップ最大値
	samplerDesc.MinLOD = 0.0f;//ミニマップ最小値
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//ピクセルシェーダからのみ可視

	ComPtr<ID3D12RootSignature> rootsignature;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_0(_countof(rootparm), rootparm, 1, &samplerDesc,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob>rootSigBlob;
	result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pipelineSet.rootsignature));

	// パイプラインにルートシグネチャをセット
	gpipeline.pRootSignature = pipelineSet.rootsignature.Get();
	ComPtr<ID3D12PipelineState> pipelinestate = nullptr;
	result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineSet.pipelinestate));

#pragma endregion
	return pipelineSet;
}


void DrawObject3d(object3d* object, ComPtr<ID3D12GraphicsCommandList> cmdList,
	ComPtr<ID3D12DescriptorHeap> descHeap, D3D12_VERTEX_BUFFER_VIEW& vbView,
	D3D12_INDEX_BUFFER_VIEW& ibView, D3D12_GPU_DESCRIPTOR_HANDLE
	gpuDescHandleSRV, UINT numIndices) {
	//頂点バッファの設定
	cmdList->IASetVertexBuffers(0, 1, &vbView);
	//インデックスバッファの設定
	cmdList->IASetIndexBuffer(&ibView);
	//デスクリプタヒープの配列
	ID3D12DescriptorHeap* ppHeaps[] = { descHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	//定数バッファビューをセット
	cmdList->SetGraphicsRootConstantBufferView(0, object->constBUff->GetGPUVirtualAddress());
	//シェーダリソースビューをセット
	cmdList->SetGraphicsRootDescriptorTable(1, gpuDescHandleSRV);
	//描画コマンド
	cmdList->DrawIndexedInstanced(numIndices, 2, 0, 0, 0);
}


//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	WinApp* winApp = nullptr;
	winApp = new WinApp();
	winApp->Initialize();

	//DirectX初期化処理　ここから
#ifdef _DEBUG
	//デバッグレイヤーをオンに
	ID3D12Debug* debugContoroller;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugContoroller)))) {
		debugContoroller->EnableDebugLayer();
	}
#endif // DEBUG

	HRESULT result;
	ComPtr<ID3D12Device>dev;
	ComPtr<IDXGIFactory6>dxgiFactory;
	ComPtr<IDXGISwapChain4>swapchain;
	ComPtr<ID3D12CommandAllocator>cmdAllocator;
	ComPtr<ID3D12GraphicsCommandList>cmdList;
	ComPtr<ID3D12CommandQueue>cmdQueue;
	ComPtr<ID3D12DescriptorHeap>rtvHeaps;
	//DXGIファクトリーの生成
	result = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	//アダプターの列挙用
	std::vector<ComPtr<IDXGIAdapter1>>adapters;
	//ここに特定の名前を持つアダプターオブジェクトが入る
	ComPtr<IDXGIAdapter1>tmpAdapter;
	for (int i = 0; dxgiFactory->EnumAdapters1(i, &tmpAdapter) !=
		DXGI_ERROR_NOT_FOUND;
		i++) {
		adapters.push_back(tmpAdapter);//動的配列に追加する
	}

	for (int i = 0; i < adapters.size(); i++) {
		DXGI_ADAPTER_DESC1 adesc;
		adapters[i]->GetDesc1(&adesc);//アダプターの情報を取得
		//ソフトウェアデバイスを回避
		if (adesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		std::wstring strDesc = adesc.Description;//アダプター名
		if (strDesc.find(L"Intel") == std::wstring::npos) {
			tmpAdapter = adapters[i];
			break;
		}
	}
	//対応レベルの配列
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_FEATURE_LEVEL featureLevel;

	for (int i = 0; i < _countof(levels); i++) {
		//採用したアダプタでデバイスを生成
		result = D3D12CreateDevice(tmpAdapter.Get(), levels[i], IID_PPV_ARGS(&dev));
		if (result == S_OK) {
			//デバイスを生成出来た時点でループを抜ける
			featureLevel = levels[i];
			break;
		}
	}

	//コマンドアロケータを生成
	result = dev->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&cmdAllocator)
	);
	//コマンドリストを生成
	result = dev->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		cmdAllocator.Get(), nullptr,
		IID_PPV_ARGS(&cmdList));

	//標準設定でコマンドキューを生成
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc{};
	dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue));

	//各種設定をしてスワップチェーンを生成
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
	swapchainDesc.Width = 1280;
	swapchainDesc.Height = 720;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//色情報の書式
	swapchainDesc.SampleDesc.Count = 1;//マルチサンプルしない
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;//バックバッファ用
	swapchainDesc.BufferCount = 2;//バッファ数を2つに設定
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//フリップ後は破棄
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ComPtr<IDXGISwapChain1>swapchain1;

	dxgiFactory->CreateSwapChainForHwnd(
		cmdQueue.Get(),
		winApp->GetHwnd(),
		&swapchainDesc,
		nullptr,
		nullptr,
		&swapchain1
	);

	swapchain1.As(&swapchain);

	//各種設定をしてデスクリプタヒープを生成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//レンダーターゲットビュー
	heapDesc.NumDescriptors = 2;//裏表の2つ
	dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	//裏表の2つ分について
	//std::vector<ID3D12Resource*>backBuffers(2);
	std::vector<ComPtr<ID3D12Resource>>backBuffers(2);
	for (int i = 0; i < 2; i++) {
		//スワップチェーンからバッファを取得
		result = swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));
		//デスクリプタヒープのハンドルを取得
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeaps->GetCPUDescriptorHandleForHeapStart(),
			//裏か表かアドレスがずれる
			i, dev->GetDescriptorHandleIncrementSize(heapDesc.Type));
		//レンダーターゲットビューの生成
		dev->CreateRenderTargetView(
			backBuffers[i].Get(),
			nullptr,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeaps->GetCPUDescriptorHandleForHeapStart(),
				i,
				dev->GetDescriptorHandleIncrementSize(heapDesc.Type)
			)
		);
	}
	//フェンスの生成
	ComPtr<ID3D12Fence>fence;
	UINT64 fenceVal = 0;

	result = dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	//DirectX初期化処理　ここまで

	//描画初期化処理
#pragma region 読み込み
	//WICテクスチャのロード
	TexMetadata metadata{};
	ScratchImage scratchImg{};

	result = LoadFromWICFile(
		L"Resources/backscreen.png",
		WIC_FLAGS_NONE,
		&metadata, scratchImg
	);

	const Image* img = scratchImg.GetImage(0, 0, 0);

	//リソース設定
	CD3DX12_RESOURCE_DESC texresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format,
		metadata.width,
		(UINT)metadata.height,
		(UINT16)metadata.arraySize,
		(UINT16)metadata.mipLevels
	);
	//テクスチャ用のバッファの生成
	ComPtr<ID3D12Resource>texbuff = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	//テクスチャバッファにデータ転送
	result = texbuff->WriteToSubresource(
		0,
		nullptr,//全領域へコピー
		img->pixels,//元データアドレス
		(UINT)img->rowPitch,//1ラインサイズ
		(UINT)img->slicePitch//全サイズ
	);
#pragma endregion
#pragma region 読み込み2
	TexMetadata metadata1{};
	ScratchImage scratchImg1{};

	result = LoadFromWICFile(
		L"Resources/syouzyun.png",
		WIC_FLAGS_NONE,
		&metadata1, scratchImg1
	);
	if (FAILED(result)) {
		return result;
	}
	const Image* img1 = scratchImg1.GetImage(0, 0, 0);

	//リソース設定
	CD3DX12_RESOURCE_DESC texresDesc1 = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata1.format,
		metadata1.width,
		(UINT)metadata1.height,
		(UINT16)metadata1.arraySize,
		(UINT16)metadata1.mipLevels
	);
	//テクスチャ用のバッファの生成
	ComPtr<ID3D12Resource>texbuff1 = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc1,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texbuff1)
	);

	//テクスチャバッファにデータ転送
	result = texbuff1->WriteToSubresource(
		0,
		nullptr,//全領域へコピー
		img1->pixels,//元データアドレス
		(UINT)img1->rowPitch,//1ラインサイズ
		(UINT)img1->slicePitch//全サイズ
	);
#pragma endregion
#pragma region 読み込み3
	TexMetadata metadata2{};
	ScratchImage scratchImg2{};

	result = LoadFromWICFile(
		L"Resources/fake.png",
		WIC_FLAGS_NONE,
		&metadata2, scratchImg2
	);
	if (FAILED(result)) {
		return result;
	}
	const Image* img2 = scratchImg2.GetImage(0, 0, 0);

	//リソース設定
	CD3DX12_RESOURCE_DESC texresDesc2 = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata2.format,
		metadata2.width,
		(UINT)metadata2.height,
		(UINT16)metadata2.arraySize,
		(UINT16)metadata2.mipLevels
	);
	//テクスチャ用のバッファの生成
	ComPtr<ID3D12Resource>texbuff2 = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc2,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texbuff2)
	);

	//テクスチャバッファにデータ転送
	result = texbuff2->WriteToSubresource(
		0,
		nullptr,//全領域へコピー
		img2->pixels,//元データアドレス
		(UINT)img2->rowPitch,//1ラインサイズ
		(UINT)img2->slicePitch//全サイズ
	);
#pragma endregion
#pragma region 読み込み4
	TexMetadata metadata3{};
	ScratchImage scratchImg3{};

	result = LoadFromWICFile(
		L"Resources/syouzyunRed.png",
		WIC_FLAGS_NONE,
		&metadata3, scratchImg3
	);
	if (FAILED(result)) {
		return result;
	}

	const Image* img3 = scratchImg3.GetImage(0, 0, 0);


	//リソース設定
	CD3DX12_RESOURCE_DESC texresDesc3 = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata3.format,
		metadata3.width,
		(UINT)metadata3.height,
		(UINT16)metadata3.arraySize,
		(UINT16)metadata3.mipLevels
	);
	//テクスチャ用のバッファの生成
	ComPtr<ID3D12Resource>texbuff3 = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc3,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texbuff3)
	);

	//テクスチャバッファにデータ転送
	result = texbuff3->WriteToSubresource(
		0,
		nullptr,//全領域へコピー
		img3->pixels,//元データアドレス
		(UINT)img3->rowPitch,//1ラインサイズ
		(UINT)img3->slicePitch//全サイズ
	);
#pragma endregion
#pragma region 読み込み
	//WICテクスチャのロード
	TexMetadata metadata4{};
	ScratchImage scratchImg4{};

	result = LoadFromWICFile(
		L"Resources/bullet.png",
		WIC_FLAGS_NONE,
		&metadata4, scratchImg4
	);

	const Image* img4 = scratchImg4.GetImage(0, 0, 0);

	//リソース設定
	CD3DX12_RESOURCE_DESC texresDesc4 = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata4.format,
		metadata4.width,
		(UINT)metadata4.height,
		(UINT16)metadata4.arraySize,
		(UINT16)metadata4.mipLevels
	);
	//テクスチャ用のバッファの生成
	ComPtr<ID3D12Resource>texbuff4 = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc4,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texbuff4)
	);

	//テクスチャバッファにデータ転送
	result = texbuff4->WriteToSubresource(
		0,
		nullptr,//全領域へコピー
		img4->pixels,//元データアドレス
		(UINT)img4->rowPitch,//1ラインサイズ
		(UINT)img4->slicePitch//全サイズ
	);
#pragma endregion
	const float topHeight = 5.0f;
	const int DIV = 4;
	const float radius = 3;
	const int  constantBufferNum = 128;

#pragma region 立方体
	//頂点データ
	Vertex vertices[] = {
		{{-0.5f,-0.5f,5.0f},{0,0,0}, {0.0f,1.0f}},
		{{-0.5f,+0.5f,5.0f },{0,0,0},{0.0f,0.0f}},
		{{+0.5f,-0.5f,5.0f},{0,0,0},{1.0f,1.0f}},
		{{+0.5f,+0.5f,5.0f},{0,0,0},{1.0f,0.0f}},
	};

	unsigned short indices[] = {
		0,1,2,
		2,1,3,
	};
#pragma endregion
#pragma region 頂点データ	
	//頂点データ全体のサイズ=頂点データ一つ文のサイズ*頂点データの要素数
	UINT sizeVB = static_cast<UINT>(sizeof(Vertex) * _countof(vertices));
	ComPtr<ID3D12Resource>vertBuff;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeVB),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

	// GPU上のバッファに対応した仮想メモリを取得
	Vertex* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);

	// 全頂点に対して
	for (int i = 0; i < _countof(vertices); i++) {
		vertMap[i] = vertices[i];   // 座標をコピー
	}

	// マップを解除
	vertBuff->Unmap(0, nullptr);
#pragma endregion
#pragma region インデックスデータ
	//インデックスデータ全体のサイズ
	UINT sizeIB = static_cast<UINT>(sizeof(unsigned short) * _countof(indices));
	//インデックスバッファの設定
	//インデックスバッファの生成
	ComPtr<ID3D12Resource>indexBuff;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeIB),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBuff)
	);

	//GPU上のバッファに対応した仮想メモリの取得
	unsigned short* indexMap = nullptr;
	result = indexBuff->Map(0, nullptr, (void**)&indexMap);

	//全インデックスに対して
	for (int i = 0; i < _countof(indices); i++) {
		indexMap[i] = indices[i];//インデックスをコピー
	}
#pragma endregion
	D3D12_INDEX_BUFFER_VIEW ibView{};
	ibView.BufferLocation = indexBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeIB;

	// 頂点バッファビューの作成
	D3D12_VERTEX_BUFFER_VIEW vbView{};

	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeVB;
	vbView.StrideInBytes = sizeof(Vertex);

	//ヒープ設定
	D3D12_HEAP_PROPERTIES cbheapprop{};//ヒープ設定
	cbheapprop.Type = D3D12_HEAP_TYPE_UPLOAD;//GPUへの転送用
	//リソース設定
	D3D12_RESOURCE_DESC cbresdesc{};//リソース設定
	cbresdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	cbresdesc.Width = (sizeof(ConstBufferData) + 0xff) & ~0xff;//256バイトアラインメント
	cbresdesc.Height = 1;
	cbresdesc.DepthOrArraySize = 1;
	cbresdesc.MipLevels = 1;
	cbresdesc.SampleDesc.Count = 1;
	cbresdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> depthBuffer;
	//深度バッファリソース設定
	CD3DX12_RESOURCE_DESC depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		WinApp::window_width,
		WinApp::window_height,
		1, 0,
		1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	//深度バッファの設定
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,//深度値書き込みに使用
		&CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0),
		IID_PPV_ARGS(&depthBuffer)
	);
	//深度ビュー用デスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ComPtr<ID3D12DescriptorHeap> dsvHeap = nullptr;
	result = dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

	//深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dev->CreateDepthStencilView(
		depthBuffer.Get(),
		&dsvDesc,
		dsvHeap->GetCPUDescriptorHandleForHeapStart()
	);

	XMMATRIX matProjection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(60.0f),
		(float)WinApp::window_width / WinApp::window_height,
		0.1f, 1000.0f
	);
	//ビュー変換行列
	XMMATRIX matView;
	XMFLOAT3 eye(0, 0, -50);//視点座標
	XMFLOAT3 target(0, 0, 0);//注視点座標
	XMFLOAT3 up(0, 1, 0);//上方向ベクトル
	matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

	XMMATRIX bulletView;
	XMFLOAT3 eyess(0, 0, -50);//視点座標
	XMFLOAT3 targetss(0, 0, 0);//注視点座標
	XMFLOAT3 upss(0, 1, 0);//上方向ベクトル
	bulletView = XMMatrixLookAtLH(XMLoadFloat3(&eyess), XMLoadFloat3(&targetss), XMLoadFloat3(&upss));
	const int OBJECT_NUM = 10;
	Input* input = nullptr;
	// 入力の初期化
	input = new Input();
	if (!input->Initialize(winApp)) {
		assert(0);
		return 1;
	}

	//照準(プレイヤー)
	object3d playerobject[2];
	//背景
	object3d haikei3d;
	//疑似3D
	object3d object3dsSqure[OBJECT_NUM];
	//疑似3D
	object3d object3dsSqure1[OBJECT_NUM];
	//定数バッファ用のデスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> basicDescHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> basicDescHeap1 = nullptr;
	ComPtr<ID3D12DescriptorHeap> basicDescHeap2 = nullptr;
	ComPtr<ID3D12DescriptorHeap> basicDescHeap3 = nullptr;
	ComPtr<ID3D12DescriptorHeap> basicDescHeap4 = nullptr;
#pragma region 
	//設定構造体
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダーから見える
	descHeapDesc.NumDescriptors = constantBufferNum + 1;//定数バッファの数
	//デスクリプタヒープの生成
	result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));

#pragma endregion
#pragma region 
	//設定構造体
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc1 = {};
	descHeapDesc1.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc1.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダーから見える
	descHeapDesc1.NumDescriptors = constantBufferNum + 1;//定数バッファの数
	//デスクリプタヒープの生成
	result = dev->CreateDescriptorHeap(&descHeapDesc1, IID_PPV_ARGS(&basicDescHeap1));
#pragma endregion
#pragma region 
	//設定構造体
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc2 = {};
	descHeapDesc2.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc2.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダーから見える
	descHeapDesc2.NumDescriptors = constantBufferNum + 1;//定数バッファの数
	//デスクリプタヒープの生成
	result = dev->CreateDescriptorHeap(&descHeapDesc2, IID_PPV_ARGS(&basicDescHeap2));
#pragma endregion
#pragma region 
	//設定構造体
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc3 = {};
	descHeapDesc3.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc3.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダーから見える
	descHeapDesc3.NumDescriptors = constantBufferNum + 1;//定数バッファの数
	//デスクリプタヒープの生成
	result = dev->CreateDescriptorHeap(&descHeapDesc3, IID_PPV_ARGS(&basicDescHeap3));
#pragma endregion
#pragma region 
	//設定構造体
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc4 = {};
	descHeapDesc4.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc4.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダーから見える
	descHeapDesc4.NumDescriptors = constantBufferNum + 1;//定数バッファの数
	//デスクリプタヒープの生成
	result = dev->CreateDescriptorHeap(&descHeapDesc4, IID_PPV_ARGS(&basicDescHeap4));

#pragma endregion
	InitializeObject3d(&haikei3d, 1, dev.Get(), basicDescHeap.Get());

	haikei3d.scale = { 620,620,0 };
	haikei3d.position = { 0,-10,200 };
	haikei3d.rotation = { 0,0,0 };
	for (int i = 0; i < _countof(object3dsSqure); i++) {
		InitializeObject3d(&object3dsSqure[i], i, dev.Get(), basicDescHeap.Get());
		object3dsSqure[i].scale = { 1,1,0 };
		object3dsSqure[i].position = { 31.0f,-16.0f,-50.0f + 30 * i };
		object3dsSqure[i].position = { 30.0f,-15.0f,-50.0f + 30 * i };
	}

	for (int i = 0; i < _countof(object3dsSqure1); i++) {
		InitializeObject3d(&object3dsSqure1[i], i, dev.Get(), basicDescHeap4.Get());

		object3dsSqure1[i].scale = { 1,1,0 };

		object3dsSqure1[i].position = { -31.0f ,-16.0f ,-50.0f + 30 * i };
	}
	for (int i = 0; i < _countof(playerobject); i++) {
		InitializeObject3d(&playerobject[i], i, dev.Get(), basicDescHeap4.Get());
		playerobject[0].scale = { 1.0f,1.0f,0.0f };
		playerobject[0].position = { 0.0f,0.0f,-45.0f };
		playerobject[1].scale = { 2.0f,2.0f,0.0f };
		playerobject[1].position = { 0.0f,0.0f,0.0f };
	}

	result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));
#pragma region ハンドル取得

	//デスクリプタヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandleSRV =
		basicDescHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleSRV =
		basicDescHeap->GetGPUDescriptorHandleForHeapStart();
	//ハンドルアドレスを進める
	cpuDescHandleSRV.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescHandleSRV.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

#pragma endregion
#pragma region ハンドル取得2
	//デスクリプタヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandleSRV1 =
		basicDescHeap1->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleSRV1 =
		basicDescHeap1->GetGPUDescriptorHandleForHeapStart();
	//ハンドルアドレスを進める
	cpuDescHandleSRV1.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescHandleSRV1.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
#pragma endregion
#pragma region ハンドル取得3
	//デスクリプタヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandleSRV2 =
		basicDescHeap2->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleSRV2 =
		basicDescHeap2->GetGPUDescriptorHandleForHeapStart();
	//ハンドルアドレスを進める
	cpuDescHandleSRV2.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescHandleSRV2.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
#pragma endregion
#pragma region ハンドル取得4
	//デスクリプタヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandleSRV3 =
		basicDescHeap3->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleSRV3 =
		basicDescHeap3->GetGPUDescriptorHandleForHeapStart();
	//ハンドルアドレスを進める
	cpuDescHandleSRV3.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescHandleSRV3.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
#pragma endregion
#pragma region ハンドル取得5
	//デスクリプタヒープの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandleSRV4 =
		basicDescHeap4->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleSRV4 =
		basicDescHeap4->GetGPUDescriptorHandleForHeapStart();
	//ハンドルアドレスを進める
	cpuDescHandleSRV4.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescHandleSRV4.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
#pragma endregion
#pragma region ビュー設定
	//シェーダリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};//設定構造体
	srvDesc.Format = metadata.format;//RGBA
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;

	//デスクリプタの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE basicHeaphandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();
	//ハンドルのアドレスを進める
	basicHeaphandle.ptr += 2 * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//ヒープの2番目にシェーダリソースビュー作成
	dev->CreateShaderResourceView(texbuff.Get(),//ビューと関連付けるバッファ
		&srvDesc,//テクスチャ設定情報
		cpuDescHandleSRV
	);
#pragma endregion

#pragma region ビュー設定2
	//シェーダリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc1{};//設定構造体
	srvDesc1.Format = metadata1.format;//RGBA
	srvDesc1.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc1.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc1.Texture2D.MipLevels = 1;

	//デスクリプタの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE basicHeaphandle1 = basicDescHeap1->GetCPUDescriptorHandleForHeapStart();
	//ハンドルのアドレスを進める
	basicHeaphandle1.ptr += 2 * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//ヒープの2番目にシェーダリソースビュー作成
	dev->CreateShaderResourceView(texbuff1.Get(),//ビューと関連付けるバッファ
		&srvDesc1,//テクスチャ設定情報
		cpuDescHandleSRV1
	);
#pragma endregion

#pragma region ビュー設定3
	//シェーダリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};//設定構造体
	srvDesc2.Format = metadata2.format;//RGBA
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = 1;

	//デスクリプタの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE basicHeaphandle2 = basicDescHeap2->GetCPUDescriptorHandleForHeapStart();
	//ハンドルのアドレスを進める
	basicHeaphandle2.ptr += 2 * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//ヒープの2番目にシェーダリソースビュー作成
	dev->CreateShaderResourceView(texbuff2.Get(),//ビューと関連付けるバッファ
		&srvDesc2,//テクスチャ設定情報
		cpuDescHandleSRV2
	);
#pragma endregion

#pragma region ビュー設定4

	//シェーダリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};//設定構造体
	srvDesc3.Format = metadata3.format;//RGBA
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc3.Texture2D.MipLevels = 1;

	//デスクリプタの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE basicHeaphandle3 = basicDescHeap3->GetCPUDescriptorHandleForHeapStart();
	//ハンドルのアドレスを進める
	basicHeaphandle3.ptr += 2 * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//ヒープの2番目にシェーダリソースビュー作成
	dev->CreateShaderResourceView(texbuff3.Get(),//ビューと関連付けるバッファ
		&srvDesc3,//テクスチャ設定情報
		cpuDescHandleSRV3
	);
#pragma endregion

#pragma region ビュー設定5

	//シェーダリソースビュー設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc4{};//設定構造体
	srvDesc4.Format = metadata4.format;//RGBA
	srvDesc4.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc4.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc4.Texture2D.MipLevels = 1;

	//デスクリプタの先頭ハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE basicHeaphandle4 = basicDescHeap4->GetCPUDescriptorHandleForHeapStart();
	//ハンドルのアドレスを進める
	basicHeaphandle4.ptr += 2 * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//ヒープの2番目にシェーダリソースビュー作成
	dev->CreateShaderResourceView(texbuff4.Get(),//ビューと関連付けるバッファ
		&srvDesc4,//テクスチャ設定情報
		cpuDescHandleSRV4
	);
#pragma endregion
	PipelineSet object3dPipelineSet = object3dCreateGrphicsPipeline(dev.Get());
	float angle = 0.0f;//カメラの回転角度
	XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };//スケーリング
	XMFLOAT3 rotation = { 0.0f,0.0f,0.0f };//回転
	XMFLOAT3 position = { 0.0f,0.0f,0.0f };//座標
	XMFLOAT4 color = { 1, 1, 1, 1 };
	XMFLOAT4 alpha = { 1,1,1,0.8f };
#pragma region//ゲーム変数
	const int ENEMY_MAX = 5;
#pragma endregion
	while (true)//ゲームループ
	{
#pragma region//入力・更新処理
		//DirectX毎フレーム処理 ここから
		input->Update();
		//プレイヤーの更新
		for (int i = 0; i < _countof(playerobject); i++) {
			UpdateObject3d(&playerobject[i], matView, matProjection, alpha);
		}
		UpdateObject3d(&haikei3d, matView, matProjection, color);
		//疑似3Dの更新
		for (int i = 0; i < _countof(object3dsSqure); i++) {
			object3dsSqure[i].position.z -= 3.0;
			if (object3dsSqure[i].position.z < -100) {
				object3dsSqure[i].position.z = 200;
			}
			UpdateObject3d(&object3dsSqure[i], matView, matProjection, color);
		}
		//疑似3Dの更新
		for (int i = 0; i < _countof(object3dsSqure1); i++) {
			object3dsSqure1[i].position.z -= 3.0;
			if (object3dsSqure1[i].position.z < -100) {
				object3dsSqure1[i].position.z = 200;
			}
			UpdateObject3d(&object3dsSqure1[i], matView, matProjection, color);
		}
#pragma endregion
#pragma region //ゲーム外処理
		if (winApp->ProcessMessage()) {
			break;
		}
		//バックバッファの番号を取得(2つなので0番か1番)
		UINT bbIndex =
			swapchain->GetCurrentBackBufferIndex();
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		//2.描画先指定
		//レンダーターゲットビュー用ディスクリプタヒープのハンドルを取得
		D3D12_CPU_DESCRIPTOR_HANDLE rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIndex * dev->GetDescriptorHandleIncrementSize(heapDesc.Type);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

		//3.画面クリア
		float clearColor[] = { 0.0f,0.0f,0.5f,1.0f };//青っぽい色
		cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		////4.描画コマンドここから
		cmdList->SetPipelineState(object3dPipelineSet.pipelinestate.Get());
		cmdList->SetGraphicsRootSignature(object3dPipelineSet.rootsignature.Get());
		//デスクリプタヒープをセット
		ID3D12DescriptorHeap* ppHeaps[] = { basicDescHeap.Get() };
		cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		//ビューポート
		cmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, WinApp::window_width, WinApp::window_height));
		//シザー矩形
		cmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, WinApp::window_width, WinApp::window_height));
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->IASetVertexBuffers(0, 1, &vbView);
		//4.描画コマンドここまで
#pragma endregion
#pragma region//描画
		for (int i = 0; i < _countof(object3dsSqure); i++) {
			DrawObject3d(&object3dsSqure[i], cmdList.Get(), basicDescHeap2.Get(), vbView, ibView, gpuDescHandleSRV, _countof(indices));
		}
		for (int i = 0; i < _countof(object3dsSqure1); i++) {
			DrawObject3d(&object3dsSqure1[i], cmdList.Get(), basicDescHeap2.Get(), vbView, ibView, gpuDescHandleSRV, _countof(indices));
		}

		for (int i = 0; i < _countof(playerobject); i++) {
			DrawObject3d(&playerobject[i], cmdList.Get(), basicDescHeap2.Get(), vbView, ibView, gpuDescHandleSRV, _countof(indices));
		}
#pragma endregion
#pragma region//ゲーム外処理２
		//5.リソースバリアを戻す
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		//命令のクローズ
		cmdList->Close();
		//コマンドリストの実行
		ID3D12CommandList* cmdLists[] = { cmdList.Get() };//コマンドリストの配列
		cmdQueue->ExecuteCommandLists(1, cmdLists);
		//コマンドリストの実行完了を持つ
		cmdQueue->Signal(fence.Get(), ++fenceVal);
		if (fence->GetCompletedValue() != fenceVal) {
			HANDLE event = CreateEvent(nullptr, false, false, nullptr);
			fence->SetEventOnCompletion(fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}

		cmdAllocator->Reset();//キューをクリア
		cmdList->Reset(cmdAllocator.Get(), nullptr);//再びコマンドリストを貯める準備
		//バッファをフリップ(裏表の入れ替え)
		swapchain->Present(1, 0);
#pragma endregion//ゲーム外処理
	}
	//ウィンドウ表示
	ShowWindow(winApp->GetHwnd(), SW_SHOW);
	winApp->Finalize();
	delete input;
	delete winApp;
	return 0;
}