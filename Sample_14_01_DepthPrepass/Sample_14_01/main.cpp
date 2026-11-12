#include "stdafx.h"
#include "system/system.h"
#include "RenderingEngine.h"
#include "ModelRender.h"
#include "DepthPrepassValidation.h"

// 関数宣言
void InitRootSignature(RootSignature& rs);

///////////////////////////////////////////////////////////////////
// ウィンドウプログラムのメイン関数
///////////////////////////////////////////////////////////////////
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    const bool validate = wcsstr(lpCmdLine, L"--validate") != nullptr;
    const bool baseline = wcsstr(lpCmdLine, L"--baseline") != nullptr;
    // Validation runs have a fixed camera and terminate after 12 frames.
    InitGame(hInstance, hPrevInstance, lpCmdLine, validate ? SW_HIDE : nCmdShow,
        TEXT("Sample 14-01 - Depth Prepass Reuse"));
    const wchar_t* validationView = L"";
    if (wcsstr(lpCmdLine, L"--view-side"))
    {
        g_camera3D->SetPosition({ 200.0f, 120.0f, 180.0f });
        g_camera3D->SetTarget({ 0.0f, 50.0f, 0.0f });
        validationView = L"-side";
    }
    else if (wcsstr(lpCmdLine, L"--view-close"))
    {
        g_camera3D->SetPosition({ 0.0f, 70.0f, 110.0f });
        g_camera3D->SetTarget({ 0.0f, 50.0f, 0.0f });
        validationView = L"-close";
    }

    //////////////////////////////////////
    // ここから初期化を行うコードを記述する
    //////////////////////////////////////

    // ルートシグネチャを作成
    RootSignature rootSignature;
    InitRootSignature(rootSignature);

    //レンダリングエンジンを初期化
    myRenderer::RenderingEngine renderingEngine;
    renderingEngine.Init();
    renderingEngine.SetDepthReuseEnabled(!baseline);

    // 背景モデルのレンダラーを初期化
    myRenderer::ModelRender bgModelRender;
    bgModelRender.InitDeferredRendering(renderingEngine, "Assets/modelData/bg/bg.tkm", true);

    // step-1 ティーポットの描画処理を初期化する
    myRenderer::ModelInitDataFR modelInitData;
    modelInitData.m_tkmFilePath = "Assets/modelData/teapot.tkm";
    modelInitData.m_fxFilePath = "Assets/shader/sample.fx";

    // 拡張SRVにZPrepassで作成された深度テクスチャを指定する
    modelInitData.m_expandShaderResoruceView[0] = &renderingEngine.GetZPrepassDepthTexture();

    // 初期化情報を使って描画処理を初期化する
    myRenderer::ModelRender teapotModelRender;

    // InitForwardRendering()を利用すると、
    // フォワードレンダリングの描画パスで描画される
    teapotModelRender.InitForwardRendering(renderingEngine, modelInitData);

    //シャドウキャスターフラグをオンにする
    teapotModelRender.SetShadowCasterFlag(true);

    teapotModelRender.UpdateWorldMatrix({ 0.0f, 50.0f, 0.0f }, g_quatIdentity, g_vec3One);

    //////////////////////////////////////
    // 初期化を行うコードを書くのはここまで！！！
    //////////////////////////////////////
    auto& renderContext = g_graphicsEngine->GetRenderContext();

    unsigned int renderedFrames = 0;
    // ここからゲームループ
    while (DispatchWindowMessage())
    {
        // レンダリング開始
        g_engine->BeginFrame();
        if (!validate)
        {
            if (GetAsyncKeyState(VK_F1) & 1)
            {
                renderingEngine.SetDepthReuseEnabled(!renderingEngine.IsDepthReuseEnabled());
            }
            g_camera3D->MoveForward(g_pad[0]->GetLStickYF());
            g_camera3D->MoveRight(g_pad[0]->GetLStickXF());
            g_camera3D->MoveUp(g_pad[0]->GetRStickYF());
        }

        //////////////////////////////////////
        // ここから絵を描くコードを記述する
        //////////////////////////////////////

        bgModelRender.Draw();

        // step-2 ティーポットを描画する
        teapotModelRender.Draw();

        //レンダリングエンジンを実行
        renderingEngine.Execute(renderContext);

        /////////////////////////////////////////
        // 絵を描くコードを書くのはここまで！！！
        //////////////////////////////////////
        // レンダリング終了
        g_engine->EndFrame();
        // EndFrame waits for the GPU, so the query results are now safe to read.
        const bool metricsAvailable = renderingEngine.ReadDepthPassMetrics();
        wchar_t title[256];
        swprintf_s(title, L"Depth Prepass: %s | GBuffer PS: %llu | Forward PS: %llu | F1: toggle",
            renderingEngine.IsDepthReuseEnabled() ? L"Reuse ON" : L"Baseline",
            renderingEngine.GetPixelShaderInvocations(DepthPassMetrics::GBuffer),
            renderingEngine.GetPixelShaderInvocations(DepthPassMetrics::Forward));
        SetWindowTextW(g_hWnd, title);
        if (validate && ++renderedFrames == 12)
        {
            return SaveDepthPrepassValidation(renderingEngine, metricsAvailable, validationView);
        }
    }

    return 0;
}

// ルートシグネチャの初期化
void InitRootSignature(RootSignature& rs)
{
    rs.Init(D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);
}
