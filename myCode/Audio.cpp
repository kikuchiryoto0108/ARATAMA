//==============================================================================
//
//  オーディオシステム [audio.cpp]
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/10
//------------------------------------------------------------------------------
//
//  【概要】
//  XAudio2とMedia Foundationを使用したオーディオシステム
//  - WAV/MP3ファイルの読み込みと再生
//  - BGMのループ再生、フェードイン/アウト、クロスフェード
//  - SEの単発再生、多重再生（同じ音を重ねて鳴らす）
//  - マスター/BGM/SE個別の音量制御
//
//==============================================================================

#include "audio.h"
#include "time_manager.h"
#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "winmm.lib")      // mmio API用
#pragma comment(lib, "mfplat.lib")     // Media Foundation基盤
#pragma comment(lib, "mfreadwrite.lib") // SourceReader用
#pragma comment(lib, "mfuuid.lib")     // Media Foundation GUID定義

//==============================================================================
// ファイルパス配列（X-Macroで自動生成）
//------------------------------------------------------------------------------
// audio_def.hのAUDIO_LISTから、パス部分だけを抽出して配列化
// 例: AUDIO(SE_JUMP, "sound/jump.mp3", false) → "sound/jump.mp3"
//==============================================================================
#define AUDIO(id, path, isBgm) path,
static const char* g_audioFiles[static_cast<int>(AudioID::MAX)] = {
    AUDIO_LIST
};
#undef AUDIO

//==============================================================================
// BGMフラグ配列（X-Macroで自動生成）
//------------------------------------------------------------------------------
// audio_def.hのAUDIO_LISTから、BGMフラグ部分だけを抽出して配列化
// 例: AUDIO(SE_JUMP, "sound/jump.mp3", false) → false
//==============================================================================
#define AUDIO(id, path, isBgm) isBgm,
static const bool g_isBGMFlags[static_cast<int>(AudioID::MAX)] = {
    AUDIO_LIST
};
#undef AUDIO

//==============================================================================
// 内部構造体：音声データ
//------------------------------------------------------------------------------
// 1つの音声ファイルに対応するデータをまとめた構造体
// - soundData: デコード済みPCMデータ（メモリ上に展開）
// - sourceVoice: XAudio2の再生ユニット
// - waveFormat: サンプルレート、チャンネル数などの情報
//==============================================================================
struct AudioData {
    IXAudio2SourceVoice* sourceVoice = nullptr;  // XAudio2再生ユニット
    BYTE* soundData = nullptr;                    // PCMデータ本体
    WAVEFORMATEX waveFormat = {};                 // 音声フォーマット情報
    int length = 0;                               // データサイズ（バイト）
    int playLength = 0;                           // 再生長（サンプル数）
    bool isBGM = false;                           // BGMフラグ（ループ再生用）
    bool isLoaded = false;                        // 読み込み完了フラグ
};

//==============================================================================
// フェード状態
//------------------------------------------------------------------------------
// BGMのフェード処理の種類を表す列挙型
//==============================================================================
enum class FadeState {
    NONE,        // フェードなし（通常再生）
    FADE_IN,     // フェードイン（0→最大音量）
    FADE_OUT,    // フェードアウト（最大音量→0）
    CROSS_FADE   // クロスフェード（旧BGM↓ + 新BGM↑ 同時進行）
};

//==============================================================================
// 静的変数（モジュール内グローバル）
//==============================================================================

// XAudio2コアオブジェクト
static IXAudio2* g_xAudio = nullptr;                    // XAudio2エンジン本体
static IXAudio2MasteringVoice* g_masteringVoice = nullptr; // マスター出力（スピーカーへの最終出力）

// 音声データ配列（全音声ファイル分）
static AudioData g_audioData[static_cast<int>(AudioID::MAX)];

// Media Foundation初期化フラグ
static bool g_mfInitialized = false;

// 音量設定（0.0〜1.0）
static float g_masterVolume = 1.0f;  // マスター音量（全体に影響）
static float g_bgmVolume = 1.0f;     // BGM音量
static float g_seVolume = 1.0f;      // SE音量

// 個別音声のボリューム（Audio_SetSoundVolume用）
static float g_audioVolume[static_cast<int>(AudioID::MAX)];

// BGM再生状態
static AudioID g_currentBGM = AudioID::MAX;  // 現在再生中のBGM（MAX=なし）
static AudioID g_nextBGM = AudioID::MAX;     // クロスフェード時の次のBGM
static float g_currentBGMVolume = 1.0f;      // 現在BGMの音量
static float g_nextBGMVolume = 1.0f;         // 次BGMの音量
static bool g_bgmPaused = false;             // 一時停止フラグ

// フェード処理用
static FadeState g_fadeState = FadeState::NONE;  // 現在のフェード状態
static float g_fadeTimer = 0.0f;                 // フェード残り時間
static float g_fadeDuration = 0.0f;              // フェード総時間（ratio計算用）

//==============================================================================
// SE多重再生用プール
//------------------------------------------------------------------------------
// 同じSEを短時間に複数回鳴らすための仕組み
// 
// 【問題】
// 通常、1つのSEに対して1つのSourceVoiceしかないため、
// 再生中に同じSEを鳴らすと前の音が途切れる
// 
// 【解決策】
// 1つのSEに対して複数のSourceVoice（プール）を用意し、
// ラウンドロビン方式で順番に使う
// 
// 【ラウンドロビン】
// Voice[0] → Voice[1] → Voice[2] → ... → Voice[7] → Voice[0] → ...
// 古い音は自然に終わるか、上書きされる
//==============================================================================
static constexpr int SE_VOICE_POOL_SIZE = 8;  // 同時再生可能数

struct SEVoicePool {
    IXAudio2SourceVoice* voices[SE_VOICE_POOL_SIZE] = {};  // Voiceの配列
    int nextIndex = 0;    // 次に使うVoiceのインデックス
    bool initialized = false;  // 初期化済みフラグ
};

// 全SEに対応するプール（Audio_PlaySEMulti使用時のみ初期化）
static SEVoicePool g_sePool[static_cast<int>(AudioID::MAX)];

//==============================================================================
// 内部関数：拡張子判定
//------------------------------------------------------------------------------
// ファイル名が指定した拡張子で終わるかチェック
// 例: HasExtension("bgm.mp3", ".mp3") → true
//==============================================================================
static bool HasExtension(const char* fileName, const char* ext) {
    std::string file(fileName);
    std::string extension(ext);
    if (file.length() < extension.length()) return false;
    return file.compare(file.length() - extension.length(), extension.length(), extension) == 0;
}

//==============================================================================
// 内部関数：WAVファイル読み込み
//------------------------------------------------------------------------------
// WAVファイルを読み込んでAudioDataに設定する
// 
// 【WAVファイルの構造】
// ┌─────────────────────────────────┐
// │ RIFFチャンク（ファイルヘッダー）    │
// │   ├─ fmtチャンク（フォーマット情報）│
// │   │    - サンプルレート (44100Hz等) │
// │   │    - チャンネル数 (1=モノラル, 2=ステレオ)
// │   │    - ビット深度 (16bit等)       │
// │   └─ dataチャンク（PCMデータ本体） │
// │        - 生の波形データ              │
// └─────────────────────────────────┘
// 
// WAVは非圧縮なので、dataチャンクをそのまま再生できる
//==============================================================================
static bool LoadWavFile(const char* fileName, AudioData* outData) {
    if (fileName == nullptr) return false;

    //--------------------------------------------------------------------------
    // ファイルを開く（mmio = Multimedia I/O）
    //--------------------------------------------------------------------------
    HMMIO hmmio = mmioOpenA(const_cast<char*>(fileName), nullptr, MMIO_READ);
    if (!hmmio) return false;

    //--------------------------------------------------------------------------
    // RIFFチャンクを探す
    // RIFF/WAVEはWAVファイルの識別子
    //--------------------------------------------------------------------------
    MMCKINFO riffChunk = {};
    riffChunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');  // "WAVE"という4文字コード
    if (mmioDescend(hmmio, &riffChunk, nullptr, MMIO_FINDRIFF) != MMSYSERR_NOERROR) {
        mmioClose(hmmio, 0);
        return false;
    }

    //--------------------------------------------------------------------------
    // fmtチャンクを探す（フォーマット情報）
    //--------------------------------------------------------------------------
    MMCKINFO fmtChunk = {};
    fmtChunk.ckid = mmioFOURCC('f', 'm', 't', ' ');  // "fmt "（スペース含む）
    if (mmioDescend(hmmio, &fmtChunk, &riffChunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR) {
        mmioClose(hmmio, 0);
        return false;
    }

    //--------------------------------------------------------------------------
    // フォーマット情報を読み込む
    // WAVEFORMATEXにはサンプルレート、チャンネル数、ビット深度などが入る
    //--------------------------------------------------------------------------
    WAVEFORMATEX wfx = {};
    if (fmtChunk.cksize >= sizeof(WAVEFORMATEX)) {
        // 拡張フォーマット（cbSizeフィールドあり）
        mmioRead(hmmio, reinterpret_cast<HPSTR>(&wfx), sizeof(wfx));
    } else {
        // 古いPCMフォーマット（cbSizeなし）
        PCMWAVEFORMAT pcmwf = {};
        mmioRead(hmmio, reinterpret_cast<HPSTR>(&pcmwf), sizeof(pcmwf));
        memcpy(&wfx, &pcmwf, sizeof(pcmwf));
        wfx.cbSize = 0;
    }
    mmioAscend(hmmio, &fmtChunk, 0);  // fmtチャンクから抜ける

    //--------------------------------------------------------------------------
    // dataチャンクを探す（実際の音声データ）
    //--------------------------------------------------------------------------
    MMCKINFO dataChunk = {};
    dataChunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
    if (mmioDescend(hmmio, &dataChunk, &riffChunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR) {
        mmioClose(hmmio, 0);
        return false;
    }

    //--------------------------------------------------------------------------
    // 音声データをメモリに読み込む
    //--------------------------------------------------------------------------
    outData->soundData = new BYTE[dataChunk.cksize];
    LONG readLen = mmioRead(hmmio, reinterpret_cast<HPSTR>(outData->soundData), dataChunk.cksize);

    outData->length = readLen;
    outData->playLength = readLen / wfx.nBlockAlign;  // サンプル数 = バイト数 / 1サンプルのバイト数
    outData->waveFormat = wfx;

    mmioClose(hmmio, 0);

    //--------------------------------------------------------------------------
    // XAudio2のSourceVoiceを作成
    // SourceVoice = 音声を再生するためのユニット
    //--------------------------------------------------------------------------
    HRESULT hr = g_xAudio->CreateSourceVoice(&outData->sourceVoice, &wfx);
    if (FAILED(hr)) {
        delete[] outData->soundData;
        outData->soundData = nullptr;
        return false;
    }

    outData->isLoaded = true;
    return true;
}

//==============================================================================
// 内部関数：MP3ファイル読み込み（Media Foundation）
//------------------------------------------------------------------------------
// MP3ファイルをPCMにデコードしてAudioDataに設定する
// 
// 【処理の流れ】
// MP3ファイル → Media Foundation (デコード) → PCMデータ → XAudio2
// 
// 【Media Foundationとは】
// Windows標準のメディア処理フレームワーク
// MP3, AAC, WMA等の圧縮音声をデコードできる
// 
// 【SourceReaderとは】
// ファイルを読み込んでデコードしてくれるオブジェクト
// 「MP3を読むけど、出力はPCMで」と指定すると自動変換してくれる
//==============================================================================
static bool LoadMP3File(const char* fileName, AudioData* outData) {
    if (fileName == nullptr || !g_mfInitialized) return false;

    //--------------------------------------------------------------------------
    // ファイルパスをワイド文字に変換
    // Media FoundationはUnicode（UTF-16）しか受け付けない
    //--------------------------------------------------------------------------
    int wlen = MultiByteToWideChar(CP_UTF8, 0, fileName, -1, nullptr, 0);
    wchar_t* wFileName = new wchar_t[wlen];
    MultiByteToWideChar(CP_UTF8, 0, fileName, -1, wFileName, wlen);

    //--------------------------------------------------------------------------
    // SourceReaderを作成
    // URLからファイルを開いてデコーダーを自動セットアップ
    //--------------------------------------------------------------------------
    IMFSourceReader* reader = nullptr;
    HRESULT hr = MFCreateSourceReaderFromURL(wFileName, nullptr, &reader);
    delete[] wFileName;

    if (FAILED(hr)) return false;

    //--------------------------------------------------------------------------
    // 出力形式をPCMに指定
    // MP3を読むが、出力はPCM（非圧縮）にしてもらう
    //--------------------------------------------------------------------------
    IMFMediaType* mediaType = nullptr;
    hr = MFCreateMediaType(&mediaType);
    if (FAILED(hr)) {
        reader->Release();
        return false;
    }

    mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);  // 音声データ
    mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);     // PCM形式で出力

    hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, mediaType);
    mediaType->Release();

    if (FAILED(hr)) {
        reader->Release();
        return false;
    }

    //--------------------------------------------------------------------------
    // 変換後のフォーマット情報を取得
    // デコード後のサンプルレート、チャンネル数などを取得
    //--------------------------------------------------------------------------
    IMFMediaType* outputType = nullptr;
    hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &outputType);
    if (FAILED(hr)) {
        reader->Release();
        return false;
    }

    WAVEFORMATEX* wfx = nullptr;
    UINT32 wfxSize = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(outputType, &wfx, &wfxSize);
    outputType->Release();

    if (FAILED(hr)) {
        reader->Release();
        return false;
    }

    //--------------------------------------------------------------------------
    // 全サンプルを読み込む
    // ReadSampleでデコード済みデータを少しずつ取得し、バッファに追加
    //--------------------------------------------------------------------------
    std::vector<BYTE> audioBuffer;
    while (true) {
        IMFSample* sample = nullptr;
        DWORD flags = 0;

        // 1フレーム分読み込み（デコード済み）
        hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);

        // ファイル終端 or エラーで終了
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (sample) sample->Release();
            break;
        }

        if (sample) {
            // サンプルから連続メモリバッファを取得
            IMFMediaBuffer* buffer = nullptr;
            hr = sample->ConvertToContiguousBuffer(&buffer);

            if (SUCCEEDED(hr)) {
                BYTE* data = nullptr;
                DWORD length = 0;
                hr = buffer->Lock(&data, nullptr, &length);  // バッファをロックしてポインタ取得

                if (SUCCEEDED(hr)) {
                    // 既存バッファの末尾に追加
                    size_t oldSize = audioBuffer.size();
                    audioBuffer.resize(oldSize + length);
                    memcpy(audioBuffer.data() + oldSize, data, length);
                    buffer->Unlock();
                }
                buffer->Release();
            }
            sample->Release();
        }
    }

    reader->Release();

    if (audioBuffer.empty()) {
        CoTaskMemFree(wfx);
        return false;
    }

    //--------------------------------------------------------------------------
    // AudioDataに設定
    //--------------------------------------------------------------------------
    outData->soundData = new BYTE[audioBuffer.size()];
    memcpy(outData->soundData, audioBuffer.data(), audioBuffer.size());
    outData->length = static_cast<int>(audioBuffer.size());
    outData->playLength = outData->length / wfx->nBlockAlign;
    memcpy(&outData->waveFormat, wfx, sizeof(WAVEFORMATEX));

    CoTaskMemFree(wfx);  // Media Foundationが確保したメモリを解放

    //--------------------------------------------------------------------------
    // XAudio2のSourceVoiceを作成
    //--------------------------------------------------------------------------
    hr = g_xAudio->CreateSourceVoice(&outData->sourceVoice, &outData->waveFormat);
    if (FAILED(hr)) {
        delete[] outData->soundData;
        outData->soundData = nullptr;
        return false;
    }

    outData->isLoaded = true;
    return true;
}

//==============================================================================
// 内部関数：ファイル読み込み（自動判定）
//------------------------------------------------------------------------------
// 拡張子を見てWAV/MP3を自動判定して読み込む
//==============================================================================
static bool LoadAudioFile(const char* fileName, AudioData* outData) {
    if (fileName == nullptr) return false;

    if (HasExtension(fileName, ".mp3") || HasExtension(fileName, ".MP3")) {
        return LoadMP3File(fileName, outData);
    } else {
        return LoadWavFile(fileName, outData);
    }
}

//==============================================================================
// 内部関数：SEプール初期化
//------------------------------------------------------------------------------
// 指定したSEの多重再生用Voiceプールを作成する
// Audio_PlaySEMultiで初めて使うときに遅延初期化される
//==============================================================================
static void InitSEPool(int idx) {
    if (g_sePool[idx].initialized) return;
    if (!g_audioData[idx].isLoaded) return;

    // 同じフォーマットのSourceVoiceを複数作成
    for (int i = 0; i < SE_VOICE_POOL_SIZE; ++i) {
        HRESULT hr = g_xAudio->CreateSourceVoice(
            &g_sePool[idx].voices[i],
            &g_audioData[idx].waveFormat
        );
        if (FAILED(hr)) {
            g_sePool[idx].voices[i] = nullptr;
        }
    }
    g_sePool[idx].nextIndex = 0;
    g_sePool[idx].initialized = true;
}

//==============================================================================
// 内部関数：音量クランプ
//------------------------------------------------------------------------------
// 音量を0.0〜1.0の範囲に制限する
//==============================================================================
static float ClampVolume(float volume) {
    if (volume < 0.0f) return 0.0f;
    if (volume > 1.0f) return 1.0f;
    return volume;
}

//==============================================================================
// 内部関数：再生状態取得
//------------------------------------------------------------------------------
// 指定した音声が再生中かどうかをチェック
// BuffersQueued > 0 なら再生中または再生待ち
//==============================================================================
static bool IsPlaying(AudioID id) {
    int idx = static_cast<int>(id);
    if (!g_audioData[idx].isLoaded || g_audioData[idx].sourceVoice == nullptr) return false;

    XAUDIO2_VOICE_STATE state;
    g_audioData[idx].sourceVoice->GetState(&state);
    return state.BuffersQueued > 0;
}

//==============================================================================
// 内部関数：BGM音量適用
//------------------------------------------------------------------------------
// BGMの実際の音量を計算して設定
// 最終音量 = マスター × BGM全体 × 個別 × フェード係数
//==============================================================================
static void ApplyBGMVolume(AudioID id, float individualVolume, float fadeMultiplier = 1.0f) {
    int idx = static_cast<int>(id);
    if (!g_audioData[idx].isLoaded || g_audioData[idx].sourceVoice == nullptr) return;

    float volume = g_masterVolume * g_bgmVolume * individualVolume * fadeMultiplier;
    g_audioData[idx].sourceVoice->SetVolume(ClampVolume(volume));
}

//==============================================================================
// 内部関数：BGM再生（内部用）
//------------------------------------------------------------------------------
// BGMをループ再生する内部関数
// 
// 【XAudio2の再生の流れ】
// 1. Stop() - 現在の再生を停止
// 2. FlushSourceBuffers() - キューに溜まったバッファをクリア
// 3. SubmitSourceBuffer() - 新しいバッファをキューに追加
// 4. SetVolume() - 音量設定
// 5. Start() - 再生開始
//==============================================================================
static void PlayBGMInternal(AudioID id, float volume, float fadeMultiplier = 1.0f) {
    int idx = static_cast<int>(id);
    if (!g_audioData[idx].isLoaded) return;

    // 前の再生を停止してバッファクリア
    g_audioData[idx].sourceVoice->Stop();
    g_audioData[idx].sourceVoice->FlushSourceBuffers();

    // バッファ設定（ループ再生）
    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = g_audioData[idx].length;       // データサイズ
    buf.pAudioData = g_audioData[idx].soundData;    // データポインタ
    buf.PlayBegin = 0;                              // 再生開始位置
    buf.PlayLength = g_audioData[idx].playLength;   // 再生長
    buf.LoopBegin = 0;                              // ループ開始位置
    buf.LoopLength = g_audioData[idx].playLength;   // ループ長
    buf.LoopCount = XAUDIO2_LOOP_INFINITE;          // 無限ループ

    g_audioData[idx].sourceVoice->SubmitSourceBuffer(&buf, nullptr);
    ApplyBGMVolume(id, volume, fadeMultiplier);
    g_audioData[idx].sourceVoice->Start();
}

//==============================================================================
// 公開関数：初期化
//------------------------------------------------------------------------------
// オーディオシステム全体を初期化
// ゲーム起動時に1回だけ呼ぶ
//==============================================================================
void Audio_Initialize() {
    //--------------------------------------------------------------------------
    // Media Foundation初期化（MP3デコード用）
    //--------------------------------------------------------------------------
    HRESULT hr = MFStartup(MF_VERSION);
    g_mfInitialized = SUCCEEDED(hr);

    //--------------------------------------------------------------------------
    // XAudio2初期化
    //--------------------------------------------------------------------------
    hr = XAudio2Create(&g_xAudio, 0);
    assert(SUCCEEDED(hr));

    // MasteringVoice = 全音声の最終出力先（スピーカー）
    hr = g_xAudio->CreateMasteringVoice(&g_masteringVoice);
    assert(SUCCEEDED(hr));

    //--------------------------------------------------------------------------
    // 全音声ファイル読み込み
    // audio_def.hで定義された全ファイルをメモリに展開
    //--------------------------------------------------------------------------
    for (int i = 0; i < static_cast<int>(AudioID::MAX); i++) {
        if (g_audioFiles[i] != nullptr) {
            if (LoadAudioFile(g_audioFiles[i], &g_audioData[i])) {
                g_audioData[i].isBGM = g_isBGMFlags[i];
            }
        }
    }

    //--------------------------------------------------------------------------
    // 状態初期化
    //--------------------------------------------------------------------------
    g_currentBGM = AudioID::MAX;
    g_nextBGM = AudioID::MAX;
    g_currentBGMVolume = 1.0f;
    g_nextBGMVolume = 1.0f;
    g_bgmPaused = false;
    g_fadeState = FadeState::NONE;
    g_masterVolume = 1.0f;
    g_bgmVolume = 1.0f;
    g_seVolume = 1.0f;
}

//==============================================================================
// 公開関数：終了処理
//------------------------------------------------------------------------------
// オーディオシステムの全リソースを解放
// ゲーム終了時に1回だけ呼ぶ
// 
// 【解放順序】
// 1. SEプール（多重再生用Voice）
// 2. 通常のSourceVoice + PCMデータ
// 3. MasteringVoice
// 4. XAudio2エンジン
// 5. Media Foundation
//==============================================================================
void Audio_Finalize() {
    //--------------------------------------------------------------------------
    // SEプール解放
    //--------------------------------------------------------------------------
    for (int i = 0; i < static_cast<int>(AudioID::MAX); i++) {
        if (g_sePool[i].initialized) {
            for (int j = 0; j < SE_VOICE_POOL_SIZE; ++j) {
                if (g_sePool[i].voices[j]) {
                    g_sePool[i].voices[j]->Stop();
                    g_sePool[i].voices[j]->DestroyVoice();
                    g_sePool[i].voices[j] = nullptr;
                }
            }
            g_sePool[i].initialized = false;
        }
    }

    //--------------------------------------------------------------------------
    // 通常のSourceVoice + PCMデータ解放
    //--------------------------------------------------------------------------
    for (int i = 0; i < static_cast<int>(AudioID::MAX); i++) {
        if (g_audioData[i].sourceVoice) {
            g_audioData[i].sourceVoice->Stop();
            g_audioData[i].sourceVoice->DestroyVoice();
            g_audioData[i].sourceVoice = nullptr;
        }
        if (g_audioData[i].soundData) {
            delete[] g_audioData[i].soundData;
            g_audioData[i].soundData = nullptr;
        }
        g_audioData[i].isLoaded = false;
    }

    //--------------------------------------------------------------------------
    // XAudio2解放
    //--------------------------------------------------------------------------
    if (g_masteringVoice) {
        g_masteringVoice->DestroyVoice();
        g_masteringVoice = nullptr;
    }

    if (g_xAudio) {
        g_xAudio->Release();
        g_xAudio = nullptr;
    }

    //--------------------------------------------------------------------------
    // Media Foundation終了
    //--------------------------------------------------------------------------
    if (g_mfInitialized) {
        MFShutdown();
        g_mfInitialized = false;
    }
}

//==============================================================================
// 公開関数：更新（毎フレーム呼ぶ）
//------------------------------------------------------------------------------
// フェード処理を進める
// フェード中でなければ何もしない
// 
// 【フェードの仕組み】
// ratio = 残り時間 / 総時間 （1.0→0.0に減少）
// FADE_IN:  音量 = 1.0 - ratio （0.0→1.0に増加）
// FADE_OUT: 音量 = ratio      （1.0→0.0に減少）
//==============================================================================
void Audio_Update() {
    if (g_fadeState == FadeState::NONE) return;

    float dt = static_cast<float>(TimeManager::GetInstance().GetElapsedTime());
    g_fadeTimer -= dt;

    //--------------------------------------------------------------------------
    // フェード完了時の処理
    //--------------------------------------------------------------------------
    if (g_fadeTimer <= 0.0f) {
        g_fadeTimer = 0.0f;

        switch (g_fadeState) {
        case FadeState::FADE_IN:
            // フェードイン完了：最大音量に設定
            if (g_currentBGM != AudioID::MAX) {
                ApplyBGMVolume(g_currentBGM, g_currentBGMVolume, 1.0f);
            }
            break;

        case FadeState::FADE_OUT:
            // フェードアウト完了：BGM停止
            if (g_currentBGM != AudioID::MAX) {
                int idx = static_cast<int>(g_currentBGM);
                g_audioData[idx].sourceVoice->Stop();
                g_audioData[idx].sourceVoice->FlushSourceBuffers();
                g_currentBGM = AudioID::MAX;
            }
            break;

        case FadeState::CROSS_FADE:
            // クロスフェード完了：旧BGM停止、新BGMを現在BGMに設定
            if (g_currentBGM != AudioID::MAX) {
                int idx = static_cast<int>(g_currentBGM);
                g_audioData[idx].sourceVoice->Stop();
                g_audioData[idx].sourceVoice->FlushSourceBuffers();
            }
            g_currentBGM = g_nextBGM;
            g_currentBGMVolume = g_nextBGMVolume;
            g_nextBGM = AudioID::MAX;
            if (g_currentBGM != AudioID::MAX) {
                ApplyBGMVolume(g_currentBGM, g_currentBGMVolume, 1.0f);
            }
            break;

        default:
            break;
        }

        g_fadeState = FadeState::NONE;
    }
    //--------------------------------------------------------------------------
    // フェード中の処理
    //--------------------------------------------------------------------------
    else {
        float ratio = g_fadeTimer / g_fadeDuration;  // 1.0→0.0に減少

        switch (g_fadeState) {
        case FadeState::FADE_IN:
            // 音量: 0.0→1.0に増加
            if (g_currentBGM != AudioID::MAX) {
                ApplyBGMVolume(g_currentBGM, g_currentBGMVolume, 1.0f - ratio);
            }
            break;

        case FadeState::FADE_OUT:
            // 音量: 1.0→0.0に減少
            if (g_currentBGM != AudioID::MAX) {
                ApplyBGMVolume(g_currentBGM, g_currentBGMVolume, ratio);
            }
            break;

        case FadeState::CROSS_FADE:
            // 旧BGM: 1.0→0.0に減少
            // 新BGM: 0.0→1.0に増加
            if (g_currentBGM != AudioID::MAX) {
                ApplyBGMVolume(g_currentBGM, g_currentBGMVolume, ratio);
            }
            if (g_nextBGM != AudioID::MAX) {
                ApplyBGMVolume(g_nextBGM, g_nextBGMVolume, 1.0f - ratio);
            }
            break;

        default:
            break;
        }
    }
}

//==============================================================================
// 公開関数：BGM再生
//------------------------------------------------------------------------------
// 指定したBGMを即座にループ再生開始
// 既存のBGMは停止される
//==============================================================================
void Audio_PlayBGM(AudioID id, float volume) {
    Audio_StopBGM();

    int idx = static_cast<int>(id);
    if (!g_audioData[idx].isLoaded) return;

    PlayBGMInternal(id, volume);
    g_currentBGM = id;
    g_currentBGMVolume = volume;
    g_bgmPaused = false;
    g_fadeState = FadeState::NONE;
}

//==============================================================================
// 公開関数：BGM停止
//------------------------------------------------------------------------------
// 現在再生中のBGMを即座に停止
//==============================================================================
void Audio_StopBGM() {
    if (g_currentBGM == AudioID::MAX) return;

    int idx = static_cast<int>(g_currentBGM);
    if (g_audioData[idx].sourceVoice) {
        g_audioData[idx].sourceVoice->Stop();
        g_audioData[idx].sourceVoice->FlushSourceBuffers();
    }

    g_currentBGM = AudioID::MAX;
    g_bgmPaused = false;
    g_fadeState = FadeState::NONE;
}

//==============================================================================
// 公開関数：BGM一時停止
//------------------------------------------------------------------------------
// 現在の再生位置を保持したまま停止
// Audio_ResumeBGM()で再開可能
//==============================================================================
void Audio_PauseBGM() {
    if (g_currentBGM == AudioID::MAX || g_bgmPaused) return;

    int idx = static_cast<int>(g_currentBGM);
    if (g_audioData[idx].sourceVoice) {
        g_audioData[idx].sourceVoice->Stop();
    }
    g_bgmPaused = true;
}

//==============================================================================
// 公開関数：BGM再開
//------------------------------------------------------------------------------
// 一時停止したBGMを再開
//==============================================================================
void Audio_ResumeBGM() {
    if (g_currentBGM == AudioID::MAX || !g_bgmPaused) return;

    int idx = static_cast<int>(g_currentBGM);
    if (g_audioData[idx].sourceVoice) {
        g_audioData[idx].sourceVoice->Start();
    }
    g_bgmPaused = false;
}

//==============================================================================
// 公開関数：BGMフェードイン再生
//------------------------------------------------------------------------------
// 音量0から徐々に上げながらBGMを再生開始
// duration: フェード時間（秒）
//==============================================================================
void Audio_FadeInBGM(AudioID id, float duration) {
    Audio_StopBGM();

    int idx = static_cast<int>(id);
    if (!g_audioData[idx].isLoaded) return;

    // 音量0で再生開始
    PlayBGMInternal(id, 1.0f, 0.0f);
    g_currentBGM = id;
    g_currentBGMVolume = 1.0f;
    g_bgmPaused = false;

    // フェード設定
    g_fadeState = FadeState::FADE_IN;
    g_fadeTimer = duration;
    g_fadeDuration = duration;
}

//==============================================================================
// 公開関数：BGMフェードアウト停止
//------------------------------------------------------------------------------
// 音量を徐々に下げてからBGMを停止
// duration: フェード時間（秒）
//==============================================================================
void Audio_FadeOutBGM(float duration) {
    if (g_currentBGM == AudioID::MAX) return;

    g_fadeState = FadeState::FADE_OUT;
    g_fadeTimer = duration;
    g_fadeDuration = duration;
}

//==============================================================================
// 公開関数：BGMクロスフェード
//------------------------------------------------------------------------------
// 旧BGMをフェードアウトしながら新BGMをフェードインする
// 音が途切れずスムーズにBGMを切り替えられる
// duration: フェード時間（秒）
//==============================================================================
void Audio_CrossFadeBGM(AudioID id, float duration) {
    int idx = static_cast<int>(id);
    if (!g_audioData[idx].isLoaded) return;

    // 新BGMを音量0で再生開始
    PlayBGMInternal(id, 1.0f, 0.0f);
    g_nextBGM = id;
    g_nextBGMVolume = 1.0f;

    // フェード設定
    g_fadeState = FadeState::CROSS_FADE;
    g_fadeTimer = duration;
    g_fadeDuration = duration;
}

//==============================================================================
// 公開関数：SE再生
//------------------------------------------------------------------------------
// SEを1回再生する
// 同じSEを再生中に呼ぶと、前の再生が停止して最初から再生
//==============================================================================
void Audio_PlaySE(AudioID id, float volume) {
    int idx = static_cast<int>(id);
    if (!g_audioData[idx].isLoaded) return;

    // 前の再生を停止してバッファクリア
    g_audioData[idx].sourceVoice->Stop();
    g_audioData[idx].sourceVoice->FlushSourceBuffers();

    // バッファ設定（ループなし）
    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = g_audioData[idx].length;
    buf.pAudioData = g_audioData[idx].soundData;
    buf.PlayBegin = 0;
    buf.PlayLength = g_audioData[idx].playLength;
    // LoopCount = 0（デフォルト）= ループしない

    g_audioData[idx].sourceVoice->SubmitSourceBuffer(&buf, nullptr);

    // 音量設定
    float finalVolume = g_masterVolume * g_seVolume * volume;
    g_audioData[idx].sourceVoice->SetVolume(ClampVolume(finalVolume));

    g_audioData[idx].sourceVoice->Start();
}

//==============================================================================
// 公開関数：SE停止
//------------------------------------------------------------------------------
// 指定したSEの再生を停止
//==============================================================================
void Audio_StopSE(AudioID id) {
    int idx = static_cast<int>(id);
    if (!g_audioData[idx].isLoaded) return;

    if (g_audioData[idx].sourceVoice) {
        g_audioData[idx].sourceVoice->Stop();
        g_audioData[idx].sourceVoice->FlushSourceBuffers();
    }
}

//==============================================================================
// 公開関数：全SE停止
//------------------------------------------------------------------------------
// 全てのSEを停止（BGMは停止しない）
//==============================================================================
void Audio_StopAllSE() {
    for (int i = 0; i < static_cast<int>(AudioID::MAX); i++) {
        if (!g_audioData[i].isBGM && g_audioData[i].isLoaded) {
            if (g_audioData[i].sourceVoice) {
                g_audioData[i].sourceVoice->Stop();
                g_audioData[i].sourceVoice->FlushSourceBuffers();
            }
        }
    }
}

//==============================================================================
// 公開関数：SE多重再生
//------------------------------------------------------------------------------
// 同じSEを重ねて再生できる
// 連続ヒット音、足音など短時間に複数回鳴らしたい場合に使う
// 
// 【通常のAudio_PlaySEとの違い】
// Audio_PlaySE:     同時に1音のみ。再生中に呼ぶと最初から再生し直し
// Audio_PlaySEMulti: 同時に最大8音まで重ねて再生可能
// 
// 【仕組み（ラウンドロビン）】
// 8個のSourceVoiceを順番に使う
// Voice[0] → Voice[1] → ... → Voice[7] → Voice[0] → ...
//==============================================================================
void Audio_PlaySEMulti(AudioID id, float volume) {
    int idx = static_cast<int>(id);
    if (!g_audioData[idx].isLoaded) return;

    // プール未初期化なら初期化（遅延初期化）
    if (!g_sePool[idx].initialized) {
        InitSEPool(idx);
    }

    // プールが使えない場合は通常再生にフォールバック
    if (!g_sePool[idx].initialized) {
        Audio_PlaySE(id, volume);
        return;
    }

    // プールから次のVoiceを取得（ラウンドロビン）
    IXAudio2SourceVoice* voice = g_sePool[idx].voices[g_sePool[idx].nextIndex];
    if (!voice) {
        Audio_PlaySE(id, volume);
        return;
    }

    // 前の再生を停止してバッファクリア
    voice->Stop();
    voice->FlushSourceBuffers();

    // バッファ設定
    XAUDIO2_BUFFER buf = {};
    buf.AudioBytes = g_audioData[idx].length;
    buf.pAudioData = g_audioData[idx].soundData;
    buf.PlayBegin = 0;
    buf.PlayLength = g_audioData[idx].playLength;

    voice->SubmitSourceBuffer(&buf, nullptr);

    // 音量設定
    float finalVolume = g_masterVolume * g_seVolume * volume;
    voice->SetVolume(ClampVolume(finalVolume));

    voice->Start();

    // 次のインデックスへ（ラウンドロビン）
    g_sePool[idx].nextIndex = (g_sePool[idx].nextIndex + 1) % SE_VOICE_POOL_SIZE;
}

//==============================================================================
// 公開関数：個別音量設定
//------------------------------------------------------------------------------
// 特定の音声の音量を個別に設定
// BGMの場合は再生中のものにも即時反映
//==============================================================================
void Audio_SetSoundVolume(AudioID id, float volume) {
    int idx = static_cast<int>(id);
    float v = ClampVolume(volume);
    g_audioVolume[idx] = v;

    if (!g_audioData[idx].isLoaded) return;

    if (g_audioData[idx].isBGM) {
        // BGMの場合：再生中なら音量を即時更新
        if (g_currentBGM == id) {
            g_currentBGMVolume = v;
            if (g_fadeState == FadeState::NONE) {
                ApplyBGMVolume(id, g_currentBGMVolume);
            } else {
                ApplyBGMVolume(id, g_currentBGMVolume);
            }
        }
        if (g_nextBGM == id) {
            g_nextBGMVolume = v;
        }
    } else {
        // SEの場合：SourceVoiceに直接設定
        if (g_audioData[idx].sourceVoice) {
            float finalVolume = g_masterVolume * g_seVolume * v;
            g_audioData[idx].sourceVoice->SetVolume(ClampVolume(finalVolume));
        }
    }
}

//==============================================================================
// 公開関数：個別音量取得
//==============================================================================
float Audio_GetSoundVolume(AudioID id) {
    return g_audioVolume[static_cast<int>(id)];
}

//==============================================================================
// 公開関数：マスター音量設定
//------------------------------------------------------------------------------
// 全ての音に影響する音量を設定
// 最終音量 = マスター × (BGM or SE) × 個別
//==============================================================================
void Audio_SetMasterVolume(float volume) {
    g_masterVolume = ClampVolume(volume);

    // 再生中の音に即時反映
    if (g_currentBGM != AudioID::MAX && g_fadeState == FadeState::NONE) {
        ApplyBGMVolume(g_currentBGM, g_currentBGMVolume);

        for (int i = 0; i < static_cast<int>(AudioID::MAX); i++) {
            if (!g_audioData[i].isBGM && g_audioData[i].isLoaded && g_audioData[i].sourceVoice) {
                float finalVolume = g_masterVolume * g_seVolume * g_audioVolume[i];
                g_audioData[i].sourceVoice->SetVolume(ClampVolume(finalVolume));
            }
        }
    }
}

//==============================================================================
// 公開関数：BGM音量設定
//------------------------------------------------------------------------------
// BGM全体の音量を設定
//==============================================================================
void Audio_SetBGMVolume(float volume) {
    g_bgmVolume = ClampVolume(volume);

    if (g_currentBGM != AudioID::MAX && g_fadeState == FadeState::NONE) {
        ApplyBGMVolume(g_currentBGM, g_currentBGMVolume);
    }
}

//==============================================================================
// 公開関数：SE音量設定
//------------------------------------------------------------------------------
// SE全体の音量を設定
// 再生中のSEにも即時反映
//==============================================================================
void Audio_SetSEVolume(float volume) {
    g_seVolume = ClampVolume(volume);

    for (int i = 0; i < static_cast<int>(AudioID::MAX); i++) {
        if (!g_audioData[i].isBGM && g_audioData[i].isLoaded && g_audioData[i].sourceVoice) {
            float finalVolume = g_masterVolume * g_seVolume * g_audioVolume[i];
            g_audioData[i].sourceVoice->SetVolume(ClampVolume(finalVolume));
        }
    }
}

//==============================================================================
// 公開関数：音量取得
//==============================================================================
float Audio_GetMasterVolume() {
    return g_masterVolume;
}

float Audio_GetBGMVolume() {
    return g_bgmVolume;
}

float Audio_GetSEVolume() {
    return g_seVolume;
}

//==============================================================================
// 公開関数：BGM再生状態取得
//==============================================================================
bool Audio_IsPlayingBGM() {
    if (g_currentBGM == AudioID::MAX) return false;
    return IsPlaying(g_currentBGM) && !g_bgmPaused;
}

//==============================================================================
// 公開関数：SE再生状態取得
//==============================================================================
bool Audio_IsPlayingSE(AudioID id) {
    return IsPlaying(id);
}
