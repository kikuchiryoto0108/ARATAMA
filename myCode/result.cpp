//==============================================================================
//
//  リザルト画面 [result.cpp]
//  - 自分のクリアタイムを表示
//  - Googleスプレッドシートからランキングを読み込み
//  - トップ5を上からスライドインで表示
//  - 自分がランクインしていたら点滅＋拡大演出
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/2/23
//------------------------------------------------------------------------------
//
//==============================================================================
#include "result.h"
#include "input_manager.h"
#include "fade.h"
#include "scene.h"
#include "direct3d.h"
#include "time_manager.h"
#include "texture.h"
#include "sprite.h"
#include "result_data.h"
#include "ranking_loader.h"
#include "game.h"
#include "Audio.h"
#include <math.h>
#include <cstdio>
#include <thread>
#include "game_manager.h"

//======================================================================
// ステート定義
//======================================================================
enum ResultState {
    RESULT_STATE_FADE_IN,               // フェードイン中
    RESULT_STATE_SHOW_MY_TIME,          // 自分のタイム表示（待ち）
    RESULT_STATE_KEYINPUT_WAIT,         // Enter 入力待ち
    RESULT_STATE_MY_TIME_SLIDE_DOWN,    // 自分のタイムが下にスライド
    RESULT_STATE_LOADING_RANKING,       // ランキング読み込み中
    RESULT_STATE_RANKING_SLIDE_IN,      // ランキングが上からスライドイン
    RESULT_STATE_RANKING_SHOW,          // ランキング表示中（Enter 待ち）
    RESULT_STATE_RANKING_FADE_OUT,      // ランキング→クレジットへのフェードアウト
    RESULT_STATE_CREDIT_FADE_IN,        // クレジットへフェードイン
    RESULT_STATE_CREDIT_SHOW,           // クレジット表示中
    RESULT_STATE_FADE_OUT,              // フェードアウト中
    RESULT_STATE_MAX,
};

//======================================================================
// 静的変数
//======================================================================

// 共通
static ResultState g_State = RESULT_STATE_FADE_IN;
static double g_AccumulatedTime = 0.0;
static double g_KeyInputTime = 0.0;

//--- 数字スプライトシート ---
static int g_NumberTexId = -1;

//--- 巻物 ---
static int g_MakimonoTexId = -1;
static const float MAKIMONO_W = 660.0f;
static const float MAKIMONO_H = 150.0f;

static const float SHEET_WIDTH = 974.0f;    // テクスチャ全体の横幅
static const float SHEET_HEIGHT = 75.0f;     // テクスチャ全体の縦幅
static const int   CHAR_COUNT = 13;          // 0-9 + コロン
static const float CHAR_UV_WIDTH = SHEET_WIDTH / static_cast<float>(CHAR_COUNT);
static const float CHAR_UV_HEIGHT = SHEET_HEIGHT;

static const float DRAW_CHAR_W = 64.0f;       // 画面上の1文字の横幅
static const float DRAW_CHAR_H = 72.0f;       // 画面上の1文字の縦幅
static const float CHAR_SPACING = 4.0f;         // 文字間の余白
static const int   COLON_INDEX = 10;           // コロンのインデックス
static const int   DOT_INDEX = 11;           // 中黒のインデックス
static const int   COMMA_INDEX = 12;           // コンマ "."（小数点用）

//--- 自分のタイム アニメーション ---
static float g_MyTimeY = 0.0f;
static float g_MyTimeTargetY = 0.0f;
static const float MY_TIME_CENTER_Y = 500.0f;  // 中央表示時の Y
static const float MY_TIME_BOTTOM_Y = 850.0f;  // 下に移動後の Y

//--- ランキング ---
static float g_RankingY = -300.0f;              // スライドイン用 Y（画面外から開始）
static float g_RankingTargetY = 120.0f;         // スライド完了後の Y
static RankingEntry g_TopEntries[5] = {};       // トップ3データ
static int g_TopCount = 0;                      // 取得件数
static bool g_RankingLoaded = false;            // ランキング読み込み完了フラグ
static int g_MyRankIndex = -1;                  // 自分のランク（-1 = ランク外）

//--- ランキング読み込みスレッド ---
static std::thread* g_LoadThread = nullptr;
static bool g_LoadFinished = false;

//--- 背景 ---
static int g_BgTexId = -1;

//--- ボスクリアタイムテキスト画像 ---
static int g_ClearTimeTitleTexId = -1;
static const float CLEAR_TIME_TITLE_W = 2309.0f;
static const float CLEAR_TIME_TITLE_H = 295.0f;

//--- タイムアウト ---
static const float LOADING_TIMEOUT = 15.0f;     // 15秒でタイムアウト
static float g_LoadingTimer = 0.0f;              // ローディング経過時間
static bool g_IsTimeout = false;                 // タイムアウトしたか

//--- クレジット ---
static int g_CreditTexId = -1;
static const float CREDIT_W = 1920.0f;
static const float CREDIT_H = 1080.0f;
static bool g_ShowCredit = false;  // クレジット表示フラグ

//======================================================================
// 描画ユーティリティ
//======================================================================

// 数字1文字を描画
static void DrawNumberChar(int charIndex, float x, float y, float w, float h,
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f }) {
    if (g_NumberTexId < 0) return;
    if (charIndex < 0 || charIndex > COMMA_INDEX) return;

    float uvX = charIndex * CHAR_UV_WIDTH;
    float uvY = 0.0f;

    Sprite_EnableCameraZoom(false);
    Sprite_Draw(g_NumberTexId,
        x, y, w, h,
        uvX, uvY,
        CHAR_UV_WIDTH, CHAR_UV_HEIGHT,
        0.0f, color);
    Sprite_EnableCameraZoom(true);
}

// MM:SS:mm の横幅を計算
static float CalcTimeWidth(float charW, float spacing) {
    float colonW = charW * 0.4f;
    float commaW = charW * 0.3f;
    // 数字6文字 + コロン1つ + コンマ1つ
    return (charW + spacing) * 6.0f
        + (colonW + spacing * 2.0f)
        + (commaW + spacing);
}

// タイムを描画（左揃え）
static void DrawTime(double clearTime, float startX, float startY,
    float scale = 1.0f,
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f }) {
    int totalSeconds = static_cast<int>(clearTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    int centiseconds = static_cast<int>((clearTime - totalSeconds) * 100);

    // MM:SS.mm → 8要素（秒とコンマ秒の間はコンマ）
    int chars[8];
    chars[0] = minutes / 10;
    chars[1] = minutes % 10;
    chars[2] = COLON_INDEX;
    chars[3] = seconds / 10;
    chars[4] = seconds % 10;
    chars[5] = COMMA_INDEX;
    chars[6] = centiseconds / 10;
    chars[7] = centiseconds % 10;

    float cw = DRAW_CHAR_W * scale;
    float ch = DRAW_CHAR_H * scale;
    float sp = CHAR_SPACING * scale;
    float colonW = cw * 0.4f;
    float commaW = cw * 0.3f;

    float x = startX;

    for (int i = 0; i < 8; ++i) {
        if (chars[i] == COLON_INDEX) {
            float colonOffset = (cw - colonW) * 0.5f;
            DrawNumberChar(COLON_INDEX, x + colonOffset, startY, colonW, ch, color);
            x += colonW + sp * 2.0f;
        } else if (chars[i] == COMMA_INDEX) {
            float commaOffset = (cw - commaW) * 0.35f;
            DrawNumberChar(COMMA_INDEX, x + commaOffset, startY, commaW, ch, color);
            x += commaW + sp;
        } else {
            DrawNumberChar(chars[i], x, startY, cw, ch, color);
            x += cw + sp;
        }
    }
}

// タイムを中央揃えで描画
static void DrawTimeCentered(double clearTime, float centerX, float y,
    float scale = 1.0f,
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f }) {
    float w = CalcTimeWidth(DRAW_CHAR_W * scale, CHAR_SPACING * scale);
    float startX = centerX - w * 0.5f;
    DrawTime(clearTime, startX, y, scale, color);
}

// 線形補間
static float Lerp(float a, float b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return a + (b - a) * t;
}

//======================================================================
// 初期化
//======================================================================
void Result_Initialize() {
    g_AccumulatedTime = 0.0;
    g_KeyInputTime = 0.0;
    g_State = RESULT_STATE_FADE_IN;

    Fade_Start(1.0f, true);

    // BGM 再生（リザルト用）
    Audio_SetBGMVolume(0.4f);

    // 数字テクスチャ取得
    g_NumberTexId = TextureManager::Instance().Get(TexID::RESULT_NUMBER);

    // 背景テクスチャ取得
    g_BgTexId = TextureManager::Instance().Get(TexID::RESULT_BACKGROUND);

    // ボスクリアタイムテキスト画像取得
    g_ClearTimeTitleTexId = TextureManager::Instance().Get(TexID::RESULT_CLEAR_TIME);

	// 巻物テクスチャ取得
    g_MakimonoTexId = TextureManager::Instance().Get(TexID::RESULT_MAKIMONO);  // TexIDに追加が必要

    // 自分のタイム位置を中央に
    g_MyTimeY = MY_TIME_CENTER_Y;
    g_MyTimeTargetY = MY_TIME_CENTER_Y;

    // タイムアウト関連
    g_LoadingTimer = 0.0f;
    g_IsTimeout = false;

    // ランキング関連リセット
    g_RankingY = -300.0f;
    g_RankingTargetY = 385.0f;
    g_TopCount = 0;
    g_RankingLoaded = false;
    g_LoadFinished = false;
    g_MyRankIndex = -1;
    RankingLoader::GetInstance().Reset();

    // チャレンジモードをランキングローダーに設定
    RankingLoader::GetInstance().SetChallengeMode(GameManager::Instance().IsHardMode());

    // 前回のスレッドが残っていたら回収
    if (g_LoadThread) {
        if (g_LoadThread->joinable()) g_LoadThread->join();
        delete g_LoadThread;
        g_LoadThread = nullptr;
    }

    // クレジット関連
    g_CreditTexId = TextureManager::Instance().Get(TexID::RESULT_CREDIT);
    g_ShowCredit = false;

#ifdef _DEBUG
    char buf[128];
    sprintf_s(buf, "Result: NumberTexId=%d ClearTime=%.2f HasData=%d\n",
        g_NumberTexId,
        ResultData::GetInstance().GetClearTime(),
        ResultData::GetInstance().HasData() ? 1 : 0);
    OutputDebugStringA(buf);
#endif
}

//======================================================================
// 終了処理
//======================================================================
void Result_Finalize() {
    g_CreditTexId = -1;
    g_NumberTexId = -1;
    g_BgTexId = -1;
    g_ClearTimeTitleTexId = -1;
    g_MakimonoTexId = -1;

    // タイムアウトでスレッドが残っている場合はデタッチ
    if (g_LoadThread) {
        if (g_IsTimeout) {
            g_LoadThread->detach();  // バックグラウンドで勝手に終わらせる
        } else {
            if (g_LoadThread->joinable()) g_LoadThread->join();
        }
        delete g_LoadThread;
        g_LoadThread = nullptr;
    }
}

//======================================================================
// 更新
//======================================================================
void Result_Update() {
    float dt = static_cast<float>(TimeManager::GetInstance().GetElapsedTime());
    g_AccumulatedTime += dt;

    switch (g_State) {

        //--- フェードイン完了待ち ---
    case RESULT_STATE_FADE_IN:
        if (Fade_GetState() == FADE_STATE_FADE_IN_FINISHED) {
            g_State = RESULT_STATE_SHOW_MY_TIME;
			Audio_FadeInBGM(AudioID::BGM_RESULT, 2.0f);
        }
        break;

        //--- 自分のタイム表示（少し待つ） ---
    case RESULT_STATE_SHOW_MY_TIME:
        if (g_AccumulatedTime > 1.5) {
            g_State = RESULT_STATE_KEYINPUT_WAIT;
        }
        break;

        //--- Enter 入力待ち → ランキング取得開始 ---
    case RESULT_STATE_KEYINPUT_WAIT:
        if (g_AccumulatedTime > 3.0f) {
            g_State = RESULT_STATE_MY_TIME_SLIDE_DOWN;
            g_MyTimeTargetY = MY_TIME_BOTTOM_Y;
            g_KeyInputTime = g_AccumulatedTime;
            g_LoadingTimer = 0.0f;
            g_IsTimeout = false;

            g_LoadFinished = false;
            g_LoadThread = new std::thread([]() {
                double myTime = 0.0;
                if (ResultData::GetInstance().HasData()) {
                    myTime = ResultData::GetInstance().GetClearTime();
                }

                RankingLoader::GetInstance().LoadFromGoogleSheets();

                if (myTime > 0.0) {
                    RankingLoader::GetInstance().AddLocalEntry(myTime);
                }

                if (myTime > 0.0) {
                    RankingLoader::GetInstance().SubmitTime(myTime);
                }

                g_LoadFinished = true;
                });
        }
        break;

        //--- 自分のタイムが下にスライド ---
    case RESULT_STATE_MY_TIME_SLIDE_DOWN:
    {
        float slideSpeed = 8.0f;
        g_MyTimeY = Lerp(g_MyTimeY, g_MyTimeTargetY, slideSpeed * dt);
        g_LoadingTimer += dt;

        if (fabsf(g_MyTimeY - g_MyTimeTargetY) < 2.0f) {
            g_MyTimeY = g_MyTimeTargetY;
            g_State = RESULT_STATE_LOADING_RANKING;
        }
        break;
    }

    //--- ランキング読み込み完了待ち（タイムアウト付き） ---
    case RESULT_STATE_LOADING_RANKING:
    {
        g_LoadingTimer += dt;

        // タイムアウト処理
        if (!g_LoadFinished && g_LoadingTimer >= LOADING_TIMEOUT) {
            g_IsTimeout = true;

            // ダミーデータを4個に（自分含めて最大5個）
            double dummyTimes[4] = { 144.35, 179.18, 221.25, 267.54 };  // 3:15, 4:30, 5:30, 7:00

            // チャレンジモードなら1.5倍
            if (GameManager::Instance().IsHardMode()) {
                for (int i = 0; i < 4; ++i) {
                    dummyTimes[i] *= 1.5;
                }
            }

            double myTime = 0.0;
            if (ResultData::GetInstance().HasData()) {
                myTime = ResultData::GetInstance().GetClearTime();
            }

            // 5つのタイムをまとめてソート
            struct TimeEntry { double time; bool isPlayer; };
            TimeEntry allEntries[5] = {
                { dummyTimes[0], false },
                { dummyTimes[1], false },
                { dummyTimes[2], false },
                { dummyTimes[3], false },
                { myTime > 0.0 ? myTime : 9999.0, myTime > 0.0 },
            };

            // バブルソート（短い順が上位）
            for (int i = 0; i < 4; ++i) {
                for (int j = i + 1; j < 5; ++j) {
                    if (allEntries[j].time < allEntries[i].time) {
                        TimeEntry tmp = allEntries[i];
                        allEntries[i] = allEntries[j];
                        allEntries[j] = tmp;
                    }
                }
            }

            // トップ5にセット
            g_TopCount = 5;
            g_MyRankIndex = -1;
            for (int i = 0; i < 5; ++i) {
                g_TopEntries[i] = { allEntries[i].time };
                if (allEntries[i].isPlayer) {
                    g_MyRankIndex = i;
                }
            }

            g_RankingLoaded = true;
            g_State = RESULT_STATE_RANKING_SLIDE_IN;
            g_RankingY = -300.0f;

#ifdef _DEBUG
            OutputDebugStringA("RankingLoader: TIMEOUT - Using dummy data\n");
            for (int i = 0; i < g_TopCount; ++i) {
                int totalSec = static_cast<int>(g_TopEntries[i].time);
                int min = totalSec / 60;
                int sec = totalSec % 60;
                int cs = static_cast<int>((g_TopEntries[i].time - totalSec) * 100);
                char buf[128];
                sprintf_s(buf, "Rank %d: %02d:%02d.%02d%s\n",
                    i + 1, min, sec, cs,
                    (i == g_MyRankIndex) ? " <<YOU>>" : "");
                OutputDebugStringA(buf);
            }
#endif
            break;
        }

        // 正常完了
        if (g_LoadFinished) {
            if (g_LoadThread && g_LoadThread->joinable()) {
                g_LoadThread->join();
            }
            delete g_LoadThread;
            g_LoadThread = nullptr;

            auto top = RankingLoader::GetInstance().GetTopEntries(5);
            g_TopCount = static_cast<int>(top.size());
            for (int i = 0; i < g_TopCount && i < 5; ++i) {
                g_TopEntries[i] = top[i];
            }
            g_RankingLoaded = true;

            g_MyRankIndex = -1;
            if (ResultData::GetInstance().HasData()) {
                double myTime = ResultData::GetInstance().GetClearTime();
                for (int i = 0; i < g_TopCount; ++i) {
                    double diff = g_TopEntries[i].time - myTime;
                    if (diff < 0.0) diff = -diff;
                    if (diff < 0.015) {
                        g_MyRankIndex = i;
                        break;
                    }
                }
            }

            g_State = RESULT_STATE_RANKING_SLIDE_IN;
            g_RankingY = -300.0f;

#ifdef _DEBUG
            for (int i = 0; i < g_TopCount; ++i) {
                int totalSec = static_cast<int>(g_TopEntries[i].time);
                int min = totalSec / 60;
                int sec = totalSec % 60;
                int cs = static_cast<int>((g_TopEntries[i].time - totalSec) * 100);
                char buf[128];
                sprintf_s(buf, "Rank %d: %02d:%02d.%02d%s\n",
                    i + 1, min, sec, cs,
                    (i == g_MyRankIndex) ? " <<YOU>>" : "");
                OutputDebugStringA(buf);
            }
#endif
        }
        break;
    }

    //--- ランキングが上からスライドイン ---
    case RESULT_STATE_RANKING_SLIDE_IN:
    {
        float slideSpeed = 6.0f;
        g_RankingY = Lerp(g_RankingY, g_RankingTargetY, slideSpeed * dt);

        if (fabsf(g_RankingY - g_RankingTargetY) < 2.0f) {
            g_RankingY = g_RankingTargetY;
            g_State = RESULT_STATE_RANKING_SHOW;
        }
        break;
    }

    //--- ランキング表示中 → Enter でクレジットへ ---
    case RESULT_STATE_RANKING_SHOW:
        if (InputManager::Cmd().enter) {
            g_State = RESULT_STATE_RANKING_FADE_OUT;
            Fade_Start(0.5f, false);  // フェードアウト開始
        }
        break;

        //--- ランキング→クレジットへのフェードアウト完了待ち ---
    case RESULT_STATE_RANKING_FADE_OUT:
        if (Fade_GetState() == FADE_STATE_FADE_OUT_FINISHED) {
            g_ShowCredit = true;  // クレジット表示ON
            g_State = RESULT_STATE_CREDIT_FADE_IN;
            Fade_Start(0.5f, true);  // フェードイン開始
        }
        break;

        //--- クレジットフェードイン完了待ち ---
    case RESULT_STATE_CREDIT_FADE_IN:
        if (Fade_GetState() == FADE_STATE_FADE_IN_FINISHED) {
            g_State = RESULT_STATE_CREDIT_SHOW;
        }
        break;

        //--- クレジット表示中 → Enter でタイトルへ ---
    case RESULT_STATE_CREDIT_SHOW:
        if (InputManager::Cmd().enter) {
            g_State = RESULT_STATE_FADE_OUT;
            Fade_Start(1.0f, false);
        }
        break;

        //--- フェードアウト完了 → タイトルシーンへ ---
    case RESULT_STATE_FADE_OUT:
        if (Fade_GetState() == FADE_STATE_FADE_OUT_FINISHED) {
            Scene_SetNextScene(SCENE_TITLE);
            Audio_FadeOutBGM(1.0f);
        }
        break;
    default:
        break;
    }
}

//======================================================================
// 描画
//======================================================================
void Result_Draw() {
    float centerX = SCREEN_WIDTH * 0.5f;

    //------------------------------------------------------------------
    // 背景
    //------------------------------------------------------------------
    if (g_BgTexId >= 0) {
        Sprite_EnableCameraZoom(false);

        // 背景本体
        Sprite_Draw(g_BgTexId, 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT);

        // 加算合成で明るくする
        Direct3D_SetAlphaBlend(BLEND_ADD);
        for (int i = 0; i < 3; ++i) {
            Sprite_Draw(g_BgTexId, 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, { 1.0f, 1.0f, 1.0f, 0.5f });
        }
        Direct3D_SetAlphaBlend(BLEND_TRANSPARENT);

        Sprite_EnableCameraZoom(true);
    }

    //------------------------------------------------------------------
    // 「ボスクリアタイム」タイトル画像（ランキングの上）
    //------------------------------------------------------------------
    if (g_ClearTimeTitleTexId >= 0) {
        float titleScale = 1.0f;
        float titleW = (CLEAR_TIME_TITLE_W / 2) * titleScale;
        float titleH = (CLEAR_TIME_TITLE_H / 2) * titleScale;
        float titleX = centerX - titleW * 0.5f;
        float titleY = 150.0f - (titleH - (CLEAR_TIME_TITLE_H / 2)) * 0.5f;

        Sprite_EnableCameraZoom(false);

        // 巻物画像を背景として描画
        if (g_MakimonoTexId >= 0) {
            float makiScale = 2.15f;  // 巻物の拡大率
            float makiW = MAKIMONO_W * makiScale;
            float makiH = MAKIMONO_H * makiScale;
            float makiX = centerX - makiW * 0.5f;
            float makiY = titleY + (titleH - makiH) * 0.5f;  // タイトルの中央に配置

            // チャレンジモードなら赤、通常は白
            DirectX::XMFLOAT4 makiColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            if (GameManager::Instance().IsHardMode()) {
                makiColor = { 1.0f, 0.4f, 0.4f, 1.0f };  // 赤
            }

            Sprite_Draw(g_MakimonoTexId,
                makiX, makiY, makiW, makiH,
                0.0f, 0.0f, MAKIMONO_W, MAKIMONO_H,
                0.0f,
                makiColor);
        }

        // タイトル画像を描画
        Sprite_Draw(g_ClearTimeTitleTexId,
            titleX, titleY,
            titleW, titleH);

        Sprite_EnableCameraZoom(true);
    }

    //------------------------------------------------------------------
    // ランキング（上からスライドイン）
    //------------------------------------------------------------------
    if (g_RankingLoaded) {
        if (g_TopCount > 0) {
            float rankY = g_RankingY;
            float rankSpacing = 80.0f * 1.1f;  // 行間1.1倍
            float rankScale = 1.2f;  // 全体の文字サイズ

            for (int i = 0; i < g_TopCount && i < 5; ++i) {
                float y = rankY + i * rankSpacing;

                // 順位ごとの色（サイズは同じ）
                DirectX::XMFLOAT4 rankColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                switch (i) {
                case 0: rankColor = { 1.0f, 0.84f, 0.0f, 1.0f }; break;   // 金
                case 1: rankColor = { 0.75f, 0.75f, 0.75f, 1.0f }; break;  // 銀
                case 2: rankColor = { 0.8f, 0.5f, 0.2f, 1.0f }; break;     // 銅
                case 3: rankColor = { 0.6f, 0.6f, 0.7f, 1.0f }; break;    // 4位
                case 4: rankColor = { 0.5f, 0.5f, 0.6f, 1.0f }; break;     // 5位
                }

                // 自分のランクなら上下にイージング
                if (i == g_MyRankIndex) {
                    float t = static_cast<float>(g_AccumulatedTime);
                    float wave = (sinf(t * 5.0f) + 1.0f) * 0.5f;  // 0.0 ~ 1.0
                    y -= wave * 18.0f;  // 上に浮く
                }

                // 順位番号（中央寄せで配置）
                float cw = DRAW_CHAR_W * rankScale;
                float ch = DRAW_CHAR_H * rankScale;
                float rankNumX = centerX - 350.0f;  // 左に調整
                float rankNumY = y - (ch - DRAW_CHAR_H * 1.2f) * 0.5f;

                // タイム位置（中央寄せ）
                float timeW = CalcTimeWidth(DRAW_CHAR_W * rankScale, CHAR_SPACING * rankScale);
                float timeX = centerX - timeW * 0.5f + 50.0f;  // 少し右にオフセット

                // 描画
                DrawNumberChar(i + 1, rankNumX, rankNumY, cw, ch, rankColor);
                DrawTime(g_TopEntries[i].time, timeX, rankNumY, rankScale, rankColor);
            }
        } else {
            // データ0件
            float y = g_RankingY;
            DirectX::XMFLOAT4 goldColor = { 1.0f, 0.84f, 0.0f, 1.0f };
            float cw = DRAW_CHAR_W * 1.2f;
            float ch = DRAW_CHAR_H * 1.2f;
            float rankNumX = centerX - 350.0f;
            DrawNumberChar(1, rankNumX, y, cw, ch, goldColor);

            double myTime = ResultData::GetInstance().HasData()
                ? ResultData::GetInstance().GetClearTime() : 0.0;
            float timeW = CalcTimeWidth(DRAW_CHAR_W * 1.2f, CHAR_SPACING * 1.2f);
            float timeX = centerX - timeW * 0.5f + 50.0f;
            DrawTime(myTime, timeX, y, 1.2f, goldColor);
        }
    }


    //------------------------------------------------------------------
    // ローディング表示（中黒点滅）
    //------------------------------------------------------------------
    if (g_State == RESULT_STATE_LOADING_RANKING) {
        int dots = (static_cast<int>(g_AccumulatedTime * 3.0) % 4);
        float dotSize = DRAW_CHAR_W * 0.5f;
        float dotSpacing = dotSize + 10.0f;
        float totalWidth = 3 * dotSpacing;
        float dotX = centerX - totalWidth * 0.5f;
        float dotY = SCREEN_HEIGHT * 0.4f;

        for (int i = 0; i < 3; ++i) {
            // 表示中のドット数に応じてアルファを変える
            float alpha = (i < dots) ? 1.0f : 0.2f;
            DrawNumberChar(DOT_INDEX, dotX + i * dotSpacing, dotY,
                dotSize, dotSize,
                { 1.0f, 1.0f, 1.0f, alpha });
        }
    }

    //------------------------------------------------------------------
    // 自分のタイム（常時表示、Y座標がアニメーションする）
    //------------------------------------------------------------------
    double clearTime = 0.0;
    if (ResultData::GetInstance().HasData()) {
        clearTime = ResultData::GetInstance().GetClearTime();
    }

    // パルスアニメーション（大きく→元→大きく）
    float t = static_cast<float>(g_AccumulatedTime);
    float pulse = (sinf(t * 3.0f) + 1.0f) * 0.5f;  // 0.0 ~ 1.0
    float minScale = 2.0f;   // 元のサイズ
    float maxScale = 2.3f;   // 最大サイズ
    float scale = minScale + (maxScale - minScale) * pulse;

    DrawTimeCentered(clearTime, centerX, g_MyTimeY, scale);

    //------------------------------------------------------------------
    // クレジット画面
    //------------------------------------------------------------------
    if (g_ShowCredit && g_CreditTexId >= 0) {
        Sprite_EnableCameraZoom(false);

        Sprite_Draw(g_CreditTexId,
            0.0f, 0.0f,
            SCREEN_WIDTH, SCREEN_HEIGHT,
            0.0f, 0.0f,
            CREDIT_W, CREDIT_H,
            0.0f,
            { 1.0f, 1.0f, 1.0f, 1.0f });

        Sprite_EnableCameraZoom(true);
    }

}
