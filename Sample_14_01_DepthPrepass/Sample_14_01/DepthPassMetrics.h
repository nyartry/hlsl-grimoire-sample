#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <array>

// Counts actual pixel-shader invocations, including helper invocations if the GPU
// reports them. This is a work counter, not a GPU execution-time measurement.
class DepthPassMetrics {
public:
    enum Pass { Prepass = 0, GBuffer = 1, Forward = 2, Count = 3 };

    bool Init(ID3D12Device* device)
    {
        m_queryHeap.Reset();
        m_readback.Reset();
        m_statistics = {};
        m_ended = {};
        m_resolved = {};
        m_activePass = -1;
        m_resolvePending = false;
        if (device == nullptr) {
            return false;
        }

        D3D12_QUERY_HEAP_DESC queryDesc = {};
        queryDesc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
        queryDesc.Count = Count;
        if (FAILED(device->CreateQueryHeap(
            &queryDesc, IID_PPV_ARGS(m_queryHeap.GetAddressOf())))) {
            return false;
        }

        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS) * Count;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(device->CreateCommittedResource(
            &heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(m_readback.GetAddressOf())))) {
            m_queryHeap.Reset();
            return false;
        }
        return true;
    }

    void Begin(ID3D12GraphicsCommandList* commandList, Pass pass)
    {
        const int index = static_cast<int>(pass);
        if (!IsReady() || commandList == nullptr || !IsValid(index)
            || m_activePass != -1) {
            return;
        }
        m_ended[index] = false;
        m_activePass = index;
        commandList->BeginQuery(m_queryHeap.Get(),
            D3D12_QUERY_TYPE_PIPELINE_STATISTICS, static_cast<UINT>(index));
    }

    void End(ID3D12GraphicsCommandList* commandList, Pass pass)
    {
        const int index = static_cast<int>(pass);
        if (!IsReady() || commandList == nullptr || !IsValid(index)
            || m_activePass != index) {
            return;
        }
        commandList->EndQuery(m_queryHeap.Get(),
            D3D12_QUERY_TYPE_PIPELINE_STATISTICS, static_cast<UINT>(index));
        m_ended[index] = true;
        m_activePass = -1;
    }

    void Resolve(ID3D12GraphicsCommandList* commandList)
    {
        if (!IsReady() || commandList == nullptr || m_activePass != -1) {
            return;
        }
        // Only queries that have completed Begin/End may be resolved.
        m_resolved = m_ended;
        for (UINT index = 0; index < Count; ++index) {
            if (m_ended[index]) {
                commandList->ResolveQueryData(m_queryHeap.Get(),
                    D3D12_QUERY_TYPE_PIPELINE_STATISTICS, index, 1,
                    m_readback.Get(),
                    sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS) * index);
            }
        }
        m_ended = {};
        m_resolvePending = true;
    }

    // The caller must submit Resolve's command list and wait for its GPU fence
    // before calling this method. In this sample EndFrame() performs that wait.
    bool ReadAfterGpuWait()
    {
        if (!IsReady() || !m_resolvePending) {
            return false;
        }
        const D3D12_RANGE readRange = {
            0, sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS) * Count
        };
        void* mapped = nullptr;
        if (FAILED(m_readback->Map(0, &readRange, &mapped))) {
            return false;
        }
        const auto* results =
            static_cast<const D3D12_QUERY_DATA_PIPELINE_STATISTICS*>(mapped);
        m_statistics = {};
        for (int index = 0; index < Count; ++index) {
            if (m_resolved[index]) {
                m_statistics[index] = results[index];
            }
        }
        const D3D12_RANGE writtenRange = { 0, 0 };
        m_readback->Unmap(0, &writtenRange);
        m_resolvePending = false;
        return true;
    }

    UINT64 GetPixelShaderInvocations(Pass pass) const
    {
        const int index = static_cast<int>(pass);
        return IsValid(index) ? m_statistics[index].PSInvocations : 0;
    }

private:
    static bool IsValid(int index)
    {
        return index >= 0 && index < Count;
    }

    bool IsReady() const
    {
        return m_queryHeap.Get() != nullptr && m_readback.Get() != nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_readback;
    std::array<D3D12_QUERY_DATA_PIPELINE_STATISTICS, Count> m_statistics = {};
    std::array<bool, Count> m_ended = {};
    std::array<bool, Count> m_resolved = {};
    int m_activePass = -1;
    bool m_resolvePending = false;
};
