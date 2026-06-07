#include "media_player.h"
#include <mferror.h>
#include <iostream>

MediaPlayer::~MediaPlayer() {
    Finalize();
}

bool MediaPlayer::Initialize(const wchar_t* filePath, ID3D11Device* device) {
    HRESULT hr;

#ifdef _DEBUG
    DWORD attr = GetFileAttributesW(filePath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        OutputDebugStringA("MediaPlayer: File NOT FOUND\n");
        return false;
    }
    OutputDebugStringA("MediaPlayer: File exists OK\n");
#endif

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
#ifdef _DEBUG
        char buf[256];
        sprintf_s(buf, "MediaPlayer: MFStartup FAILED (hr=0x%08X)\n", hr);
        OutputDebugStringA(buf);
#endif
        return false;
    }
    m_mfStarted = true;

    IMFAttributes* pAttributes = nullptr;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) {
#ifdef _DEBUG
        OutputDebugStringA("MediaPlayer: MFCreateAttributes FAILED\n");
#endif
        return false;
    }
    pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

    hr = MFCreateSourceReaderFromURL(filePath, pAttributes, &m_pReader);
    pAttributes->Release();
    if (FAILED(hr)) {
#ifdef _DEBUG
        char buf[256];
        sprintf_s(buf, "MediaPlayer: MFCreateSourceReaderFromURL FAILED (hr=0x%08X)\n", hr);
        OutputDebugStringA(buf);
#endif
        return false;
    }

#ifdef _DEBUG
    OutputDebugStringA("MediaPlayer: SourceReader created OK\n");
#endif

    // 出力フォーマット設定（RGB32 → YUY2 → NV12 の順で試す）
    IMFMediaType* pType = nullptr;
    hr = MFCreateMediaType(&pType);
    if (FAILED(hr)) {
#ifdef _DEBUG
        OutputDebugStringA("MediaPlayer: MFCreateMediaType FAILED\n");
#endif
        return false;
    }

    pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);

    const GUID formats[] = {
        MFVideoFormat_RGB32,
        MFVideoFormat_YUY2,
        MFVideoFormat_NV12,
    };
    const char* formatNames[] = {
        "RGB32", "YUY2", "NV12"
    };

    bool formatFound = false;
    m_outputFormat = OutputFormat::UNKNOWN;

    for (int i = 0; i < 3; ++i) {
        pType->SetGUID(MF_MT_SUBTYPE, formats[i]);
        hr = m_pReader->SetCurrentMediaType(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType);
        if (SUCCEEDED(hr)) {
#ifdef _DEBUG
            char buf[128];
            sprintf_s(buf, "MediaPlayer: Using format %s\n", formatNames[i]);
            OutputDebugStringA(buf);
#endif
            if (formats[i] == MFVideoFormat_RGB32)  m_outputFormat = OutputFormat::RGB32;
            else if (formats[i] == MFVideoFormat_YUY2) m_outputFormat = OutputFormat::YUY2;
            else if (formats[i] == MFVideoFormat_NV12) m_outputFormat = OutputFormat::NV12;
            formatFound = true;
            break;
        }
    }

    pType->Release();

    if (!formatFound) {
#ifdef _DEBUG
        OutputDebugStringA("MediaPlayer: No supported output format found\n");
#endif
        return false;
    }

    IMFMediaType* pCurrentType = nullptr;
    hr = m_pReader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
    if (FAILED(hr)) {
#ifdef _DEBUG
        OutputDebugStringA("MediaPlayer: GetCurrentMediaType FAILED\n");
#endif
        return false;
    }

    UINT32 width = 0, height = 0;
    hr = MFGetAttributeSize(pCurrentType, MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
#ifdef _DEBUG
        OutputDebugStringA("MediaPlayer: Failed to get video size\n");
#endif
        pCurrentType->Release();
        return false;
    }
    m_videoWidth = width;
    m_videoHeight = height;

    UINT32 num = 0, den = 0;
    MFGetAttributeRatio(pCurrentType, MF_MT_FRAME_RATE, &num, &den);
    m_frameDuration = (num > 0 && den > 0) ? (double)den / (double)num : 1.0 / 30.0;

    INT32 stride = 0;
    hr = pCurrentType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&stride);
    if (FAILED(hr)) {
        if (m_outputFormat == OutputFormat::RGB32) {
            stride = -(INT32)(width * 4);
        } else if (m_outputFormat == OutputFormat::YUY2) {
            stride = (INT32)(width * 2);
        } else {
            stride = (INT32)width;
        }
    }
    m_isBottomUp = (stride < 0);
    m_stride = (stride < 0) ? (UINT)(-stride) : (UINT)stride;

#ifdef _DEBUG
    char dbg[256];
    sprintf_s(dbg, "MediaPlayer: %ux%u, fps=%u/%u, stride=%d, bottomUp=%d\n",
        width, height, num, den, stride, m_isBottomUp ? 1 : 0);
    OutputDebugStringA(dbg);
#endif

    pCurrentType->Release();

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = m_videoWidth;
    texDesc.Height = m_videoHeight;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateTexture2D(&texDesc, nullptr, &m_pTexture);
    if (FAILED(hr)) {
#ifdef _DEBUG
        OutputDebugStringA("MediaPlayer: CreateTexture2D FAILED\n");
#endif
        return false;
    }

    hr = device->CreateShaderResourceView(m_pTexture, nullptr, &m_pSRV);
    if (FAILED(hr)) {
#ifdef _DEBUG
        OutputDebugStringA("MediaPlayer: CreateSRV FAILED\n");
#endif
        return false;
    }

    m_isPlaying = false;
    m_isFinished = false;
    m_elapsed = 0.0;

#ifdef _DEBUG
    OutputDebugStringA("MediaPlayer: Initialize SUCCESS\n");
#endif

    return true;
}

void MediaPlayer::Finalize() {
    if (m_pSRV) { m_pSRV->Release(); m_pSRV = nullptr; }
    if (m_pTexture) { m_pTexture->Release(); m_pTexture = nullptr; }
    if (m_pReader) { m_pReader->Release(); m_pReader = nullptr; }
    if (m_mfStarted) {
        MFShutdown();
        m_mfStarted = false;
    }
}

bool MediaPlayer::Update(double deltaTime) {
    if (!m_isPlaying || m_isFinished) return false;

    m_elapsed += deltaTime;

    if (m_elapsed >= m_frameDuration) {
        m_elapsed -= m_frameDuration;

        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        m_pTexture->GetDevice(&device);
        device->GetImmediateContext(&context);

        bool ok = DecodeNextFrame(device, context);

        context->Release();
        device->Release();

        return ok;
    }

    return true;
}

bool MediaPlayer::DecodeNextFrame(ID3D11Device* device, ID3D11DeviceContext* context) {
    DWORD streamIndex = 0, flags = 0;
    LONGLONG timestamp = 0;
    IMFSample* pSample = nullptr;

    HRESULT hr = m_pReader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0, &streamIndex, &flags, &timestamp, &pSample);

    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
        m_isFinished = true;
        if (pSample) pSample->Release();
        return false;
    }

    if (!pSample) return true;

    IMFMediaBuffer* pBuffer = nullptr;
    pSample->ConvertToContiguousBuffer(&pBuffer);

    BYTE* pData = nullptr;
    DWORD maxLen = 0, curLen = 0;
    pBuffer->Lock(&pData, &maxLen, &curLen);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(m_pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        switch (m_outputFormat) {
        case OutputFormat::RGB32:
            CopyRGB32(pData, mapped);
            break;
        case OutputFormat::YUY2:
            CopyYUY2(pData, mapped);
            break;
        case OutputFormat::NV12:
            CopyNV12(pData, mapped);
            break;
        default:
            break;
        }
        context->Unmap(m_pTexture, 0);
    }

    pBuffer->Unlock();
    pBuffer->Release();
    pSample->Release();

    return true;
}

void MediaPlayer::CopyRGB32(BYTE* pData, const D3D11_MAPPED_SUBRESOURCE& mapped) {
    UINT rowBytes = m_videoWidth * 4;
    for (UINT y = 0; y < m_videoHeight; ++y) {
        BYTE* dst = (BYTE*)mapped.pData + y * mapped.RowPitch;
        UINT srcRow = m_isBottomUp ? (m_videoHeight - 1 - y) : y;
        BYTE* src = pData + srcRow * m_stride;
        memcpy(dst, src, rowBytes);
        for (UINT x = 0; x < m_videoWidth; ++x) {
            dst[x * 4 + 3] = 0xFF;
        }
    }
}

void MediaPlayer::CopyYUY2(BYTE* pData, const D3D11_MAPPED_SUBRESOURCE& mapped) {
    UINT srcStride = m_stride > 0 ? m_stride : m_videoWidth * 2;

    // ルックアップテーブル（初回のみ構築）
    static bool tableBuilt = false;
    static BYTE clampTable[1024];
    if (!tableBuilt) {
        for (int i = 0; i < 1024; ++i) {
            int v = i - 300;
            clampTable[i] = (BYTE)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
        tableBuilt = true;
    }

    for (UINT y = 0; y < m_videoHeight; ++y) {
        BYTE* dst = (BYTE*)mapped.pData + y * mapped.RowPitch;
        UINT srcRow = m_isBottomUp ? (m_videoHeight - 1 - y) : y;
        BYTE* src = pData + srcRow * srcStride;

        for (UINT x = 0; x < m_videoWidth; x += 2) {
            int Y0 = src[x * 2 + 0];
            int U = src[x * 2 + 1];
            int Y1 = src[x * 2 + 2];
            int V = src[x * 2 + 3];

            int D = U - 128;
            int E = V - 128;
            int bAdd = (516 * D + 128) >> 8;
            int gAdd = (-100 * D - 208 * E + 128) >> 8;
            int rAdd = (409 * E + 128) >> 8;

            int C0 = (298 * (Y0 - 16)) >> 8;
            int C1 = (298 * (Y1 - 16)) >> 8;

            auto c = [](int v) -> BYTE {
                return (BYTE)(v < 0 ? 0 : (v > 255 ? 255 : v));
                };

            BYTE* p0 = dst + x * 4;
            p0[0] = c(C0 + bAdd);
            p0[1] = c(C0 + gAdd);
            p0[2] = c(C0 + rAdd);
            p0[3] = 0xFF;

            if (x + 1 < m_videoWidth) {
                BYTE* p1 = p0 + 4;
                p1[0] = c(C1 + bAdd);
                p1[1] = c(C1 + gAdd);
                p1[2] = c(C1 + rAdd);
                p1[3] = 0xFF;
            }
        }
    }
}

void MediaPlayer::CopyNV12(BYTE* pData, const D3D11_MAPPED_SUBRESOURCE& mapped) {
    UINT srcStride = m_stride > 0 ? m_stride : m_videoWidth;
    BYTE* yPlane = pData;
    BYTE* uvPlane = pData + srcStride * m_videoHeight;

    for (UINT y = 0; y < m_videoHeight; y += 2) {
        BYTE* yRow0 = yPlane + y * srcStride;
        BYTE* yRow1 = yPlane + (y + 1) * srcStride;
        BYTE* uvRow = uvPlane + (y / 2) * srcStride;
        BYTE* dst0 = (BYTE*)mapped.pData + y * mapped.RowPitch;
        BYTE* dst1 = (BYTE*)mapped.pData + (y + 1) * mapped.RowPitch;

        for (UINT x = 0; x < m_videoWidth; x += 2) {
            int U = uvRow[x + 0] - 128;
            int V = uvRow[x + 1] - 128;

            int bAdd = (516 * U + 128) >> 8;
            int gAdd = (-100 * U - 208 * V + 128) >> 8;
            int rAdd = (409 * V + 128) >> 8;

            auto c = [](int v) -> BYTE {
                return (BYTE)(v < 0 ? 0 : (v > 255 ? 255 : v));
                };

            // 2x2ブロックを一括処理
            for (int dy = 0; dy < 2 && (y + dy) < m_videoHeight; ++dy) {
                BYTE* yRow = (dy == 0) ? yRow0 : yRow1;
                BYTE* dst = (dy == 0) ? dst0 : dst1;

                for (int dx = 0; dx < 2 && (x + dx) < m_videoWidth; ++dx) {
                    int C = (298 * (yRow[x + dx] - 16)) >> 8;
                    BYTE* p = dst + (x + dx) * 4;
                    p[0] = c(C + bAdd);
                    p[1] = c(C + gAdd);
                    p[2] = c(C + rAdd);
                    p[3] = 0xFF;
                }
            }
        }
    }
}
