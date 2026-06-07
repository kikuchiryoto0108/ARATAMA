/*********************************************************************
 * \file   game_boss_state.cpp
 * \brief  ボスのステート管理
 *
 * \author Ryoto Kikuchi
 * \date   2026/2/14
 *********************************************************************/
#include "game_boss_state.h"
#include "game_boss.h"
#include "game_boss_attacks.h"
#include "game_stage.h"
#include "game_gimmick.h"
#include "game_player.h"

// これ全部撃破演出
#include "camera.h"
#include "game_manager.h"
#include "fade.h"
#include "scene.h"
#include "texture.h"
#include "sprite.h"
#include "Audio.h"
#include "game.h"
#include "input_manager.h"

#ifdef _DEBUG
#include <Windows.h>
#include <string>
#endif

//==============================================================================
// BossWaitState - 待機状態
//==============================================================================
void BossWaitState::Enter(Boss* boss) {
    boss->ShowBarrier(false);

    m_boss = boss;

    // 攻撃マネージャー無効化
    boss->GetNormalAttackManager()->SetActive(false);
    boss->GetSpecialAttackManager()->SetActive(false);

    m_wallsDestroyed = false;
    m_playerEnteredBossRoom = false;
    m_wallPositionX = GetWallPositionX();

#ifdef _DEBUG
    OutputDebugStringA("Boss State: Wait - Waiting for player to enter boss room...\n");
#endif
}

void BossWaitState::Update(Boss* boss, float deltaTime) {
    // イントロ動画再生中は何もしない
    if (boss->IsIntroPlaying()) return;

    if (!m_stage) {
        // 壁がない場合もイントロチェック
        if (!boss->HasIntroPlayed()) {
            boss->StartIntroMovie();
            return;
        }
        boss->ChangeState(new BossNormalState());
        return;
    }

    if (!HasAnyWalls()) {
        // イントロチェック
        if (!boss->HasIntroPlayed()) {
            boss->StartIntroMovie();
            return;
        }
        boss->ChangeState(new BossNormalState());
        return;
    }

    if (!m_wallsDestroyed) {
        if (AreAllWallsDestroyed()) {
            m_wallsDestroyed = true;
        }
        return;
    }

    if (!m_playerEnteredBossRoom) {
        if (HasPlayerPassedWall()) {
            m_playerEnteredBossRoom = true;
            SpawnBlocksAtWallPositions();

            // ブロック配置後、イントロ動画開始
            if (!boss->HasIntroPlayed()) {
                boss->StartIntroMovie();
                return;
            }
            boss->ChangeState(new BossNormalState());
        }
    }
    // イントロ再生が終わったらNormalStateへ
    else if (boss->HasIntroPlayed()) {
        boss->ChangeState(new BossNormalState());
    }
}


void BossWaitState::Draw(Boss* boss) {
}

void BossWaitState::Exit(Boss* boss) {
#ifdef _DEBUG
    OutputDebugStringA("Boss State: Wait -> Normal\n");
#endif
}

void BossWaitState::OnDamage(Boss* boss, int damage) {
#ifdef _DEBUG
    OutputDebugStringA("Boss: Damage BLOCKED (Wait state)!\n");
#endif
}

bool BossWaitState::AreAllWallsDestroyed() const {
    if (!m_stage) return true;

    const auto& gimmicks = m_stage->GetGimmicks();

    for (auto* gimmick : gimmicks) {
        MidBossWall* wall = dynamic_cast<MidBossWall*>(gimmick);
        if (wall && wall->IsActive()) {
            return false;
        }
    }

    return true;
}

float BossWaitState::GetWallPositionX() const {
    if (!m_stage) return 0.0f;

    const auto& gimmicks = m_stage->GetGimmicks();

    for (auto* gimmick : gimmicks) {
        MidBossWall* wall = dynamic_cast<MidBossWall*>(gimmick);
        if (wall) {
            // 最初に見つかった壁のX座標を返す
            return wall->GetPosition().x;
        }
    }

    return 0.0f;
}

bool BossWaitState::HasPlayerPassedWall() const {
    if (!m_stage || m_wallPositionX <= 0.0f) return false;

    Player* player = m_boss->GetPlayer();
    if (!player) return false;

    if (m_wallPositionX <= 0.0f) return false;

    float playerX = player->GetPosition().x;
    return playerX > m_wallPositionX + 128.0f;
}

void BossWaitState::SpawnBlocksAtWallPositions() {
    if (!m_stage) return;

    // 非アクティブになったMidBossWallの位置を取得してブロックを配置
    const auto& gimmicks = m_stage->GetGimmicks();

    for (auto* gimmick : gimmicks) {
        MidBossWall* wall = dynamic_cast<MidBossWall*>(gimmick);
        if (wall && !wall->IsActive()) {
            // 壁があった位置にブロックを追加
            DirectX::XMFLOAT2 pos = wall->GetPosition();
            m_stage->AddNormalBlock(pos, { 64.0f, 64.0f });

#ifdef _DEBUG
            OutputDebugStringA(("Spawned block at (" +
                std::to_string(pos.x) + ", " + std::to_string(pos.y) + ")\n").c_str());
#endif
        }
    }
}

bool BossWaitState::HasAnyWalls() const {
    if (!m_stage) return false;

    const auto& gimmicks = m_stage->GetGimmicks();
    for (auto* gimmick : gimmicks) {
        MidBossWall* wall = dynamic_cast<MidBossWall*>(gimmick);
        if (wall) {
            return true;
        }
    }
    return false;
}


//==============================================================================
// BossNormalState - 通常攻撃状態
//==============================================================================
void BossNormalState::Enter(Boss* boss) {
    m_stateTimer = 0.0f;
    m_damageAccumulated = 0;
    boss->ShowBarrier(false);

    // 手の登場アニメ開始
    if (boss->GetRightHand()) {
        boss->GetRightHand()->StartAppearAnim();
    }
    if (boss->GetLeftHand()) {
        boss->GetLeftHand()->StartAppearAnim();
    }

    // 攻撃マネージャー有効化
    boss->GetNormalAttackManager()->SetActive(true);

#ifdef _DEBUG
    OutputDebugStringA("Boss State: Normal\n");
#endif
}

void BossNormalState::Update(Boss* boss, float deltaTime) {
    m_stateTimer += deltaTime;

    // 攻撃更新
    boss->GetNormalAttackManager()->Update(deltaTime);

    // 一定時間経過 or 累積ダメージで必殺溜め状態
    if (m_stateTimer >= STATE_DURATION || m_damageAccumulated >= DAMAGE_THRESHOLD) {
        boss->ChangeState(new BossChargeState());
    }
}

void BossNormalState::Draw(Boss* boss) {
    boss->GetNormalAttackManager()->Draw();
}

void BossNormalState::Exit(Boss* boss) {
    boss->GetNormalAttackManager()->StopCurrentAttack();
}

void BossNormalState::OnDamage(Boss* boss, int damage) {
    m_damageAccumulated += damage;
    boss->TakeDamage(damage);

    // 撃破判定
    if (boss->IsDead()) {
        Player* player = boss->GetPlayer();
        Camera* camera = GameManager::Instance().GetCamera();
        boss->ChangeState(new BossDefeatedState(player, camera));
    }
}

//==============================================================================
// BossChargeState - 必殺溜め状態
//==============================================================================
void BossChargeState::Enter(Boss* boss) {
    m_stateTimer = 0.0f;
    boss->ShowBarrier(true);
    boss->SpawnMasks();

    // 手にバリアを張る
    if (boss->GetRightHand()) {
        boss->GetRightHand()->SetBarrier(true);
        boss->GetRightHand()->StopBarrierAnim();
    }
    if (boss->GetLeftHand()) {
        boss->GetLeftHand()->SetBarrier(true);
        boss->GetLeftHand()->StopBarrierAnim();
    }

#ifdef _DEBUG
    OutputDebugStringA("Boss State: Charge - BARRIER UP!\n");
#endif
}

void BossChargeState::Update(Boss* boss, float deltaTime) {
    m_stateTimer += deltaTime;

    // 全仮面壊れたらスタン状態へ
    if (boss->AreAllMasksDestroyed()) {
        boss->ChangeState(new BossStunnedState());
        return;
    }
    
    float chargeDuration = GetChargeDuration();

    // 時間切れで必殺技へ
    if (m_stateTimer >= chargeDuration) {
        Audio_PlaySE(AudioID::SE_SPECIAL_ATTACK_READY);
        boss->ChangeState(new BossSpecialState());
    }
}

void BossChargeState::Draw(Boss* boss) {
    // Charge中は攻撃描画なし（仮面はBoss側で描画）
}

void BossChargeState::Exit(Boss* boss) {
    boss->DestroyAllMasks();
}

void BossChargeState::OnDamage(Boss* boss, int damage) {
    // バリア中はダメージ無効
#ifdef _DEBUG
    OutputDebugStringA("Boss: Damage BLOCKED!\n");
#endif
}

float BossChargeState::GetChargeDuration() const {
    float duration = CHARGE_DURATION;
    if (GameManager::Instance().IsHardMode()) {
        duration *= 0.6f;
    }
    return duration;
}

//==============================================================================
// BossSpecialState - 必殺技状態
//==============================================================================
void BossSpecialState::Enter(Boss* boss) {
    m_stateTimer = 0.0f;
    m_hasAttacked = false;

    // 攻撃マネージャー有効化
    boss->GetSpecialAttackManager()->SetActive(false);

#ifdef _DEBUG
    OutputDebugStringA("Boss State: Special - ATTACK!\n");
#endif
}

void BossSpecialState::Update(Boss* boss, float deltaTime) {
    m_stateTimer += deltaTime;

    // 必殺発動（0.5秒後）
    if (!m_hasAttacked && m_stateTimer >= 0.5f) {
        if (boss->GetSpecialAttackManager()->GetAttackCount() > 0) {
            boss->GetSpecialAttackManager()->SetActive(true);
            boss->GetSpecialAttackManager()->StartRandomAttack();
        }

#ifdef _DEBUG
        OutputDebugStringA("Boss: SPECIAL ATTACK!\n");
#endif
        m_hasAttacked = true;
    }

    // 攻撃更新
    if (m_hasAttacked) {
        boss->GetSpecialAttackManager()->Update(deltaTime);
    }

    // 必殺技終了したら通常状態へ
    bool attackFinished = !boss->GetSpecialAttackManager()->IsAttacking();

    if (m_hasAttacked && attackFinished && m_stateTimer >= ATTACK_DURATION) {
        boss->ChangeState(new BossNormalState());
    }
}

void BossSpecialState::Draw(Boss* boss) {
    boss->GetSpecialAttackManager()->Draw();
}

void BossSpecialState::Exit(Boss* boss) {
    boss->GetSpecialAttackManager()->StopCurrentAttack();
    boss->GetSpecialAttackManager()->SetActive(false);
    boss->ShowBarrier(false);

    // ボス本体のバリア解除アニメーション再生
    boss->PlayBarrierBreakAnim();
    boss->ShowBarrier(false);
    Audio_PlaySEMulti(AudioID::SE_BARRIER_BREAK);

    // 手のバリアも解除
    if (boss->GetRightHand()) {
        boss->GetRightHand()->SetBarrier(false);
        boss->GetRightHand()->PlayBarrierBreakAnim();
        Audio_PlaySEMulti(AudioID::SE_BARRIER_BREAK);
    }
    if (boss->GetLeftHand()) {
        boss->GetLeftHand()->SetBarrier(false);
        boss->GetLeftHand()->PlayBarrierBreakAnim();
        Audio_PlaySEMulti(AudioID::SE_BARRIER_BREAK);
    }
}

void BossSpecialState::OnDamage(Boss* boss, int damage) {
    // 必殺中はダメージ無効
}

//==============================================================================
// BossStunnedState - スタン状態
//==============================================================================
void BossStunnedState::Enter(Boss* boss) {
    m_stateTimer = 0.0f;
    boss->ShowBarrier(false);

    boss->PlayBarrierBreakAnim();
    boss->ShowBarrier(false);
    Audio_PlaySEMulti(AudioID::SE_BARRIER_BREAK);

    // スタン時もボスと同時に手のバリアを壊す
    if (boss->GetRightHand()) {
        boss->GetRightHand()->SetBarrier(false);
        boss->GetRightHand()->PlayBarrierBreakAnim();
        Audio_PlaySEMulti(AudioID::SE_BARRIER_BREAK);
    }
    if (boss->GetLeftHand()) {
        boss->GetLeftHand()->SetBarrier(false);
        boss->GetLeftHand()->PlayBarrierBreakAnim();
        Audio_PlaySEMulti(AudioID::SE_BARRIER_BREAK);
    }

#ifdef _DEBUG
    OutputDebugStringA("Boss State: Stunned - VULNERABLE!\n");
#endif
}

void BossStunnedState::Update(Boss* boss, float deltaTime) {
    m_stateTimer += deltaTime;

    // スタン終了 → 通常へ戻る
    if (m_stateTimer >= STUN_DURATION) {
        boss->ChangeState(new BossNormalState());
    }
}

void BossStunnedState::Draw(Boss* boss) {
    // スタン中は攻撃描画なし
}

void BossStunnedState::Exit(Boss* boss) {
}

void BossStunnedState::OnDamage(Boss* boss, int damage) {
    // スタン中は2倍ダメージ
    int actualDamage = static_cast<int>(damage * DAMAGE_MULTIPLIER);
    boss->TakeDamage(actualDamage);

    // 撃破判定
    if (boss->IsDead()) {
        Player* player = boss->GetPlayer();
        Camera* camera = GameManager::Instance().GetCamera();
        boss->ChangeState(new BossDefeatedState(player, camera));
    }
}

//==============================================================================
// BossDefeatedState - 撃破演出
//==============================================================================

void BossDefeatedState::Enter(Boss* boss) {
    m_phase = Phase::STAGGER;
    m_phaseTimer = 0.0f;
    m_totalTimer = 0.0f;
    m_flashAlpha = 0.0f;
    m_playerHidden = false;
    m_soulStarted = false;

    m_whiteTexId = TextureManager::Instance().Get(TexID::WHITE);
    m_doorRightTexId = TextureManager::Instance().Get(TexID::BOSS_DOOR_RIGHT);
    m_doorLeftTexId = TextureManager::Instance().Get(TexID::BOSS_DOOR_LEFT);
    m_doorProgress = 0.0f;

    // 攻撃全停止
    boss->GetNormalAttackManager()->StopCurrentAttack();
    boss->GetNormalAttackManager()->SetActive(false);
    boss->GetSpecialAttackManager()->StopCurrentAttack();
    boss->GetSpecialAttackManager()->SetActive(false);

    boss->ShowBarrier(false);

    // HPバー非表示
    boss->SetHideHpBar(true);

    // ボスのコライダー無効化
    for (auto& col : boss->GetColliders()) {
        col.SetActive(false);
    }

    if (m_player) {
        m_player->ForceStopAttack();
    }

    // プレイヤー開始位置記録
    if (m_player) {
        m_playerStartPos = m_player->GetPosition();
    }

    // ターゲット＝ボス中心
    m_targetPos = {
        boss->GetPosition().x + boss->GetSize().x * 0.5f - 32.0f,
        boss->GetPosition().y + boss->GetSize().y * 0.5f - 32.0f
    };

    TimeManager::GetInstance().SetTimeScale(1.0f);

    if (m_camera) {
        m_camera->StartShake(STAGGER_TIME, 15.0f);
    }

    Audio_FadeOutBGM(2.0f);

#ifdef _DEBUG
    OutputDebugStringA("Boss State: Defeated - Starting defeat sequence\n");
#endif
}


void BossDefeatedState::Update(Boss* boss, float deltaTime) {
    // 演出は生の時間で進行
    float dt = static_cast<float>(TimeManager::GetInstance().GetElapsedTime());
    m_phaseTimer += dt;
    m_totalTimer += dt;

    switch (m_phase) {
    case Phase::STAGGER:      UpdateStagger(boss, dt);      break;
    case Phase::PLAYER_MOVE:  UpdatePlayerMove(boss, dt);   break;
    case Phase::POSSESS_FLASH:UpdatePossessFlash(boss, dt); break;
    case Phase::SCREAM:       UpdateScream(boss, dt);       break;
    case Phase::DOOR_CLOSE:   UpdateDoorClose(boss, dt);    break;
    case Phase::FADE_OUT:     UpdateFadeOut(boss, dt);      break;
    case Phase::DONE:         break;
    }
}

void BossDefeatedState::Draw(Boss* boss) {
    // 白フラッシュ
    if (m_flashAlpha > 0.0f && m_whiteTexId >= 0) {
        Sprite_EnableCameraZoom(false);
        Sprite_Draw(m_whiteTexId,
            0.0f, 0.0f,
            SCREEN_WIDTH, SCREEN_HEIGHT,
            0.0f, 0.0f, 1.0f, 1.0f,
            0.0f,
            { 1.0f, 1.0f, 1.0f, m_flashAlpha });
        Sprite_EnableCameraZoom(true);
    }

    // 扉演出描画
    if (m_doorProgress > 0.0f) {
        Sprite_EnableCameraZoom(false);

        // 左扉：画面左外から中央へスライドイン
        float leftDoorX = -DOOR_HALF_WIDTH * (1.0f - m_doorProgress);
        if (m_doorLeftTexId >= 0) {
            Sprite_Draw(m_doorLeftTexId,
                leftDoorX, 0.0f,
                DOOR_HALF_WIDTH, DOOR_HEIGHT);
        }

        // 右扉：画面右外から中央へスライドイン
        float rightDoorX = SCREEN_WIDTH - DOOR_HALF_WIDTH * m_doorProgress;
        if (m_doorRightTexId >= 0) {
            Sprite_Draw(m_doorRightTexId,
                rightDoorX, 0.0f,
                DOOR_HALF_WIDTH, DOOR_HEIGHT);
        }

        Sprite_EnableCameraZoom(true);
    }
}

void BossDefeatedState::Exit(Boss* boss) {
#ifdef _DEBUG
    OutputDebugStringA("Boss State: Defeated -> Exit\n");
#endif
}

void BossDefeatedState::OnDamage(Boss* boss, int damage) {
    // 演出中ダメージ無効
}

//==============================================================================
// 各フェーズ
//==============================================================================

void BossDefeatedState::UpdateStagger(Boss* boss, float dt) {
    // スロー演出
    TimeManager::GetInstance().SetTimeScale(0.3f);

    if (m_phaseTimer >= STAGGER_TIME) {
        m_phase = Phase::PLAYER_MOVE;
        m_phaseTimer = 0.0f;

        if (m_player) {
            m_playerStartPos = m_player->GetPosition();
        }

        TimeManager::GetInstance().SetTimeScale(1.0f);

        // カメラをボス中心へ
        if (m_camera) {
            float cx = boss->GetPosition().x + boss->GetSize().x * 0.5f;
            float cy = boss->GetPosition().y + boss->GetSize().y * 0.5f;
            m_camera->SetPosition(cx, cy);
        }

#ifdef _DEBUG
        OutputDebugStringA("BossDefeated: STAGGER -> PLAYER_MOVE\n");
#endif
    }
}

void BossDefeatedState::UpdatePlayerMove(Boss* boss, float dt) {
    if (!m_soulStarted && m_player) {
        m_soulStarted = true;

        // プレイヤーのオーラを消す
        Aura* aura = m_player->GetAura();
        if (aura) {
            aura->SetActive(false);
        }

        DirectX::XMFLOAT2 playerCenter = {
            m_player->GetPosition().x + m_player->GetSize().x * 0.5f,
            m_player->GetPosition().y + m_player->GetSize().y * 0.5f
        };
        DirectX::XMFLOAT2 bossCenter = {
            boss->GetPosition().x + boss->GetSize().x * 0.5f,
            boss->GetPosition().y + boss->GetSize().y * 0.5f
        };

        Soul* soul = m_player->GetSoul();
        if (soul) {
            soul->Start(playerCenter, bossCenter, PLAYER_MOVE_TIME);
        }
    }

    // 魂を手動でUpdate
    Soul* soul = m_player ? m_player->GetSoul() : nullptr;
    if (soul) {
        soul->Update();
    }

    if (m_phaseTimer >= PLAYER_MOVE_TIME) {
        m_phase = Phase::POSSESS_FLASH;
        m_phaseTimer = 0.0f;

#ifdef _DEBUG
        OutputDebugStringA("BossDefeated: PLAYER_MOVE -> POSSESS_FLASH\n");
#endif
    }
}



void BossDefeatedState::UpdatePossessFlash(Boss* boss, float dt) {
    float progress = m_phaseTimer / POSSESS_FLASH_TIME;
    if (progress > 1.0f) progress = 1.0f;

    // フラッシュカーブ
    if (progress < 0.2f) {
        m_flashAlpha = progress / 0.2f;
    } else if (progress < 0.4f) {
        m_flashAlpha = 1.0f;
    } else {
        m_flashAlpha = 1.0f - (progress - 0.4f) / 0.6f;
    }

    // ピーク時にプレイヤー非表示
    if (progress >= 0.3f && !m_playerHidden) {
        m_playerHidden = true;
        if (m_player) {
            m_player->ForceHide();  // フラッシュでプレイヤーも魂もまとめて消える
        }
        Audio_PlaySE(AudioID::SE_HYOUI);

        // ボスのオーラを青に変更
        Aura* bossAura = boss->GetAura();
        if (bossAura) {
            bossAura->SetColor({ 0.2f, 0.8f, 2.0f, 1.0f });  // 青
        }
    }

    if (progress >= 0.3f && progress < 0.5f && m_camera) {
        m_camera->StartShake(0.3f, 20.0f);
    }

    if (m_phaseTimer >= POSSESS_FLASH_TIME) {
        m_phase = Phase::SCREAM;
        m_phaseTimer = 0.0f;
        m_flashAlpha = 0.0f;

        Audio_PlaySE(AudioID::SE_BOSS_SCREAM);

        // SCREAMフェーズ開始と同時に手を暴走させる
        float stageW = 1920.0f;
        float stageH = 1088.0f;
        Stage* stage = GameManager::Instance().GetStage();
        if (stage) {
            stageW = stage->GetWidth() * 64.0f;
            stageH = stage->GetHeight() * 64.0f;
        }

        if (boss->GetRightHand()) {
            boss->GetRightHand()->StartRaging(0.0f, stageW, 0.0f, stageH);
        }
        if (boss->GetLeftHand()) {
            boss->GetLeftHand()->StartRaging(0.0f, stageW, 0.0f, stageH);
        }

#ifdef _DEBUG
        OutputDebugStringA("BossDefeated: POSSESS_FLASH -> SCREAM\n");
#endif
    }
}




void BossDefeatedState::UpdateScream(Boss* boss, float dt) {
    float progress = m_phaseTimer / SCREAM_TIME;

    InputManager::StartVibration(0.8f, 0.3f);

    // 断続シェイク（徐々に弱まる）
    if (m_camera && progress < 0.8f) {
        float shakePower = 30.0f * (1.0f - progress);
        if (static_cast<int>(m_phaseTimer * 10.0f) % 3 == 0) {
            m_camera->StartShake(0.2f, shakePower);
        }
    }

    if (m_phaseTimer >= SCREAM_TIME) {
        m_phase = Phase::DOOR_CLOSE;
        m_phaseTimer = 0.0f;
        m_doorProgress = 0.0f;

		Audio_PlaySE(AudioID::SE_DOOR_CLOSE);

#ifdef _DEBUG
        OutputDebugStringA("BossDefeated: SCREAM -> FADE_OUT\n");
#endif
    }
}

void BossDefeatedState::UpdateDoorClose(Boss* boss, float dt) {
    float progress = m_phaseTimer / DOOR_CLOSE_TIME;
    if (progress > 1.0f) progress = 1.0f;

    m_doorProgress = EaseInOut(progress);

    if (m_phaseTimer >= DOOR_CLOSE_TIME) {
        m_phase = Phase::FADE_OUT;
        m_phaseTimer = 0.0f;
        m_doorProgress = 1.0f;

        // 手の暴走を止める
        if (boss->GetRightHand()) {
            boss->GetRightHand()->StopRaging();
        }
        if (boss->GetLeftHand()) {
            boss->GetLeftHand()->StopRaging();
        }

        InputManager::StopVibration();

        DirectX::XMFLOAT4 black = { 0.0f, 0.0f, 0.0f, 1.0f };
        Fade_Start(FADE_OUT_TIME, false, black);

#ifdef _DEBUG
        OutputDebugStringA("BossDefeated: DOOR_CLOSE -> FADE_OUT\n");
#endif
    }
}

void BossDefeatedState::UpdateFadeOut(Boss* boss, float dt) {
    m_doorProgress = 1.0f;  // 扉は閉じたまま
    if (Fade_GetState() == FADE_STATE_FADE_OUT_FINISHED) {
        m_phase = Phase::DONE;

#ifdef _DEBUG
        OutputDebugStringA("BossDefeated: FADE_OUT -> DONE\n");
#endif
    }
}

float BossDefeatedState::EaseInOut(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}
