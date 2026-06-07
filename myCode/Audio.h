//==============================================================================
//
//  オーディオシステム [Audio.h]
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/10
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once
#include "audio_def.h"

//==============================================================================
// AudioID（自動生成）
//==============================================================================
#define AUDIO(id, path, isBgm) id,
enum class AudioID {
    AUDIO_LIST
    MAX
};
#undef AUDIO

//==============================================================================
// 初期化/終了
//==============================================================================

void Audio_Initialize();
void Audio_Finalize();

// 更新（毎フレーム呼ぶ、フェード処理用）
void Audio_Update();

//==============================================================================
// BGM操作
//==============================================================================

// BGM再生（ループ）、volume: 0.0~1.0（省略時1.0）
void Audio_PlayBGM(AudioID id, float volume = 1.0f);
void Audio_StopBGM();
void Audio_PauseBGM();

// BGM再開
void Audio_ResumeBGM();

// BGMフェードイン再生、duration: 秒
void Audio_FadeInBGM(AudioID id, float duration);

// BGMフェードアウト停止、duration: 秒
void Audio_FadeOutBGM(float duration);

// BGMクロスフェード、duration: 秒
void Audio_CrossFadeBGM(AudioID id, float duration);

//==============================================================================
// SE操作
//==============================================================================

// SE再生（1回）、volume: 0.0~1.0（省略時1.0）
void Audio_PlaySE(AudioID id, float volume = 1.0f);
void Audio_StopSE(AudioID id);
void Audio_StopAllSE();

// SE再生（複数同時再生可能）、volume: 0.0~1.0（省略時1.0）
void Audio_PlaySEMulti(AudioID id, float volume = 1.0f);

//==============================================================================
// 音量設定（0.0~1.0）
//==============================================================================

// マスター音量設定（BGM・SE両方に影響）
void Audio_SetMasterVolume(float volume);

// BGM音量設定
void Audio_SetBGMVolume(float volume);

// SE音量設定
void Audio_SetSEVolume(float volume);

// 個別音声の音量設定
void Audio_SetSoundVolume(AudioID id, float volume);

// 個別音声の音量取得
float Audio_GetSoundVolume(AudioID id);

// マスター音量取得
float Audio_GetMasterVolume();

// BGM音量取得
float Audio_GetBGMVolume();

// SE音量取得
float Audio_GetSEVolume();

//==============================================================================
// 状態取得
//==============================================================================

// BGM再生中か
bool Audio_IsPlayingBGM();

// SE再生中か
bool Audio_IsPlayingSE(AudioID id);
