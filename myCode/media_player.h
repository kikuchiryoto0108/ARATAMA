/*********************************************************************
 * \file   media_player.h
 * \brief  ìÆâÊçƒê∂ÉNÉâÉX
 * 
 * \author Ryoto Kikuchi
 * \date   2026/3/7
 *********************************************************************/
#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <DirectXMath.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

class MediaPlayer {
public:
    enum class OutputFormat {
        UNKNOWN,
        RGB32,
        YUY2,
        NV12,
    };

private:
    IMFSourceReader* m_pReader{ nullptr };
    ID3D11Texture2D* m_pTexture{ nullptr };
    ID3D11ShaderResourceView* m_pSRV{ nullptr };

    UINT m_videoWidth{ 0 };
    UINT m_videoHeight{ 0 };
    bool m_isPlaying{ false };
    bool m_isFinished{ false };

    double m_frameDuration{ 0.0 };
    double m_elapsed{ 0.0 };

    bool m_mfStarted{ false };
    bool m_isBottomUp{ true };
    UINT m_stride{ 0 };
    OutputFormat m_outputFormat{ OutputFormat::UNKNOWN };

public:
    MediaPlayer() = default;
    ~MediaPlayer();

    bool Initialize(const wchar_t* filePath, ID3D11Device* device);
    void Finalize();

    bool Update(double deltaTime);

    ID3D11ShaderResourceView* GetSRV() const { return m_pSRV; }

    void Play() { m_isPlaying = true; }
    void Stop() { m_isPlaying = false; }
    bool IsPlaying() const { return m_isPlaying; }
    bool IsFinished() const { return m_isFinished; }

    UINT GetWidth() const { return m_videoWidth; }
    UINT GetHeight() const { return m_videoHeight; }

private:
    bool DecodeNextFrame(ID3D11Device* device, ID3D11DeviceContext* context);
    void CopyRGB32(BYTE* pData, const D3D11_MAPPED_SUBRESOURCE& mapped);
    void CopyYUY2(BYTE* pData, const D3D11_MAPPED_SUBRESOURCE& mapped);
    void CopyNV12(BYTE* pData, const D3D11_MAPPED_SUBRESOURCE& mapped);
};

#endif
