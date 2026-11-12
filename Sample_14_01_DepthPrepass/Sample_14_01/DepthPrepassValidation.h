#pragma once

#include "DirectXTK/Inc/ScreenGrab.h"
#include <d3d12sdklayers.h>
#include <wincodec.h>
#include <string>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

// Called only by --validate, after EndFrame has waited for GPU completion.
inline int SaveDepthPrepassValidation(myRenderer::RenderingEngine& renderer,
    bool metricsAvailable, const wchar_t* viewName)
{
    CreateDirectoryW(L"validation", nullptr);
#ifdef _DEBUG
    const wchar_t* validationDirectory = L"validation/Debug";
#else
    const wchar_t* validationDirectory = L"validation/Release";
#endif
    CreateDirectoryW(validationDirectory, nullptr);
    const bool reuse = renderer.IsDepthReuseEnabled();
    const std::wstring stem = std::wstring(validationDirectory) + L"/"
        + (reuse ? L"primed" : L"baseline") + viewName;
    const auto comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto* resource = renderer.GetMainColorTexture().Get();
    auto* queue = g_graphicsEngine->GetCommandQueue();
    const auto pngResult = DirectX::SaveWICTextureToFile(queue, resource,
        GUID_ContainerFormatPng, (stem + L".png").c_str(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const auto ddsResult = DirectX::SaveDDSTextureToFile(queue, resource,
        (stem + L".dds").c_str(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (SUCCEEDED(comResult)) CoUninitialize();

    UINT64 debugErrors = 0;
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(g_graphicsEngine->GetD3DDevice()->QueryInterface(IID_PPV_ARGS(&infoQueue))))
    {
        FILE* debugFile = nullptr;
        _wfopen_s(&debugFile, (stem + L"-debug.txt").c_str(), L"wb");
        const auto messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
        for (UINT64 i = 0; i < messageCount; ++i)
        {
            SIZE_T bytes = 0;
            if (FAILED(infoQueue->GetMessage(i, nullptr, &bytes))) continue;
            std::vector<unsigned char> storage(bytes);
            auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            if (FAILED(infoQueue->GetMessage(i, message, &bytes))) continue;
            if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR
                || message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
            {
                ++debugErrors;
            }
            if (debugFile) fprintf(debugFile, "%d: %s\n", message->Severity, message->pDescription);
        }
        if (debugFile) fclose(debugFile);
    }

    FILE* report = nullptr;
    if (_wfopen_s(&report, (stem + L".json").c_str(), L"wb") != 0 || !report) return 2;
    fprintf(report,
        "{\n  \"depth_reuse\": %s,\n  \"metrics_available\": %s,\n"
        "  \"prepass_ps_invocations\": %llu,\n  \"gbuffer_ps_invocations\": %llu,\n"
        "  \"forward_ps_invocations\": %llu,\n  \"debug_errors\": %llu,\n"
        "  \"png_hresult\": %ld,\n  \"dds_hresult\": %ld\n}\n",
        reuse ? "true" : "false", metricsAvailable ? "true" : "false",
        renderer.GetPixelShaderInvocations(DepthPassMetrics::Prepass),
        renderer.GetPixelShaderInvocations(DepthPassMetrics::GBuffer),
        renderer.GetPixelShaderInvocations(DepthPassMetrics::Forward),
        debugErrors, static_cast<long>(pngResult), static_cast<long>(ddsResult));
    fclose(report);
    return metricsAvailable && debugErrors == 0 && SUCCEEDED(pngResult)
        && SUCCEEDED(ddsResult) ? 0 : 2;
}
