/*********************************************************************
 * \file   game_boss.cpp
 * \brief  ボス
 *
 * \author Ryoto Kikuchi
 * \date   2026/2/14
 *********************************************************************/
#include "game_boss.h"
#include "game_boss_state.h"
#include "texture.h"
#include "sprite.h"
#include "CollisionManager.h"
#include "collision.h"
#include "time_manager.h"
#include "game_stage.h"
#include "game_player.h"
#include "weapon_bow_bullet.h"
#include "game_boss_attacks.h"
#include "Audio.h"
#include "game_manager.h"
#include "camera.h"
#include <cmath>
#include <cstring>
#include <string>
#include "result_data.h" 
#include "media_player.h"
#include "direct3d.h"

Boss::Boss(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size)
    : GameObject(pos, size, ObjectTag::BOSS) {
}

Boss::~Boss() {
    Finalize();
}

void Boss::Initialize() {
    m_textureId = TextureManager::Instance().Get(TexID::BOSS);
    m_barrierTextureId = TextureManager::Instance().Get(TexID::BOSS_BARRIER);
    m_hpBarTextureId = TextureManager::Instance().Get(TexID::BOSS_HP_BAR);
    m_hpBarBgTextureId = TextureManager::Instance().Get(TexID::BOSS_HP_BAR_BG);

    // ハードモードの場合、HPを1.2倍にしてコンティニュー時全回復フラグを立てる
    if (GameManager::Instance().IsHardMode()) {
        m_maxHp = static_cast<int>(BASE_MAX_HP * 1.5f);  // 660
        m_shouldResetHp = true;
    } else {
        m_maxHp = BASE_MAX_HP;  // 550
        m_shouldResetHp = false;
    }

    m_hp = m_maxHp;
    m_isDead = false;
    m_showBarrier = false;

    // コライダー設定
    GetColliders().clear();
    AddCollider(GetPosition(), GetSize(), ColliderType::BODY);
    CollisionManager::GetInstance().AddObject(this);

    // ステージサイズ取得（攻撃の範囲制限などで使用）
    float stageWidth = 1920.0f;
    float stageHeight = 1088.0f;
    if (m_stage) {
        stageWidth = m_stage->GetWidth() * 64.0f;
        stageHeight = m_stage->GetHeight() * 64.0f;
    }

    // 通常攻撃マネージャー初期化
    m_normalAttackManager.Initialize(this, m_player);
    m_normalAttackManager.SetStageSize(stageWidth, stageHeight);
    m_normalAttackManager.AddAttack(new GuillotineDropAttack());
    m_normalAttackManager.AddAttack(new MaskBulletHorizontalAttack());
    m_normalAttackManager.AddAttack(new MaskBulletDiagonalAttack());
    m_normalAttackManager.AddAttack(new MaskBeamAttack());
    m_normalAttackManager.AddAttack(new ScratchAttack());

    // 必殺技マネージャー初期化
    m_specialAttackManager.Initialize(this, m_player);
    m_specialAttackManager.SetStageSize(stageWidth, stageHeight);
    m_specialAttackManager.AddAttack(new SpecialBeamAttack());
    m_specialAttackManager.AddAttack(new SpecialBulletStormAttack());

    // チャレンジモードなら攻撃間隔を0.8倍に
    if (GameManager::Instance().IsHardMode()) {
        m_normalAttackManager.SetTimeScale(0.6f);
        m_specialAttackManager.SetTimeScale(0.6f);
    }

    // バリアアニメーション作成
    if (!m_barrierAnim) {
        m_barrierAnim = new AnimPattern(
            m_barrierTextureId,
            BARRIER_ANIM_TOTAL,         // 総パターン数 (30)
            BARRIER_ANIM_COLS,          // 横のパターン数 (5)
            0.05f,                      // アニメーション速度
            { 0, 0 },                   // 開始位置
            { static_cast<uint32_t>(BARRIER_FRAME_WIDTH), static_cast<uint32_t>(BARRIER_FRAME_HEIGHT) },  // 1フレームのサイズ
            false,
            false                       // ループしない
        );
    }
    m_barrierAnimPlayer = new AnimPatternPlayer(m_barrierAnim);
    m_isBarrierAnimPlaying = false;
    m_barrierAnimPlayed = false;
    m_masksToDestroy.clear();


    // マスク生成
    for (auto* mask : m_masks) {
        if (mask) {
            mask->Finalize();
            delete mask;
        }
    }
    m_masks.clear();

    // 4種類のマスクを作成
    BossMask::MaskType maskTypes[] = {
        BossMask::MaskType::KATANA,
        BossMask::MaskType::KANABO,
        BossMask::MaskType::BOW,
        BossMask::MaskType::SPEAR,
        BossMask::MaskType::KATANA
    };

    for (int i = 0; i < MASK_COUNT; ++i) {
        auto* mask = new BossMask({ 0.0f, 0.0f }, { 64.0f * 2.0f, 64.0f * 2.0f }, this, maskTypes[i]);
        mask->Deactivate();
        m_masks.push_back(mask);
    }

    // 初期ステートは待機状態
    m_currentState = new BossWaitState(m_stage);
    m_currentState->Enter(this);

    // ボス初期位置を記録（浮遊移動の基準）
    m_basePos = GetPosition();
    m_floatTimer = 0.0f;

    // 右手生成
    if (m_rightHand) {
        m_rightHand->Finalize();
        delete m_rightHand;
    }
    m_rightHand = new BossHand(m_rightHandPos, { 64.0f * 2, 64.0f * 2 }, this, BossHand::HandSide::RIGHT);
    m_rightHand->SetBasePosition(m_rightHandPos);
    m_rightHand->Initialize();

    // 左手生成
    if (m_leftHand) {
        m_leftHand->Finalize();
        delete m_leftHand;
    }
    m_leftHand = new BossHand(m_leftHandPos, { 64.0f * 2, 64.0f * 2 }, this, BossHand::HandSide::LEFT);
    m_leftHand->SetBasePosition(m_leftHandPos);
    m_leftHand->Initialize();

    // ボスアイドルアニメーション初期化
    m_idleAnimTexIds[0] = TextureManager::Instance().Get(TexID::BOSS_IDLE_ANIM1);
    m_idleAnimTexIds[1] = TextureManager::Instance().Get(TexID::BOSS_IDLE_ANIM2);
    m_idleAnimTexIds[2] = TextureManager::Instance().Get(TexID::BOSS_IDLE_ANIM3);

    for (int i = 0; i < 3; ++i) {
        if (!m_idleAnim[i]) {
            m_idleAnim[i] = new AnimPattern(
                m_idleAnimTexIds[i],
                BOSS_ANIM_TOTAL,
                BOSS_ANIM_COLS,
                0.05f,
                { 0, 0 },
                { static_cast<uint32_t>(BOSS_FRAME_WIDTH), static_cast<uint32_t>(BOSS_FRAME_HEIGHT) },
                false,
                false
            );
        }
    }
    m_currentIdleIndex = 0;
    if (!m_idleAnimPlayer) {
        m_idleAnimPlayer = new AnimPatternPlayer(m_idleAnim[0]);
    } else {
        m_idleAnimPlayer->SetAnimPattern(m_idleAnim[0]);
        m_idleAnimPlayer->Reset();
    }

    // ボスダウンアニメーション初期化
    m_downAnimTexIds[0] = TextureManager::Instance().Get(TexID::BOSS_DOWN_ANIM1);
    m_downAnimTexIds[1] = TextureManager::Instance().Get(TexID::BOSS_DOWN_ANIM2);
    m_downAnimTexIds[2] = TextureManager::Instance().Get(TexID::BOSS_DOWN_ANIM3);

    for (int i = 0; i < 3; ++i) {
        if (!m_downAnim[i]) {
            m_downAnim[i] = new AnimPattern(
                m_downAnimTexIds[i],
                BOSS_ANIM_TOTAL,
                BOSS_ANIM_COLS,
                0.05f,
                { 0, 0 },
                { static_cast<uint32_t>(BOSS_FRAME_WIDTH), static_cast<uint32_t>(BOSS_FRAME_HEIGHT) },
                false,
                false
            );
        }
    }
    m_currentDownIndex = 0;
    if (!m_downAnimPlayer) {
        m_downAnimPlayer = new AnimPatternPlayer(m_downAnim[0]);
    } else {
        m_downAnimPlayer->SetAnimPattern(m_downAnim[0]);
        m_downAnimPlayer->Reset();
    }

    // イントロ動画初期化
    if (!m_introMovie) {
        m_introMovie = new MediaPlayer();
        if (!m_introMovie->Initialize(L"resource/movie/boss_intro.mp4", Direct3D_GetDevice())) {
            delete m_introMovie;
            m_introMovie = nullptr;
        }
    }
    m_introPlaying = false;

    // オーラ初期化
    if (!m_aura) {
        m_aura = new Aura();
    }
    m_aura->SetActive(true);
    m_aura->SetColor({ 0.6f, 0.1f, 1.5f, 1.0f });  // 紫
    m_aura->SetScale(3.0f, 3.0f);  // ボスを覆うサイズ
}

void Boss::Finalize() {
    CollisionManager::GetInstance().RemoveObject(this);

    // オーラ解放
    if (m_aura) {
        delete m_aura;
        m_aura = nullptr;
    }

    // 動画解放
    if (m_introMovie) {
        m_introMovie->Finalize();
        delete m_introMovie;
        m_introMovie = nullptr;
    }
    m_introPlaying = false;

    // ダウンアニメ解放
    if (m_downAnimPlayer) {
        delete m_downAnimPlayer;
        m_downAnimPlayer = nullptr;
    }
    for (int i = 0; i < 3; ++i) {
        if (m_downAnim[i]) {
            delete m_downAnim[i];
            m_downAnim[i] = nullptr;
        }
    }

    // アイドルアニメ解放
    if (m_idleAnimPlayer) {
        delete m_idleAnimPlayer;
        m_idleAnimPlayer = nullptr;
    }
    for (int i = 0; i < 3; ++i) {
        if (m_idleAnim[i]) {
            delete m_idleAnim[i];
            m_idleAnim[i] = nullptr;
        }
    }

    // 攻撃マネージャー解放
    m_normalAttackManager.Finalize();
    m_specialAttackManager.Finalize();

    // バリアアニメーション解放
    if (m_barrierAnimPlayer) {
        delete m_barrierAnimPlayer;
        m_barrierAnimPlayer = nullptr;
    }
    if (m_barrierAnim) {
        delete m_barrierAnim;
        m_barrierAnim = nullptr;
    }

    // ステート解放
    if (m_currentState) {
        delete m_currentState;
        m_currentState = nullptr;
    }

    // 仮面解放
    for (auto* mask : m_masks) {
        if (mask) {
            mask->Finalize();
            delete mask;
        }
    }
    m_masks.clear();

    // 手の解放
    if (m_rightHand) {
        m_rightHand->Finalize();
        delete m_rightHand;
        m_rightHand = nullptr;
    }
    if (m_leftHand) {
        m_leftHand->Finalize();
        delete m_leftHand;
        m_leftHand = nullptr;
    }
}


void Boss::Update() {
    if (m_introPlaying) {
        if (m_introMovie) {
            // 実時間で更新（TimeScaleの影響を受けない）
            double dt = TimeManager::GetInstance().GetElapsedTime();
            m_introMovie->Update(dt);

            if (m_introMovie->IsFinished()) {
                m_introPlaying = false;
                m_introPlayed = true;

                // 動画リソース解放
                m_introMovie->Finalize();
                delete m_introMovie;
                m_introMovie = nullptr;
                TimeManager::GetInstance().WatchTimerStart();
            }
        }
        return;  // 動画中はボスの通常更新をスキップ
    }

    // リセット中のスムーズ移動処理
    if (m_isResetting) {
        float dt = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());
        m_resetTimer += dt;
        m_floatTimer += dt;  // 浮遊タイマーも進める

        // リセット完了時の目標位置を浮遊位置として計算
        float targetOffsetX = sinf(m_floatTimer * BOSS_FLOAT_SPEED_X) * BOSS_FLOAT_RANGE_X;
        float targetOffsetY = sinf(m_floatTimer * BOSS_FLOAT_SPEED_Y) * BOSS_FLOAT_RANGE_Y;
        DirectX::XMFLOAT2 targetPos = {
            m_basePos.x + targetOffsetX,
            m_basePos.y + targetOffsetY
        };

        float t = m_resetTimer / RESET_MOVE_TIME;
        if (t >= 1.0f) {
            t = 1.0f;
            m_isResetting = false;
            SetPosition(targetPos);  // 浮遊位置に設定
        } else {
            // イーズアウト補間（開始位置から現在の浮遊位置へ）
            float easeT = 1.0f - (1.0f - t) * (1.0f - t);
            DirectX::XMFLOAT2 newPos = {
                m_resetStartPos.x + (targetPos.x - m_resetStartPos.x) * easeT,
                m_resetStartPos.y + (targetPos.y - m_resetStartPos.y) * easeT
            };
            SetPosition(newPos);
        }

        // コライダー位置更新
        if (!GetColliders().empty()) {
            GetColliders()[0].SetColliderPos(GetPosition());
        }

        // 手の更新
        if (m_rightHand && m_rightHand->IsActive()) {
            m_rightHand->Update();
        }
        if (m_leftHand && m_leftHand->IsActive()) {
            m_leftHand->Update();
        }

        // オーラ更新
        if (m_aura) {
            DirectX::XMFLOAT2 center = {
                GetPosition().x + GetSize().x * 0.5f,
                GetPosition().y + 64.0f * 6.0f
            };
            m_aura->Update(center, GetSize());
        }

        // アイドルアニメーション更新
        if (m_idleAnimPlayer) {
            m_idleAnimPlayer->Update();
            if (m_idleAnimPlayer->IsEnd()) {
                m_currentIdleIndex = (m_currentIdleIndex + 1) % 3;
                m_idleAnimPlayer->SetAnimPattern(m_idleAnim[m_currentIdleIndex]);
                m_idleAnimPlayer->Reset();
            }
        }

        return;
    }
        
    // 撃破演出中はステート更新 + 手の更新を行う
    if (m_isDead) {
        float dt = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());
        if (m_currentState) {
            m_currentState->Update(this, dt);
        }
        if (m_rightHand && m_rightHand->IsActive()) {
            m_rightHand->Update();
        }
        if (m_leftHand && m_leftHand->IsActive()) {
            m_leftHand->Update();
        }
        // オーラ更新
        if (m_aura) {
            DirectX::XMFLOAT2 center = {
                GetPosition().x + GetSize().x * 0.5f,
                GetPosition().y + 64.0f * 5.0f
            };
            m_aura->Update(center, GetSize());
        }
        return;
    }

    float dt = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());

    // 無敵タイマー更新
    if (m_damageInvincibleTimer > 0.0f) {
        m_damageInvincibleTimer -= dt;
        if (m_damageInvincibleTimer < 0.0f) {
            m_damageInvincibleTimer = 0.0f;
        }
    }

    // マスクの遅延破壊処理（コリジョン処理完了後に実行）
    ProcessPendingMaskDestroys();

    // ステート更新
    if (m_currentState) {
        m_currentState->Update(this, dt);
    }

    // バリアアニメーション更新
    if (m_isBarrierAnimPlaying && m_barrierAnimPlayer) {
        m_barrierAnimPlayer->Update();

        // アニメーション終了チェック
        if (m_barrierAnimPlayer->IsEnd()) {
            m_isBarrierAnimPlaying = false;
        }
    }

    // 仮面更新
    for (auto* mask : m_masks) {
        if (mask && mask->IsActive()) {
            mask->Update();
        }
    }

    // ダウン状態の判定
    bool wasDown = m_isDown;
    m_isDown = false;
    if (m_currentState) {
        const char* stateName = m_currentState->GetStateName();
        if (strcmp(stateName, "Stunned") == 0) {
            m_isDown = true;
        }
    }

    // ダウン状態に入った瞬間にアニメリセット
    if (m_isDown && !wasDown) {
        m_currentDownIndex = 0;
        if (m_downAnimPlayer && m_downAnim[0]) {
            m_downAnimPlayer->SetAnimPattern(m_downAnim[0]);
            m_downAnimPlayer->Reset();
        }
    }

    // ダウンアニメーション更新
    if (m_isDown && m_downAnimPlayer) {
        m_downAnimPlayer->Update();

        if (m_downAnimPlayer->IsEnd()) {
            m_currentDownIndex = (m_currentDownIndex + 1) % 3;
            m_downAnimPlayer->SetAnimPattern(m_downAnim[m_currentDownIndex]);
            m_downAnimPlayer->Reset();
        }
    }


    // アイドルアニメーション更新
    if (!m_isDown && m_idleAnimPlayer) {
        m_idleAnimPlayer->Update();

        if (m_idleAnimPlayer->IsEnd()) {
            m_currentIdleIndex = (m_currentIdleIndex + 1) % 3;
            m_idleAnimPlayer->SetAnimPattern(m_idleAnim[m_currentIdleIndex]);
            m_idleAnimPlayer->Reset();
        }
    }

    // オーラ更新
    if (m_aura) {
        DirectX::XMFLOAT2 center = {
            GetPosition().x + GetSize().x * 0.5f,
            GetPosition().y + 64.0f * 6.0f
        };
        m_aura->Update(center, GetSize());
    }
    //ボスエフェクト演出
    if (m_currentState)
    {
        const char* stateName = m_currentState->GetStateName();
        if (strcmp(stateName, "Charge") == 0)
        {
            BossChargeState* state = dynamic_cast<BossChargeState*>(m_currentState);
            if(state) m_chargeEffect = (state->GetStateTime() / state->GetChargeDuration());
        }
        else if (strcmp(stateName, "Special") == 0)
        {
            if (!m_isReadyEffect)
            {
                m_isReadyEffect = true;
                m_readyEffect = 1.0f;
                m_chargeEffect = 0.0f;
            }
        }
        else if (strcmp(stateName, "Stunned") == 0)
        {
            if (!m_isStunnedEffect)
            {
                m_isStunnedEffect = true;
                m_stunnedEffect = 1.0f;
                m_chargeEffect = 0.0f;
            }
        }
        else
        {
            m_isReadyEffect = false;
            m_isStunnedEffect = false;
        }

        if (m_isReadyEffect)
        {
            if (m_readyEffect > 0.0f) {
                m_readyEffect -= 0.015f;
                if (m_readyEffect < 0.0f) m_readyEffect = 0.0f;
            }
        }
        if (m_isStunnedEffect)
        {
            if (m_stunnedEffect > 0.0f) {
                m_stunnedEffect -= 0.007f;
                if (m_stunnedEffect < 0.0f) m_stunnedEffect = 0.0f;
            }
        }

        GameManager::Instance().GetPostProcessManager()->SetBossChargeEffect(m_chargeEffect);
        GameManager::Instance().GetPostProcessManager()->SetBossReadyEffect(m_readyEffect);
        GameManager::Instance().GetPostProcessManager()->SetBossStunEffect(m_stunnedEffect);
    }

    // ボス浮遊移動（大技・スタン中以外）
    {
        bool shouldFloat = true;

        if (m_currentState) {
            const char* stateName = m_currentState->GetStateName();
            if (strcmp(stateName, "Special") == 0 ||
                strcmp(stateName, "Stunned") == 0 ||
                strcmp(stateName, "Defeated") == 0 ||
                strcmp(stateName, "Wait") == 0) {
                shouldFloat = false;
            }
        }

        if (shouldFloat) {
            m_floatTimer += dt;

            float offsetX = sinf(m_floatTimer * BOSS_FLOAT_SPEED_X) * BOSS_FLOAT_RANGE_X;
            float offsetY = sinf(m_floatTimer * BOSS_FLOAT_SPEED_Y) * BOSS_FLOAT_RANGE_Y;

            DirectX::XMFLOAT2 newPos = {
                m_basePos.x + offsetX,
                m_basePos.y + offsetY
            };
            SetPosition(newPos);
        }
    }

    // コライダー位置を更新
    if (!GetColliders().empty()) {
        GetColliders()[0].SetColliderPos(GetPosition());
    }

    // 手の更新
    if (m_rightHand && m_rightHand->IsActive()) {
        m_rightHand->Update();
    }
    if (m_leftHand && m_leftHand->IsActive()) {
        m_leftHand->Update();
    }
}

void Boss::Draw() {
    // イントロ動画再生中は動画だけ描画
    if (m_introPlaying && m_introMovie && m_introMovie->GetSRV()) {
        // ボスやHP等は一切描画しない
        return;
    }
    
    // 撃破演出中：ボス本体＋手は描画、ステートのDraw（フラッシュ等）も行う
    if (m_isDead) {
        // オーラの描画
        if (m_aura) {
            m_aura->Draw();
        }

        // ダウン中はダウンアニメ、それ以外はアイドルアニメ
        if (m_isDown && m_downAnimPlayer) {
            int pattern = m_downAnimPlayer->GetPattern();
            int col = pattern % BOSS_ANIM_COLS;
            int row = pattern / BOSS_ANIM_COLS;

            float uvX = col * BOSS_FRAME_WIDTH;
            float uvY = row * BOSS_FRAME_HEIGHT;

            int currentTexId = m_downAnimTexIds[m_currentDownIndex];
            if (currentTexId >= 0) {
                Sprite_ScrollDraw(currentTexId,
                    GetPosition().x, GetPosition().y,
                    GetSize().x, GetSize().y,
                    uvX, uvY,
                    BOSS_FRAME_WIDTH, BOSS_FRAME_HEIGHT,
                    0.0f,
                    { 1.0f, 1.0f, 1.0f, 1.0f });
            }
        } else if (m_idleAnimPlayer) {
            // 既存のアイドルアニメ描画
            int pattern = m_idleAnimPlayer->GetPattern();
            int col = pattern % BOSS_ANIM_COLS;
            int row = pattern / BOSS_ANIM_COLS;

            float uvX = col * BOSS_FRAME_WIDTH;
            float uvY = row * BOSS_FRAME_HEIGHT;

            int currentTexId = m_idleAnimTexIds[m_currentIdleIndex];
            if (currentTexId >= 0) {
                Sprite_ScrollDraw(currentTexId,
                    GetPosition().x, GetPosition().y,
                    GetSize().x, GetSize().y,
                    uvX, uvY,
                    BOSS_FRAME_WIDTH, BOSS_FRAME_HEIGHT,
                    0.0f,
                    { 1.0f, 1.0f, 1.0f, 1.0f });
            }
        } else if (m_textureId >= 0) {
            Sprite_ScrollDraw(m_textureId, GetPosition().x, GetPosition().y,
                GetSize().x, GetSize().y);
        }

        // 手の描画
        if (m_rightHand) {
            m_rightHand->Draw();
        }
        if (m_leftHand) {
            m_leftHand->Draw();
        }
        return;
    }

    if (!IsActive()) return;

    // オーラ描画
    if (m_aura) {
        m_aura->Draw();
    }

    // ダウン中はダウンアニメ、それ以外はアイドルアニメ
    if (m_isDown && m_downAnimPlayer) {
        int pattern = m_downAnimPlayer->GetPattern();
        int col = pattern % BOSS_ANIM_COLS;
        int row = pattern / BOSS_ANIM_COLS;

        float uvX = col * BOSS_FRAME_WIDTH;
        float uvY = row * BOSS_FRAME_HEIGHT;

        int currentTexId = m_downAnimTexIds[m_currentDownIndex];
        if (currentTexId >= 0) {
            Sprite_ScrollDraw(currentTexId,
                GetPosition().x, GetPosition().y,
                GetSize().x, GetSize().y,
                uvX, uvY,
                BOSS_FRAME_WIDTH, BOSS_FRAME_HEIGHT,
                0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    } else if (m_idleAnimPlayer) {
        // 既存のアイドルアニメ描画
        int pattern = m_idleAnimPlayer->GetPattern();
        int col = pattern % BOSS_ANIM_COLS;
        int row = pattern / BOSS_ANIM_COLS;

        float uvX = col * BOSS_FRAME_WIDTH;
        float uvY = row * BOSS_FRAME_HEIGHT;

        int currentTexId = m_idleAnimTexIds[m_currentIdleIndex];
        if (currentTexId >= 0) {
            Sprite_ScrollDraw(currentTexId,
                GetPosition().x, GetPosition().y,
                GetSize().x, GetSize().y,
                uvX, uvY,
                BOSS_FRAME_WIDTH, BOSS_FRAME_HEIGHT,
                0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    } else if (m_textureId >= 0) {
        Sprite_ScrollDraw(m_textureId, GetPosition().x, GetPosition().y,
            GetSize().x, GetSize().y);
    }

    // バリア描画
    if (m_barrierTextureId >= 0) {
        if (m_isBarrierAnimPlaying && m_barrierAnimPlayer) {
            // アニメーション再生中
            int pattern = m_barrierAnimPlayer->GetPattern();
            int col = pattern % BARRIER_ANIM_COLS;
            int row = pattern / BARRIER_ANIM_COLS;

            float uvX = col * BARRIER_FRAME_WIDTH;
            float uvY = row * BARRIER_FRAME_HEIGHT;

            // バリアの描画サイズ（ボスより少し大きく）
            float drawW = GetSize().x + 64.0f;
            float drawH = GetSize().y + 64.0f;
            float drawX = GetPosition().x - 32.0f;
            float drawY = GetPosition().y - 32.0f;

            Sprite_ScrollDraw(m_barrierTextureId,
                drawX, drawY,
                drawW, drawH,
                uvX, uvY,
                BARRIER_FRAME_WIDTH, BARRIER_FRAME_HEIGHT,
                0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        } else if (m_showBarrier) {
            // バリア表示中（最初のフレーム）
            float drawW = GetSize().x + 64.0f;
            float drawH = GetSize().y + 64.0f;
            float drawX = GetPosition().x - 32.0f;
            float drawY = GetPosition().y - 32.0f;

            Sprite_ScrollDraw(m_barrierTextureId,
                drawX, drawY,
                drawW, drawH,
                0.0f, 0.0f,  // 最初のフレーム
                BARRIER_FRAME_WIDTH, BARRIER_FRAME_HEIGHT,
                0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    // 仮面
    for (auto* mask : m_masks) {
        if (mask) {
            mask->Draw();
        }
    }

    // ステートの描画（攻撃など）
    if (m_currentState) {
        m_currentState->Draw(this);
    }

    // 手の描画
    if (m_rightHand) {
        m_rightHand->Draw();
    }
    if (m_leftHand) {
        m_leftHand->Draw();
    }

    // HPバー
    if (!m_hideHpBar && m_hpBarBgTextureId >= 0 && m_hpBarTextureId >= 0) {
        float barWidth = 1500.0f;
        float barHeight = 120.0f;
        float marginBottom = 0.0f;

        float posX = (SCREEN_WIDTH - barWidth) * 0.5f;
        float posY = SCREEN_HEIGHT - marginBottom - barHeight;

        Sprite_EnableCameraZoom(false);

        // HP量に応じてUVで切り取る
        float hpRatio = static_cast<float>(m_hp) / static_cast<float>(m_maxHp);
        float currentBarWidth = barWidth * hpRatio;

        // テクスチャの実サイズを取得
        float texW = 1500.0f;
        float texH = 120.0f;

        // UVで左端からhpRatio分だけ切り取って描画
        Sprite_Draw(m_hpBarTextureId,
            posX, posY,
            currentBarWidth, barHeight,
            0.0f, 0.0f,
            texW * hpRatio, texH,
            {2.0f, 1.0f, 1.0f, 1.0f});

        // 背景
        Sprite_Draw(m_hpBarBgTextureId, posX, posY, barWidth, barHeight);

        Sprite_EnableCameraZoom(true);
    }
}

void Boss::OnCollision(const GameObject* otherObject, const Collider* myCollider,
    const Collider* otherCollider, COLLISION_VEC hitVec) {
    if (!otherObject || !otherCollider || m_isDead) return;

    // 無敵中はダメージを受けない
    if (IsInvincible()) return;

    // プレイヤーの攻撃判定
    bool isPlayerAttack = (otherCollider->GetType() == ColliderType::ATTACK);
    bool isBullet = (otherObject->GetTag() == ObjectTag::BULLET);

    if (isPlayerAttack || isBullet) {
        // バリア中は効果音を鳴らしてダメージ無効
        if (m_showBarrier) {
            Audio_PlaySE(AudioID::SE_BARRIER_HIT);  // バリアヒット音
            return;
        }

        // 無敵中はダメージを受けない
        if (IsInvincible()) return;

		// ダメージ処理
        if (m_currentState && m_currentState->CanTakeDamage()) {
            // プレイヤーから武器情報を取得してダメージ計算
            int damage = CalcDamageFromPlayer();
            m_currentState->OnDamage(this, damage);
            SetInvincible(DAMAGE_INVINCIBLE_TIME);
        }
    }
}

void Boss::ChangeState(IBossState* newState) {
    if (m_currentState) {
        m_currentState->Exit(this);
        delete m_currentState;
    }
    m_currentState = newState;
    if (m_currentState) {
        m_currentState->Enter(this);
    }
}

void Boss::TakeDamage(int damage) {
    if (m_isDead) return;

    m_hp -= damage;

	Audio_PlaySE(AudioID::SE_BOSS_DAMAGE);

#ifdef _DEBUG
    OutputDebugStringA(("Boss HP: " + std::to_string(m_hp) + "/" +
        std::to_string(m_maxHp) + "\n").c_str());
#endif

    if (m_hp <= 0) {
        m_hp = 0;
        m_isDead = true;

        TimeManager::GetInstance().WatchTimerStop();
        ResultData::GetInstance().SetClearTime(
            TimeManager::GetInstance().WatchGetTime()
        );

#ifdef _DEBUG
        OutputDebugStringA("Boss: DEAD!\n");
#endif
    }
}

void Boss::SpawnMasks() {
    // GetPosition() ではなく m_basePos（初期位置）を基準にする
    float bossX = m_basePos.x + GetSize().x * 0.5f;
    float bossY = m_basePos.y + GetSize().y * 0.5f;

    float maskSize = 64.0f * 2.0f;
    float halfSize = maskSize * 0.5f;

    // パターンをランダムに選択
    m_maskPattern = rand() % 2;

    DirectX::XMFLOAT2 positions[5];

    switch (m_maskPattern) {
    case 0:
    {
		// 上段2体、下段3体のパターン
        float upperSpacing = 64.0f * 8.0f;
        float upperY = bossY - 64.0f * 4.5f - halfSize;

        float lowerSpacing = 64.0f * 11.0f;
        float lowerY = bossY + 64.0f * 2.5f;

        positions[0] = { bossX - upperSpacing - halfSize, upperY };
        positions[1] = { bossX + upperSpacing - halfSize, upperY };

        positions[2] = { bossX - lowerSpacing - halfSize, lowerY };
        positions[3] = { bossX - halfSize,                lowerY };
        positions[4] = { bossX + lowerSpacing - halfSize, lowerY };
        break;
    }
    case 1:
    {
		// 上段3体、下段2体のパターン
        float upperSpacing = 64.0f * 8.0f;
        float upperY = bossY - 64.0f * 4.5f - halfSize;

        float lowerSpacing = 64.0f * 11.0f;
        float lowerY = bossY + 64.0f * 2.5f;

        positions[0] = { bossX - upperSpacing - halfSize, upperY };
        positions[1] = { bossX - halfSize,                upperY };
        positions[2] = { bossX + upperSpacing - halfSize, upperY };

        positions[3] = { bossX - lowerSpacing - halfSize, lowerY };
        positions[4] = { bossX + lowerSpacing - halfSize, lowerY };
        break;
    }
    }

    for (int i = 0; i < static_cast<int>(m_masks.size()) && i < MASK_COUNT; ++i) {
        m_masks[i]->SetMaskPosition(positions[i]);
        m_masks[i]->SetMaskSize({ maskSize, maskSize });
        m_masks[i]->Reset();
        m_masks[i]->Activate();
        m_masks[i]->Initialize();
    }

#ifdef _DEBUG
    OutputDebugStringA(("Boss: Masks spawned! Pattern: " +
        std::to_string(m_maskPattern) +
        (m_maskPattern == 0 ? " (Upper2 + Lower3)" : " (Upper3 + Lower2)") + "\n").c_str());
#endif
}


void Boss::DestroyAllMasks() {
    for (auto* mask : m_masks) {
        if (mask) {
            mask->Deactivate();
        }
    }
}

int Boss::GetDestroyedMaskCount() const {
    int count = 0;
    for (const auto* mask : m_masks) {
        if (mask && mask->IsDestroyed()) {
            ++count;
        }
    }
    return count;
}

bool Boss::AreAllMasksDestroyed() const {
    return GetDestroyedMaskCount() >= MASK_COUNT;
}

void Boss::SetPlayer(Player* player) {
    m_player = player;
    // AttackManagerにもプレイヤー参照を更新
    m_normalAttackManager.SetPlayer(player);
    m_specialAttackManager.SetPlayer(player);
}

void Boss::SetStage(Stage* stage) {
    m_stage = stage;
    if (stage) {
        float width = stage->GetWidth() * 64.0f;
        float height = stage->GetHeight() * 64.0f;
        m_normalAttackManager.SetStageSize(width, height);
        m_specialAttackManager.SetStageSize(width, height);
    }
}

void Boss::PlayBarrierBreakAnim() {
    // 既にアニメーション再生中なら何もしない
    if (m_isBarrierAnimPlaying) {
        return;
    }

    if (m_barrierAnimPlayer) {
        m_barrierAnimPlayer->Reset();
        m_isBarrierAnimPlaying = true;
    }

    m_showBarrier = false;
}

std::vector<BossMask*> Boss::GetActiveMasks() const {
    std::vector<BossMask*> activeMasks;
    activeMasks.reserve(m_masks.size());

    for (auto* mask : m_masks) {
        if (mask && mask->IsActive() && !mask->IsDestroyed()) {
            activeMasks.push_back(mask);
        }
    }

    return activeMasks;
}

int Boss::GetActiveMaskCount() const {
    int count = 0;
    for (const auto* mask : m_masks) {
        if (mask && mask->IsActive() && !mask->IsDestroyed()) {
            ++count;
        }
    }
    return count;
}

void Boss::DrawOverlay() {
    if (m_isDead && m_currentState) {
        m_currentState->Draw(this);
    }
}

void Boss::Reset() {
    // HP・状態リセット
    // チャレンジモード（m_shouldResetHp == true）の場合のみMAXにリセット
    // 通常モードでは現在のHPを引き継ぐ
    if (m_shouldResetHp) {
        m_hp = m_maxHp;
    }

    if (m_introMovie) {
        m_introMovie->Finalize();
        delete m_introMovie;
        m_introMovie = nullptr;
    }
    m_introPlaying = false;

    // オーラリセット
    if (m_aura) {
        m_aura->SetActive(true);
        m_aura->SetColor({ 0.6f, 0.1f, 1.5f, 1.0f });
        m_aura->SetScale(3.0f, 3.0f);
    }

    m_isDead = false;
    m_showBarrier = false;
    m_damageInvincibleTimer = 0.0f;
    m_barrierAnimPlayed = false;
    m_masksToDestroy.clear();

    // アクティブな攻撃のProjectileを全てクリア
    if (m_normalAttackManager.GetActiveAttack()) {
        m_normalAttackManager.GetActiveAttack()->ClearAllProjectiles();
    }
    if (m_specialAttackManager.GetActiveAttack()) {
        m_specialAttackManager.GetActiveAttack()->ClearAllProjectiles();
    }

    // 攻撃マネージャーの現在の攻撃を停止
    m_normalAttackManager.StopCurrentAttack();
    m_specialAttackManager.StopCurrentAttack();

    // 全攻撃を一度クリアして再登録
    m_normalAttackManager.Finalize();
    m_specialAttackManager.Finalize();

    // 攻撃マネージャー再初期化
    float stageWidth = 1920.0f;
    float stageHeight = 1088.0f;
    if (m_stage) {
        stageWidth = m_stage->GetWidth() * 64.0f;
        stageHeight = m_stage->GetHeight() * 64.0f;
    }

    m_normalAttackManager.Initialize(this, m_player);
    m_normalAttackManager.SetStageSize(stageWidth, stageHeight);
    m_normalAttackManager.AddAttack(new GuillotineDropAttack());
    m_normalAttackManager.AddAttack(new MaskBulletHorizontalAttack());
    m_normalAttackManager.AddAttack(new MaskBulletDiagonalAttack());
    m_normalAttackManager.AddAttack(new MaskBeamAttack());
    m_normalAttackManager.AddAttack(new ScratchAttack());

    m_specialAttackManager.Initialize(this, m_player);
    m_specialAttackManager.SetStageSize(stageWidth, stageHeight);
    m_specialAttackManager.AddAttack(new SpecialBeamAttack());
    m_specialAttackManager.AddAttack(new SpecialBulletStormAttack());

    // チャレンジモードなら攻撃間隔を0.8倍に
    if (GameManager::Instance().IsHardMode()) {
        m_normalAttackManager.SetTimeScale(0.6f);
        m_specialAttackManager.SetTimeScale(0.6f);
    }

    m_normalAttackManager.SetActive(false);
    m_specialAttackManager.SetActive(false);

    // マスクを完全に非表示・非アクティブ状態にリセット
    BossMask::MaskType maskTypes[] = {
        BossMask::MaskType::KATANA,
        BossMask::MaskType::KANABO,
        BossMask::MaskType::BOW,
        BossMask::MaskType::SPEAR,
        BossMask::MaskType::KATANA
    };

    for (int i = 0; i < static_cast<int>(m_masks.size()); ++i) {
        auto* mask = m_masks[i];
        if (mask) {
            CollisionManager::GetInstance().RemoveObject(mask);
            mask->Reset();
            mask->SetMaskType(maskTypes[i]);
            mask->Deactivate();
        }
    }

    // バリアアニメーションリセット
    m_isBarrierAnimPlaying = false;
    if (m_barrierAnimPlayer) {
        m_barrierAnimPlayer->Reset();
    }

    // ステートを待機状態に戻す（ただしリセット中は遷移しない）
    if (m_currentState) {
        m_currentState->Exit(this);
        delete m_currentState;
    }
    m_currentState = new BossWaitState(m_stage);
    m_currentState->Enter(this);

    // コライダー再有効化
    for (auto& col : GetColliders()) {
        col.SetActive(true);
    }
    SetActive(true);

    // スムーズ移動開始（現在位置から基準位置へ）
    m_resetStartPos = GetPosition();
    m_resetTimer = 0.0f;
    m_isResetting = true;
    m_floatTimer = 0.0f;  // 浮遊タイマーを0にリセット（基準位置からスタート）

    // アイドルアニメもリセット
    m_currentIdleIndex = 0;
    if (m_idleAnimPlayer && m_idleAnim[0]) {
        m_idleAnimPlayer->SetAnimPattern(m_idleAnim[0]);
        m_idleAnimPlayer->Reset();
    }

    // 手のリセット（スムーズ移動付き）
    if (m_rightHand) {
        m_rightHand->ResetSmooth();
    }
    if (m_leftHand) {
        m_leftHand->ResetSmooth();
    }

#ifdef _DEBUG
    OutputDebugStringA("Boss: Reset complete!\n");
#endif
}


void Boss::RequestMaskDestroy(BossMask* mask) {
    if (!mask) return;

    // 既にリストにあれば追加しない
    for (auto* m : m_masksToDestroy) {
        if (m == mask) return;
    }

    m_masksToDestroy.push_back(mask);
}

void Boss::ProcessPendingMaskDestroys() {
    if (m_masksToDestroy.empty()) return;

    for (auto* mask : m_masksToDestroy) {
        if (mask && !mask->IsDestroyed()) {
            // 実際の破壊処理
            mask->Deactivate();

            // プレイヤーに憑依処理を通知
            if (m_player) {
                // マスクタイプ → Enemy::Type 変換
                Enemy::Type enemyType = Enemy::Type::KATANA;
                switch (mask->GetMaskType()) {
                case BossMask::MaskType::KATANA:  enemyType = Enemy::Type::KATANA; break;
                case BossMask::MaskType::KANABO:  enemyType = Enemy::Type::KANABO; break;
                case BossMask::MaskType::BOW:     enemyType = Enemy::Type::BOW;    break;
                case BossMask::MaskType::SPEAR:   enemyType = Enemy::Type::YARI;   break;
                }

                m_player->OnMaskDestroyed(mask->GetPosition(), enemyType);

                // PostProcessエフェクトをクリア
                auto* pp = GameManager::Instance().GetPostProcessManager();
                pp->SetFlashEffect(0.0f);
                pp->SetGlitchEffect(0.0f);
                pp->SetChromaEffect(0.0f);
                pp->SetMonoEffect(0.0f);
            }

            // 破壊フラグを立てる
            mask->MarkAsDestroyed();

#ifdef _DEBUG
            OutputDebugStringA("BossMask: Destroyed via deferred processing\n");
#endif
        }
    }

    // 全マスク破壊時にバリアアニメーション再生（1回だけ）
    if (AreAllMasksDestroyed() && !m_barrierAnimPlayed) {
        PlayBarrierBreakAnim();
        m_barrierAnimPlayed = true;
        Audio_PlaySEMulti(AudioID::SE_BARRIER_BREAK);
    }

    m_masksToDestroy.clear();
}


void Boss::StartIntroMovie() {
    // 既に再生済みならスキップ（コンティニュー対応）
    if (m_introPlayed) return;

    if (m_introMovie) {
        m_introMovie->Play();
        m_introPlaying = true;
        TimeManager::GetInstance().WatchTimerStop();
    } else {
        // 動画ファイルが無い場合はスキップ
        m_introPlayed = true;
    }
}

void Boss::SkipIntroMovie() {
    if (m_introPlaying) {
        m_introPlaying = false;
        m_introPlayed = true;

        if (m_introMovie) {
            m_introMovie->Finalize();
            delete m_introMovie;
            m_introMovie = nullptr;
        }
        TimeManager::GetInstance().WatchTimerStart();
    }
}

ID3D11ShaderResourceView* Boss::GetIntroMovieSRV() const {
    if (m_introMovie) return m_introMovie->GetSRV();
    return nullptr;
}

UINT Boss::GetIntroMovieWidth() const {
    return m_introMovie ? m_introMovie->GetWidth() : 1920;
}

UINT Boss::GetIntroMovieHeight() const {
    return m_introMovie ? m_introMovie->GetHeight() : 1080;
}

int Boss::CalcDamageFromPlayer() const {
    if (!m_player) return 10; // デフォルト

    // 武器タイプを取得（enumの順番: KATANA=0, SPEAR=1, KANABO=2, BOW=3）
    int weaponIndex = static_cast<int>(m_player->GetWeaponType());

    // 範囲チェック
    if (weaponIndex < 0 || weaponIndex > 3) {
        weaponIndex = 0;
    }

    // 特殊攻撃かどうか
    bool isSpecial = m_player->IsSpecialAttack();

    return DAMAGE_TABLE[weaponIndex][isSpecial ? 1 : 0];
}

//==============================================================================
// BossMask
//==============================================================================
BossMask::BossMask(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size, Boss* owner, MaskType type)
    : GameObject(pos, size, ObjectTag::BOSS_MASK)
    , m_owner(owner)
    , m_maskType(type) {
}

void BossMask::Initialize() {
    // タイプに応じたテクスチャを設定
    switch (m_maskType) {
    case MaskType::KATANA:
        m_textureId = TextureManager::Instance().Get(TexID::BOSS_MASK_KATANA);
        break;
    case MaskType::KANABO:
        m_textureId = TextureManager::Instance().Get(TexID::BOSS_MASK_KANABO);
        break;
    case MaskType::BOW:
        m_textureId = TextureManager::Instance().Get(TexID::BOSS_MASK_BOW);
        break;
    case MaskType::SPEAR:
        m_textureId = TextureManager::Instance().Get(TexID::BOSS_MASK_SPEAR);
        break;
    }

    m_isDestroyed = false;

    GetColliders().clear();
    AddCollider(GetPosition(), GetSize(), ColliderType::BODY);
    // 既に登録されていたら削除してから追加
    CollisionManager::GetInstance().RemoveObject(this);
    CollisionManager::GetInstance().AddObject(this);
}

void BossMask::Finalize() {
    CollisionManager::GetInstance().RemoveObject(this);
}

void BossMask::Update() {
    if (m_isDestroyed) return;

    float dt = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());

    // フェードイン処理
    if (m_isFadingIn) {
        m_fadeTimer += dt;
        m_alpha = m_fadeTimer / FADE_IN_TIME;
        if (m_alpha >= 1.0f) {
            m_alpha = 1.0f;
            m_isFadingIn = false;
        }
    }

    // フェードアウト処理
    if (m_isFadingOut) {
        m_fadeTimer += dt;
        m_alpha = 1.0f - (m_fadeTimer / FADE_OUT_TIME);
        if (m_alpha <= 0.0f) {
            m_alpha = 0.0f;
            m_isFadingOut = false;
            SetActive(false);  // フェードアウト完了で非アクティブ
        }
    }

    if (!GetColliders().empty()) {
        GetColliders()[0].SetColliderPos(GetPosition());
    }
}

void BossMask::Draw() {
    if (m_isDestroyed || !IsActive()) return;

    if (m_textureId >= 0) {
        Sprite_ScrollDraw(m_textureId, GetPosition().x, GetPosition().y,
            GetSize().x, GetSize().y, {1.0f, 1.0f, 1.0f, m_alpha});
    }
}

void BossMask::OnCollision(const GameObject* otherObject, const Collider* myCollider,
    const Collider* otherCollider, COLLISION_VEC hitVec) {
    if (m_isDestroyed || m_pendingDestroy || !otherObject || !otherCollider) return;

    // フェード中は衝突無効
    if (m_isFadingIn || m_isFadingOut) return;

    // プレイヤーの攻撃で破壊
    bool shouldDestroy = false;

    if (otherCollider->GetType() == ColliderType::ATTACK) {
        shouldDestroy = true;
    }
    // 弓の弾チェック
    if (otherObject->GetTag() == ObjectTag::BULLET) {
        shouldDestroy = true;
    }

    if (shouldDestroy && m_owner) {
		Audio_PlaySE(AudioID::SE_MASK_BREAK, 0.3f);
        m_pendingDestroy = true;  // 即座に破壊せずフラグを立てる
        m_owner->RequestMaskDestroy(this);  // Bossに破壊リクエスト
    }
}

void BossMask::Destroy() {
    // この関数は直接呼ばないようにする
    if (m_isDestroyed || m_pendingDestroy) return;

    if (m_owner) {
        m_pendingDestroy = true;
        m_owner->RequestMaskDestroy(this);
    }
}

void BossMask::Reset() {
    m_isDestroyed = false;
    m_pendingDestroy = false;  // ← 追加
}


void BossMask::Activate() {
    SetActive(true);
    m_isFadingIn = true;
    m_isFadingOut = false;
    m_fadeTimer = 0.0f;
    m_alpha = 0.0f;
}

void BossMask::Deactivate() {
    // 破壊された場合は即座に非アクティブ（フェードアウトなし）
    if (m_isDestroyed || m_pendingDestroy) {
        SetActive(false);
        m_alpha = 0.0f;
        return;
    }

    // 通常の非アクティブ化はフェードアウト
    m_isFadingOut = true;
    m_isFadingIn = false;
    m_fadeTimer = 0.0f;
}

//==============================================================================
// BossHand - ボスの手
//==============================================================================
BossHand::BossHand(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size, Boss* owner, HandSide side)
    : GameObject(pos, size, ObjectTag::BOSS)
    , m_owner(owner)
    , m_side(side)
    , m_basePos(pos) {
}

void BossHand::RandomizeParameters() {
    // 速度をランダム化（0.8 ~ 1.8）
    m_moveSpeedX = 0.8f + static_cast<float>(rand() % 100) / 100.0f;
    m_moveSpeedY = m_moveSpeedX * (1.5f + static_cast<float>(rand() % 100) / 100.0f);  // Xの1.5 ~ 2.5倍

    // 範囲をランダム化
    m_moveRangeX = 100.0f + static_cast<float>(rand() % 100);  // 100 ~ 200
    m_moveRangeY = 50.0f + static_cast<float>(rand() % 60);    // 50 ~ 110

    // 位相オフセットをランダム化（0 ~ 2π）
    m_phaseOffsetX = static_cast<float>(rand() % 628) / 100.0f;
    m_phaseOffsetY = static_cast<float>(rand() % 628) / 100.0f;

    // タイマーもランダムな位置から開始
    m_moveTimer = static_cast<float>(rand() % 628) / 100.0f;
}

void BossHand::Initialize() {
    if (m_side == HandSide::RIGHT) {
        m_textureId = TextureManager::Instance().Get(TexID::BOSS_RIGHT_HAND);
    } else {
        m_textureId = TextureManager::Instance().Get(TexID::BOSS_LEFT_HAND);
    }

    m_barrierTextureId = TextureManager::Instance().Get(TexID::BOSS_BARRIER);

    // バリアアニメーション作成
    if (!m_barrierAnim) {
        m_barrierAnim = new AnimPattern(
            m_barrierTextureId,
            BARRIER_ANIM_TOTAL,
            BARRIER_ANIM_COLS,
            0.05f,
            { 0, 0 },
            { static_cast<uint32_t>(BARRIER_FRAME_WIDTH), static_cast<uint32_t>(BARRIER_FRAME_HEIGHT) },
            false,
            false
        );
    }
    if (!m_barrierAnimPlayer) {
        m_barrierAnimPlayer = new AnimPatternPlayer(m_barrierAnim);
    }
    m_isBarrierAnimPlaying = false;
    m_hasBarrier = false;

    RandomizeParameters();

    m_hasHitPlayer = false;
    m_hitCooldown = 0.0f;

    // 手のアニメーション初期化
    if (m_side == HandSide::RIGHT) {
        m_startAnimTexId = TextureManager::Instance().Get(TexID::BOSS_RIGHT_HAND_START_ANIM);
        m_idleAnimTexIds[0] = TextureManager::Instance().Get(TexID::BOSS_RIGHT_HAND_IDLE_ANIM1);
        m_idleAnimTexIds[1] = TextureManager::Instance().Get(TexID::BOSS_RIGHT_HAND_IDLE_ANIM2);
        m_idleAnimTexIds[2] = TextureManager::Instance().Get(TexID::BOSS_RIGHT_HAND_IDLE_ANIM3);
    } else {
        m_startAnimTexId = TextureManager::Instance().Get(TexID::BOSS_LEFT_HAND_START_ANIM);
        m_idleAnimTexIds[0] = TextureManager::Instance().Get(TexID::BOSS_LEFT_HAND_IDLE_ANIM1);
        m_idleAnimTexIds[1] = TextureManager::Instance().Get(TexID::BOSS_LEFT_HAND_IDLE_ANIM2);
        m_idleAnimTexIds[2] = TextureManager::Instance().Get(TexID::BOSS_LEFT_HAND_IDLE_ANIM3);
    }

    // スタートアニメ（1回再生、ループなし）
    if (!m_startAnim) {
        m_startAnim = new AnimPattern(
            m_startAnimTexId,
            HAND_ANIM_TOTAL - 2,
            HAND_ANIM_COLS,
            0.05f,
            { 0, 0 },
            { static_cast<uint32_t>(HAND_FRAME_WIDTH), static_cast<uint32_t>(HAND_FRAME_HEIGHT) },
            false,
            false   // ループしない
        );
    }
    if (!m_startAnimPlayer) {
        m_startAnimPlayer = new AnimPatternPlayer(m_startAnim);
    }

    // アイドルアニメ（各1回再生、終わったら次に切り替え）
    for (int i = 0; i < 3; ++i) {
        if (!m_idleAnim[i]) {
            m_idleAnim[i] = new AnimPattern(
                m_idleAnimTexIds[i],
                HAND_ANIM_TOTAL,
                HAND_ANIM_COLS,
                0.05f,
                { 0, 0 },
                { static_cast<uint32_t>(HAND_FRAME_WIDTH), static_cast<uint32_t>(HAND_FRAME_HEIGHT) },
                false,
                false
            );
        }
    }
    m_currentIdleIndex = 0;
    if (!m_idleAnimPlayer) {
        m_idleAnimPlayer = new AnimPatternPlayer(m_idleAnim[0]);
    }

    m_isStartAnimPlaying = false;
    m_startAnimFinished = false;

    // ダウンアニメーション初期化
    if (m_side == HandSide::RIGHT) {
        m_downAnimTexIds[0] = TextureManager::Instance().Get(TexID::BOSS_RIGHT_HAND_DOWN_ANIM1);
        m_downAnimTexIds[1] = TextureManager::Instance().Get(TexID::BOSS_RIGHT_HAND_DOWN_ANIM2);
        m_downAnimTexIds[2] = TextureManager::Instance().Get(TexID::BOSS_RIGHT_HAND_DOWN_ANIM3);
    } else {
        m_downAnimTexIds[0] = TextureManager::Instance().Get(TexID::BOSS_LEFT_HAND_DOWN_ANIM1);
        m_downAnimTexIds[1] = TextureManager::Instance().Get(TexID::BOSS_LEFT_HAND_DOWN_ANIM2);
        m_downAnimTexIds[2] = TextureManager::Instance().Get(TexID::BOSS_LEFT_HAND_DOWN_ANIM3);
    }

    for (int i = 0; i < 3; ++i) {
        if (!m_downAnim[i]) {
            m_downAnim[i] = new AnimPattern(
                m_downAnimTexIds[i],
                HAND_ANIM_TOTAL,
                HAND_ANIM_COLS,
                0.05f,
                { 0, 0 },
                { static_cast<uint32_t>(HAND_FRAME_WIDTH), static_cast<uint32_t>(HAND_FRAME_HEIGHT) },
                false,
                false
            );
        }
    }
    m_currentDownIndex = 0;
    if (!m_downAnimPlayer) {
        m_downAnimPlayer = new AnimPatternPlayer(m_downAnim[0]);
    }

    // コライダー設定
    GetColliders().clear();
    AddCollider(GetPosition(), GetSize(), ColliderType::BODY);
    CollisionManager::GetInstance().AddObject(this);
}


void BossHand::Finalize() {
    CollisionManager::GetInstance().RemoveObject(this);

    if (m_startAnimPlayer) { delete m_startAnimPlayer; m_startAnimPlayer = nullptr; }
    if (m_startAnim) { delete m_startAnim; m_startAnim = nullptr; }
    if (m_idleAnimPlayer) { delete m_idleAnimPlayer; m_idleAnimPlayer = nullptr; }
    for (int i = 0; i < 3; ++i) {
        if (m_idleAnim[i]) { delete m_idleAnim[i]; m_idleAnim[i] = nullptr; }
    }

    if (m_barrierAnimPlayer) {
        delete m_barrierAnimPlayer;
        m_barrierAnimPlayer = nullptr;
    }
    if (m_barrierAnim) {
        delete m_barrierAnim;
        m_barrierAnim = nullptr;
    }

    // ダウンアニメ解放
    if (m_downAnimPlayer) { delete m_downAnimPlayer; m_downAnimPlayer = nullptr; }
    for (int i = 0; i < 3; ++i) {
        if (m_downAnim[i]) { delete m_downAnim[i]; m_downAnim[i] = nullptr; }
    }
}

void BossHand::Update() {
    if (!IsActive()) return;

    float dt = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());

    // リセット中のスムーズ移動処理
    if (m_isResetting) {
        m_resetTimer += dt;
        m_moveTimer += dt;  // 浮遊タイマーも進める

        // リセット完了時の目標位置を浮遊位置として計算
        float targetOffsetX = sinf(m_moveTimer * m_moveSpeedX + m_phaseOffsetX) * m_moveRangeX;
        float targetOffsetY = sinf(m_moveTimer * m_moveSpeedY + m_phaseOffsetY) * m_moveRangeY;
        if (m_side == HandSide::RIGHT) {
            targetOffsetX = -targetOffsetX;
        }
        DirectX::XMFLOAT2 targetPos = {
            m_basePos.x + targetOffsetX,
            m_basePos.y + targetOffsetY
        };

        float t = m_resetTimer / RESET_MOVE_TIME;
        if (t >= 1.0f) {
            t = 1.0f;
            m_isResetting = false;
            SetPosition(targetPos);  // 浮遊位置に設定
        } else {
            // イーズアウト補間（開始位置から現在の浮遊位置へ）
            float easeT = 1.0f - (1.0f - t) * (1.0f - t);
            DirectX::XMFLOAT2 newPos = {
                m_resetStartPos.x + (targetPos.x - m_resetStartPos.x) * easeT,
                m_resetStartPos.y + (targetPos.y - m_resetStartPos.y) * easeT
            };
            SetPosition(newPos);
        }

        // コライダー位置更新
        if (!GetColliders().empty()) {
            GetColliders()[0].SetColliderPos(GetPosition());
        }

        // アイドルアニメーション更新
        if (m_idleAnimPlayer) {
            m_idleAnimPlayer->Update();
            if (m_idleAnimPlayer->IsEnd()) {
                m_currentIdleIndex = (m_currentIdleIndex + 1) % 3;
                m_idleAnimPlayer->SetAnimPattern(m_idleAnim[m_currentIdleIndex]);
                m_idleAnimPlayer->Reset();
            }
        }

        return;
    }


    // ダウン状態の判定（オーナーのボスがスタン中かどうか）
    bool wasDown = m_isDown;
    m_isDown = false;
    if (m_owner && m_owner->GetCurrentState()) {
        const char* stateName = m_owner->GetCurrentState()->GetStateName();
        if (strcmp(stateName, "Stunned") == 0) {
            m_isDown = true;
        }
    }

    // ダウン状態に入った瞬間にアニメリセット
    if (m_isDown && !wasDown) {
        m_currentDownIndex = 0;
        if (m_downAnimPlayer && m_downAnim[0]) {
            m_downAnimPlayer->SetAnimPattern(m_downAnim[0]);
            m_downAnimPlayer->Reset();
        }
    }

    // ダウンアニメーション更新
    if (m_isDown && m_downAnimPlayer) {
        m_downAnimPlayer->Update();

        if (m_downAnimPlayer->IsEnd()) {
            m_currentDownIndex = (m_currentDownIndex + 1) % 3;
            m_downAnimPlayer->SetAnimPattern(m_downAnim[m_currentDownIndex]);
            m_downAnimPlayer->Reset();
        }
    }

    // ヒットクールダウン更新
    if (m_hitCooldown > 0.0f) {
        m_hitCooldown -= dt;
        if (m_hitCooldown <= 0.0f) {
            m_hitCooldown = 0.0f;
            m_hasHitPlayer = false;
        }
    }

    // バリアアニメーション更新
    if (m_isBarrierAnimPlaying && m_barrierAnimPlayer) {
        m_barrierAnimPlayer->Update();
        if (m_barrierAnimPlayer->IsEnd()) {
            m_isBarrierAnimPlaying = false;
        }
    }

    // 登場アニメーション更新
    if (m_isStartAnimPlaying && m_startAnimPlayer) {
        m_startAnimPlayer->Update();
        if (m_startAnimPlayer->IsEnd()) {
            m_isStartAnimPlaying = false;
            m_startAnimFinished = true;
        }
        // 登場アニメ中は移動しない
        if (!GetColliders().empty()) {
            GetColliders()[0].SetColliderPos(GetPosition());
        }
        return;
    }

    // アイドルアニメーション更新（1周完了で次へ切り替え）
    if (!m_isDown && m_idleAnimPlayer) {
        m_idleAnimPlayer->Update();

        if (m_idleAnimPlayer->IsEnd()) {
            m_currentIdleIndex = (m_currentIdleIndex + 1) % 3;
            m_idleAnimPlayer->SetAnimPattern(m_idleAnim[m_currentIdleIndex]);
            m_idleAnimPlayer->Reset();
        }
    }

    m_moveTimer += dt;

    if (m_isRaging) {
        m_rageTimer += dt;

        DirectX::XMFLOAT2 pos = GetPosition();
        pos.x += m_rageVelocity.x * dt;
        pos.y += m_rageVelocity.y * dt;

        bool hitWall = false;

        if (pos.x <= m_stageLeft) {
            pos.x = m_stageLeft;
            m_rageVelocity.x = fabsf(m_rageVelocity.x);
            hitWall = true;
        }
        if (pos.x + GetSize().x >= m_stageRight) {
            pos.x = m_stageRight - GetSize().x;
            m_rageVelocity.x = -fabsf(m_rageVelocity.x);
            hitWall = true;
        }
        if (pos.y <= m_stageTop) {
            pos.y = m_stageTop;
            m_rageVelocity.y = fabsf(m_rageVelocity.y);
            hitWall = true;
        }
        if (pos.y + GetSize().y >= m_stageBottom) {
            pos.y = m_stageBottom - GetSize().y;
            m_rageVelocity.y = -fabsf(m_rageVelocity.y);
            hitWall = true;
        }

        if (hitWall) {
            Camera* camera = GameManager::Instance().GetCamera();
            if (camera) {
                camera->StartShake(WALL_HIT_SHAKE_DURATION, WALL_HIT_SHAKE_POWER);
            }

            float randomOffset = (static_cast<float>(rand() % 200) - 100.0f) / 100.0f * 0.3f;
            float currentAngle = atan2f(m_rageVelocity.y, m_rageVelocity.x);
            currentAngle += randomOffset;
            float speed = sqrtf(m_rageVelocity.x * m_rageVelocity.x +
                m_rageVelocity.y * m_rageVelocity.y);
            m_rageVelocity.x = cosf(currentAngle) * speed;
            m_rageVelocity.y = sinf(currentAngle) * speed;

            Audio_PlaySEMulti(AudioID::SE_BOSS_WALL_HIT, 0.2f);
        }

        SetPosition(pos);
    } else {
        // 通常の浮遊移動
        float offsetX = sinf(m_moveTimer * m_moveSpeedX + m_phaseOffsetX) * m_moveRangeX;
        float offsetY = sinf(m_moveTimer * m_moveSpeedY + m_phaseOffsetY) * m_moveRangeY;

        if (m_side == HandSide::RIGHT) {
            offsetX = -offsetX;
        }

        DirectX::XMFLOAT2 newPos = {
            m_basePos.x + offsetX,
            m_basePos.y + offsetY
        };

        SetPosition(newPos);
    }

    // コライダー位置更新
    if (!GetColliders().empty()) {
        GetColliders()[0].SetColliderPos(GetPosition());
    }
}


void BossHand::Draw() {
    if (!IsActive()) return;

    // 出現アニメ再生中
    if (m_isStartAnimPlaying && m_startAnimPlayer) {
        int pattern = m_startAnimPlayer->GetPattern();
        int col = pattern % HAND_ANIM_COLS;
        int row = pattern / HAND_ANIM_COLS;

        float uvX = col * HAND_FRAME_WIDTH;
        float uvY = row * HAND_FRAME_HEIGHT;

        if (m_startAnimTexId >= 0) {
            Sprite_ScrollDraw(m_startAnimTexId,
                GetPosition().x, GetPosition().y,
                GetSize().x, GetSize().y,
                uvX, uvY,
                HAND_FRAME_WIDTH, HAND_FRAME_HEIGHT,
                0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    // ダウン中はダウンアニメ
    else if (m_isDown && m_downAnimPlayer) {
        int pattern = m_downAnimPlayer->GetPattern();
        int col = pattern % HAND_ANIM_COLS;
        int row = pattern / HAND_ANIM_COLS;

        float uvX = col * HAND_FRAME_WIDTH;
        float uvY = row * HAND_FRAME_HEIGHT;

        int texId = m_downAnimTexIds[m_currentDownIndex];
        if (texId >= 0) {
            Sprite_ScrollDraw(texId,
                GetPosition().x, GetPosition().y,
                GetSize().x, GetSize().y,
                uvX, uvY,
                HAND_FRAME_WIDTH, HAND_FRAME_HEIGHT,
                0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    // アイドルアニメ（startが終わった後 or startが再生されてない場合）
    else if (m_idleAnimPlayer) {
        int pattern = m_idleAnimPlayer->GetPattern();
        int col = pattern % HAND_ANIM_COLS;
        int row = pattern / HAND_ANIM_COLS;

        float uvX = col * HAND_FRAME_WIDTH;
        float uvY = row * HAND_FRAME_HEIGHT;

        int texId = m_idleAnimTexIds[m_currentIdleIndex];
        if (texId >= 0) {
            Sprite_ScrollDraw(texId,
                GetPosition().x, GetPosition().y,
                GetSize().x, GetSize().y,
                uvX, uvY,
                HAND_FRAME_WIDTH, HAND_FRAME_HEIGHT,
                0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }
    // フォールバック
    else if (m_textureId >= 0) {
        Sprite_ScrollDraw(m_textureId,
            GetPosition().x, GetPosition().y,
            GetSize().x, GetSize().y);
    }

    // バリア描画
    if (m_barrierTextureId >= 0) {
        float drawW = GetSize().x + 32.0f;
        float drawH = GetSize().y + 32.0f;
        float drawX = GetPosition().x - 16.0f;
        float drawY = GetPosition().y - 16.0f;

        if (m_isBarrierAnimPlaying && m_barrierAnimPlayer) {
            // 破壊アニメーション再生中
            int pattern = m_barrierAnimPlayer->GetPattern();
            int col = pattern % BARRIER_ANIM_COLS;
            int row = pattern / BARRIER_ANIM_COLS;

            float uvX = col * BARRIER_FRAME_WIDTH;
            float uvY = row * BARRIER_FRAME_HEIGHT;

            Sprite_ScrollDraw(m_barrierTextureId,
                drawX, drawY,
                drawW, drawH,
                uvX, uvY,
                BARRIER_FRAME_WIDTH, BARRIER_FRAME_HEIGHT,
                0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        } else if (m_hasBarrier) {
            // バリア表示中（最初のフレーム）
            Sprite_ScrollDraw(m_barrierTextureId,
                drawX, drawY,
                drawW, drawH,
                0.0f, 0.0f,
                BARRIER_FRAME_WIDTH, BARRIER_FRAME_HEIGHT,
                0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }
}

void BossHand::OnCollision(const GameObject* otherObject, const Collider* myCollider,
    const Collider* otherCollider, COLLISION_VEC hitVec) {
    if (!otherObject || !otherCollider) return;

    // ボスが死んでたらダメージなし
    if (m_owner && m_owner->IsDead()) return;

    // ダウン中またはチャージ中（マスク出現中）はプレイヤーにダメージを与えない
    bool isCharging = false;
    if (m_owner && m_owner->GetCurrentState()) {
        const char* stateName = m_owner->GetCurrentState()->GetStateName();
        if (strcmp(stateName, "Charge") == 0) {
            isCharging = true;
        }
    }

    // プレイヤーの攻撃判定
    bool isPlayerAttack = (otherCollider->GetType() == ColliderType::ATTACK);
    bool isBullet = (otherObject->GetTag() == ObjectTag::BULLET);

    if (isPlayerAttack || isBullet) {
        // バリア中はダメージ無効
        if (m_hasBarrier) {
            Audio_PlaySEMulti(AudioID::SE_BARRIER_HIT);
            return;
        }

        // ボスにダメージを与える
        if (m_owner && !m_owner->IsInvincible()) {
            IBossState* state = m_owner->GetCurrentState();
            if (state && state->CanTakeDamage()) {
                int damage = m_owner->CalcDamageFromPlayer();
                state->OnDamage(m_owner, damage);
                m_owner->SetInvincible(0.2f);
            }
        }
    }

    // ダウン中またはチャージ中はプレイヤーにダメージを与えない
    if (m_isDown || isCharging) return;

    // プレイヤーへの接触ダメージ
    if (m_hasHitPlayer) return;

    if (otherObject->GetTag() == ObjectTag::PLAYER &&
        otherCollider->GetType() == ColliderType::BODY) {

        Player* player = dynamic_cast<Player*>(const_cast<GameObject*>(otherObject));
        if (player) {
            player->BossAttacksHit();

            float knockDir = 0.0f;
            if (m_isRaging) {
                knockDir = (m_rageVelocity.x >= 0.0f) ? 1.0f : -1.0f;
            } else {
                float dx = player->GetPosition().x - GetPosition().x;
                knockDir = (dx >= 0.0f) ? 1.0f : -1.0f;
            }
            player->ApplyImpulseX(knockDir * 500.0f);

            m_hasHitPlayer = true;
            m_hitCooldown = HIT_COOLDOWN_TIME;
        }
    }
}

void BossHand::Reset() {
    RandomizeParameters();

    SetPosition(m_basePos);
    SetActive(true);

    m_hasHitPlayer = false;
    m_hitCooldown = 0.0f;
    m_isRaging = false;
    m_rageVelocity = { 0.0f, 0.0f };
    m_hasBarrier = false;
    m_isBarrierAnimPlaying = false;
    if (m_barrierAnimPlayer) {
        m_barrierAnimPlayer->Reset();
    }

    // スタートアニメをスキップ
    m_skipStartAnim = true;
    m_isStartAnimPlaying = false;
    m_startAnimFinished = true;

    for (auto& col : GetColliders()) {
        col.SetActive(true);
    }
}


void BossHand::StartRaging(float stageLeft, float stageRight, float stageTop, float stageBottom) {
    m_isRaging = true;
    m_rageTimer = 0.0f;
    m_wallHitTimer = 0.0f;

    // 壁の内側（外周ブロック64.0fの内側）
    m_stageLeft = stageLeft + 64.0f;
    m_stageRight = stageRight - 64.0f;
    m_stageTop = stageTop + 64.0f;
    m_stageBottom = stageBottom - 64.0f;

    // ランダムな方向に発射
    float angle = static_cast<float>(rand() % 628) / 100.0f;

    // 真横・真縦すぎると単調になるので少しずらす
    float minAngle = 0.3f;
    float cosA = cosf(angle);
    float sinA = sinf(angle);
    if (fabsf(cosA) < minAngle) cosA = (cosA >= 0.0f) ? minAngle : -minAngle;
    if (fabsf(sinA) < minAngle) sinA = (sinA >= 0.0f) ? minAngle : -minAngle;

    // 正規化して速度適用
    float len = sqrtf(cosA * cosA + sinA * sinA);
    m_rageVelocity.x = (cosA / len) * m_rageSpeed;
    m_rageVelocity.y = (sinA / len) * m_rageSpeed;

    // 左右の手で初期方向を変える
    if (m_side == HandSide::RIGHT) {
        m_rageVelocity.x = fabsf(m_rageVelocity.x);   // 右手は右方向スタート
    } else {
        m_rageVelocity.x = -fabsf(m_rageVelocity.x);  // 左手は左方向スタート
    }
}

void BossHand::StopRaging() {
    m_isRaging = false;
    m_rageVelocity = { 0.0f, 0.0f };
}

void BossHand::PlayBarrierBreakAnim() {
    if (m_isBarrierAnimPlaying) return;

    if (m_barrierAnimPlayer) {
        m_barrierAnimPlayer->Reset();
        m_isBarrierAnimPlaying = true;
    }
    m_hasBarrier = false;
}

void BossHand::StartAppearAnim() {
    // スキップフラグが立っていたらアニメを再生しない
    if (m_skipStartAnim) {
        m_skipStartAnim = false;  // フラグをリセット
        m_isStartAnimPlaying = false;
        m_startAnimFinished = true;
        return;
    }

    m_isStartAnimPlaying = true;
    m_startAnimFinished = false;
    if (m_startAnimPlayer) {
        m_startAnimPlayer->Reset();
    }
}

void BossHand::StopBarrierAnim() {
    m_isBarrierAnimPlaying = false;
    if (m_barrierAnimPlayer) {
        m_barrierAnimPlayer->Reset();
    }
}

void BossHand::ResetSmooth() {
    RandomizeParameters();

    // 現在位置を記録してスムーズ移動開始
    m_resetStartPos = GetPosition();
    m_resetTimer = 0.0f;
    m_isResetting = true;

    SetActive(true);

    m_hasHitPlayer = false;
    m_hitCooldown = 0.0f;
    m_isRaging = false;
    m_rageVelocity = { 0.0f, 0.0f };
    m_hasBarrier = false;
    m_isBarrierAnimPlaying = false;
    if (m_barrierAnimPlayer) {
        m_barrierAnimPlayer->Reset();
    }

    // スタートアニメをスキップ
    m_skipStartAnim = true;
    m_isStartAnimPlaying = false;
    m_startAnimFinished = true;  // 既に終わった扱いにする

    for (auto& col : GetColliders()) {
        col.SetActive(true);
    }
}
