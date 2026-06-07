/*********************************************************************
 * \file   game_boss_attacks.cpp
 * \brief  ボスの各種攻撃
 *
 * \author Ryoto Kikuchi
 * \date   2026/2/14
 *********************************************************************/
#include "game_boss_attacks.h"
#include "game_boss.h"
#include "game_player.h"
#include "texture.h"
#include "sprite.h"
#include "CollisionManager.h"
#include <cmath>
#include <cstdlib>
#include "Audio.h"
#include "direct3d.h"

#ifdef _DEBUG
#include <Windows.h>
#include <string>
#endif

#include "game_manager.h"

//==============================================================================
// GuillotineBladeProjectile - ギロチンの刃
//==============================================================================
GuillotineBladeProjectile::GuillotineBladeProjectile(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size,
    Boss* boss, Player* target, float targetX, float groundY)
    : BossProjectile(pos, size, boss, target)
    , m_targetX(targetX)
    , m_groundY(groundY - 64.0f) {
}

void GuillotineBladeProjectile::Initialize() {
    if (m_textureId < 0) m_textureId = TextureManager::Instance().Get(TexID::BOSS_GUILLOTINE_BLADE);
    if (m_warningTexId < 0) m_warningTexId = TextureManager::Instance().Get(TexID::BOSS_WARNING);

    m_state = State::WARNING;
    m_warningTimer = 0.0f;
    m_disappearTimer = 0.0f;
    m_velocityY = 0.0f;
    m_hasHitPlayer = false;

    // フェードイン開始
    StartFadeIn();

    // 警告中はコライダーを無効化（落下開始時に有効化）
    GetColliders().clear();
}

void GuillotineBladeProjectile::Update() {
    if (m_state == State::INACTIVE) return;

    float deltaTime = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());

    // フェード更新
    UpdateFade(deltaTime);

    switch (m_state) {
    case State::WARNING:
        m_warningTimer += deltaTime;
        if (m_warningTimer >= m_warningDuration) {
            StartFalling();
        }
        break;

    case State::FALLING:
        m_velocityY += 50.0f * deltaTime;  // 加速
        {
            DirectX::XMFLOAT2 pos = GetPosition();
            pos.y += (m_fallSpeed + m_velocityY) * deltaTime;
            SetProjectilePosition(pos);
        }

        // コライダー位置更新
        UpdateColliderPosition();

        // 着地判定
        if (GetPosition().y + GetSize().y >= m_groundY) {
            DirectX::XMFLOAT2 pos = GetPosition();
            pos.y = m_groundY - GetSize().y;  // ギロチンの底辺が地面に着く
            SetProjectilePosition(pos);
            m_state = State::LANDED;
            StartFadeOut();
			Audio_PlaySEMulti(AudioID::SE_BOSS_GUILLOTINE);

            // 着地でコライダー削除
            for (auto& col : GetColliders()) {
                col.SetActive(false);
            }
        }
        break;

    case State::LANDED:
        // フェードアウト完了で削除
        if (IsFadeOutComplete()) {
            m_state = State::INACTIVE;
            SetActive(false);
            MarkForDeletion();
        }
        break;

    default:
        break;
    }
}

void GuillotineBladeProjectile::StartFalling() {
    m_state = State::FALLING;
    m_alpha = 1.0f;
    m_isFadingIn = false;

    Audio_PlaySEMulti(AudioID::SE_BOSS_GUILLOTINE_RAKKA, 0.5f);

    // 落下開始時にコライダーを追加してCollisionManagerに登録
    GetColliders().clear();
    AddCollider(GetPosition(), GetSize(), ColliderType::BOSS_ATTACK);
    CollisionManager::GetInstance().AddObject(this);
}

void GuillotineBladeProjectile::Draw() {
    if (m_state == State::INACTIVE) return;

    // 警告表示
    if (m_state == State::WARNING && m_warningTexId >= 0) {
        Sprite_ScrollDraw(m_warningTexId,
            m_targetX, m_groundY - 32.0f,
            GetSize().x, 32.0f,
            0.0f, 0.0f, 1024.0f, 1024.0f,
            0.0f,
            { 1.0f, 1.0f, 1.0f, m_alpha });
    }

    // ギロチン本体（WARNING状態からフェードインで表示）
    if (m_textureId >= 0) {
        Sprite_ScrollDraw(m_textureId,
            GetPosition().x, GetPosition().y,
            GetSize().x, GetSize().y,
            0.0f, 0.0f, m_bladeWidth, m_bladeHeight,
            0.0f,
            { 1.0f, 1.0f, 1.0f, m_alpha });
    }
}

void GuillotineBladeProjectile::OnCollision(const GameObject* otherObject, const Collider* myCollider,
    const Collider* otherCollider, COLLISION_VEC hitVec) {
    if (!otherObject || !otherCollider) return;
    if (m_hasHitPlayer) return;  // 多重ヒット防止
    if (m_state != State::FALLING) return;  // 落下中以外は当たらない
    if (IsFadingOut()) return;  // フェードアウト中は当たらない

    if (otherObject->GetTag() == ObjectTag::PLAYER) {
        if (otherCollider->GetType() == ColliderType::BODY) {
            ApplyDamageToPlayer();
            m_hasHitPlayer = true;

            // ★当たり判定を無効化（描画は継続）
            for (auto& col : GetColliders()) {
                col.SetActive(false);
            }

#ifdef _DEBUG
            OutputDebugStringA("GuillotineBladeProjectile: Hit player! Collider disabled.\n");
#endif
        }
    }
}

//==============================================================================
// MaskBulletProjectile - 仮面弾
//==============================================================================
MaskBulletProjectile::MaskBulletProjectile(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size,
    Boss* boss, Player* target, bool fromLeft, float stageWidth)
    : BossProjectile(pos, size, boss, target)
    , m_fromLeft(fromLeft)
    , m_stageWidth(stageWidth) {
}

void MaskBulletProjectile::Initialize() {
    if (m_textureId < 0) m_textureId = TextureManager::Instance().Get(TexID::BOSS_MASK_BULLET);

    m_state = State::CHARGING;
    m_chargeTimer = 0.0f;
    m_hasHitPlayer = false;

    // フェードイン開始
    StartFadeIn();

    // チャージ中はコライダーを無効化
    GetColliders().clear();
}

void MaskBulletProjectile::Update() {
    if (m_state == State::INACTIVE) return;

    float deltaTime = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());

    // フェード更新
    UpdateFade(deltaTime);

    switch (m_state) {
    case State::CHARGING:
        m_chargeTimer += deltaTime;
        if (m_chargeTimer >= m_chargeDuration) {
            Fire();
        }
        break;

    case State::FIRED:
    {
        DirectX::XMFLOAT2 pos = GetPosition();
        DirectX::XMFLOAT2 vel;

        if (m_moveAngle != 0.0f) {
            // 斜め移動
            vel.x = cosf(m_moveAngle) * m_bulletSpeed;
            vel.y = sinf(m_moveAngle) * m_bulletSpeed;
        } else {
            // 水平移動
            float dir = m_fromLeft ? 1.0f : -1.0f;
            vel.x = m_bulletSpeed * dir;
            vel.y = 0.0f;
        }

        pos.x += vel.x * deltaTime;
        pos.y += vel.y * deltaTime;
        SetProjectilePosition(pos);

        UpdateColliderPosition();

        // 境界チェック（LANDED状態へ移行）
        float centerX = pos.x + GetSize().x * 0.5f;
        if ((m_fromLeft && centerX >= m_stageRight) ||
            (!m_fromLeft && centerX <= m_stageLeft) ||
            pos.y > m_stageBottom) {
            m_state = State::LANDED;
            StartFadeOut();
            for (auto& col : GetColliders()) {
                col.SetActive(false);
            }
        }

        // 画面外判定（フェールセーフ）
        if (pos.x < -500.0f || pos.x > m_stageWidth + 500.0f ||
            pos.y > m_stageBottom + 500.0f || pos.y < -500.0f) {
            m_state = State::INACTIVE;
            SetActive(false);
            MarkForDeletion();
        }
    }
    break;

    case State::LANDED:
    {
        // フェードアウトしながら移動継続
        DirectX::XMFLOAT2 pos = GetPosition();
        DirectX::XMFLOAT2 vel;

        if (m_moveAngle != 0.0f) {
            vel.x = cosf(m_moveAngle) * m_bulletSpeed;
            vel.y = sinf(m_moveAngle) * m_bulletSpeed;
        } else {
            float dir = m_fromLeft ? 1.0f : -1.0f;
            vel.x = m_bulletSpeed * dir;
            vel.y = 0.0f;
        }

        pos.x += vel.x * deltaTime;
        pos.y += vel.y * deltaTime;
        SetProjectilePosition(pos);

        // フェードアウト完了で削除
        if (IsFadeOutComplete()) {
            m_state = State::INACTIVE;
            SetActive(false);
            MarkForDeletion();
        }
    }
    break;

    default:
        break;
    }
}

void MaskBulletProjectile::Fire() {
    m_state = State::FIRED;
    m_alpha = 1.0f;
    m_isFadingIn = false;

	Audio_PlaySEMulti(AudioID::SE_MASK_FIRE);

    // 発射時にコライダーを追加してCollisionManagerに登録
    GetColliders().clear();
    AddCollider(GetPosition(), GetSize(), ColliderType::BOSS_ATTACK);
    CollisionManager::GetInstance().AddObject(this);
}

void MaskBulletProjectile::Draw() {
    if (m_state == State::INACTIVE) return;

    if (m_textureId >= 0) {
        // チャージ中でフェードイン完了後は点滅
        float drawAlpha = m_alpha;
        if (m_state == State::CHARGING && IsFadeInComplete()) {
            float blinkTime = m_chargeTimer * 10.0f;
            drawAlpha = (fmodf(blinkTime, 2.0f) < 1.0f) ? m_alpha : m_alpha * 0.5f;
        }

        // テクスチャの向きを調整
        // 移動方向 - 90度（π/2）で正面
        float drawAngle = m_moveAngle - 1.5708f;  // -90度 = -π/2

        // 横移動の場合（角度が0の場合）
        if (m_moveAngle == 0.0f) {
            drawAngle = m_fromLeft ? -1.5708f : 1.5708f;  // 左から→右向き、右から→左向き
        }

        Sprite_ScrollDraw(m_textureId,
            GetPosition().x, GetPosition().y,
            GetSize().x, GetSize().y,
            0.0f, 0.0f, 512.0f, 512.0f,
            drawAngle,
            { 1.0f, 1.0f, 1.0f, drawAlpha });
    }
}

void MaskBulletProjectile::OnCollision(const GameObject* otherObject, const Collider* myCollider,
    const Collider* otherCollider, COLLISION_VEC hitVec) {
    if (!otherObject || !otherCollider) return;
    if (m_hasHitPlayer) return;
    if (m_state != State::FIRED) return;
    if (IsFadingOut()) return;  // フェードアウト中は当たらない

    if (otherObject->GetTag() == ObjectTag::PLAYER) {
        if (otherCollider->GetType() == ColliderType::BODY) {
            // ノックバック処理
            float knockDir = m_fromLeft ? 1.0f : -1.0f;
            ApplyKnockbackToPlayer(knockDir, 500.0f);
            ApplyDamageToPlayer();
            m_hasHitPlayer = true;

            // 当たり判定を無効化（描画は継続）
            for (auto& col : GetColliders()) {
                col.SetActive(false);
            }

#ifdef _DEBUG
            OutputDebugStringA("MaskBulletProjectile: Hit player! Collider disabled.\n");
#endif
        }
    }
}

//==============================================================================
// BeamHitboxProjectile - ビーム当たり判定
//==============================================================================
BeamHitboxProjectile::BeamHitboxProjectile(DirectX::XMFLOAT2 originPos, float angle,
    Boss* boss, Player* target, float length, float width)
    : BossProjectile(originPos, { length, width }, boss, target)
    , m_originPos(originPos)
    , m_beamAngle(angle)
    , m_beamLength(length)
    , m_beamWidth(width) {
}

void BeamHitboxProjectile::Initialize() {
    m_lifeTimer = 0.0f;
    m_hasHitPlayer = false;

    // OBB無理だから短いAABBいっぱい作って当たり判定近似
    float cosA = cosf(m_beamAngle);
    float sinA = sinf(m_beamAngle);

    // 1セグメントの長さ
    float segmentSize = m_beamLength / SEGMENT_COUNT;
    float spacing = segmentSize * 0.5f;  // 半分ずつ重ねる

    // セグメント数を増やして重ねる
    int actualSegments = SEGMENT_COUNT * 2 - 1;

    for (int i = 0; i < actualSegments; ++i) {
        // セグメントの中心位置
        float t = i * spacing + segmentSize * 0.5f;
        if (t > m_beamLength) break;

        float cx = m_originPos.x + cosA * t;
        float cy = m_originPos.y + sinA * t;

        // コライダーのサイズ（正方形に近くして隙間を埋める）
        float colSize = segmentSize;

        // コライダーの左上位置
        float colX = cx - colSize * 0.5f;
        float colY = cy - colSize * 0.5f;

        AddCollider(
            { colX, colY },
            { colSize, colSize },
            ColliderType::BOSS_ATTACK
        );
    }

    CollisionManager::GetInstance().AddObject(this);

    // 全体のサイズ設定（描画用）
    SetProjectilePosition(m_originPos);
    SetSize({ m_beamLength, m_beamWidth });
}

void BeamHitboxProjectile::Update() {
    float deltaTime = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());

    m_lifeTimer += deltaTime;
    if (m_lifeTimer >= m_lifeDuration) {
        SetActive(false);
        MarkForDeletion();
    }
}

void BeamHitboxProjectile::Draw() {
    // ビームの描画は MaskBeamAttack 側で行う（仮面と一緒に描画するため）
}

void BeamHitboxProjectile::OnCollision(const GameObject* otherObject, const Collider* myCollider,
    const Collider* otherCollider, COLLISION_VEC hitVec) {
    if (!otherObject || !otherCollider) return;
    if (m_hasHitPlayer) return;

    if (otherObject->GetTag() == ObjectTag::PLAYER) {
        if (otherCollider->GetType() == ColliderType::BODY) {
            // より精密なビーム判定（点と線分の距離）
            auto playerPos = m_target->GetPosition();
            auto playerSize = m_target->GetSize();
            float px = playerPos.x + playerSize.x * 0.5f;
            float py = playerPos.y + playerSize.y * 0.5f;

            float endX = m_originPos.x + cosf(m_beamAngle) * m_beamLength;
            float endY = m_originPos.y + sinf(m_beamAngle) * m_beamLength;

            float lineLen = sqrtf((endX - m_originPos.x) * (endX - m_originPos.x) +
                (endY - m_originPos.y) * (endY - m_originPos.y));
            float dist = fabsf((endY - m_originPos.y) * px - (endX - m_originPos.x) * py +
                endX * m_originPos.y - endY * m_originPos.x) / lineLen;

            if (dist < m_beamWidth * 0.5f + playerSize.x * 0.5f) {
                // ノックバック処理（ビームの向きに応じて）
                float knockDir = cosf(m_beamAngle) >= 0.0f ? 1.0f : -1.0f;
                ApplyKnockbackToPlayer(knockDir, 600.0f);

                ApplyDamageToPlayer();
                m_hasHitPlayer = true;

                // 当たり判定を無効化（描画は継続）
                for (auto& col : GetColliders()) {
                    col.SetActive(false);
                }

#ifdef _DEBUG
                OutputDebugStringA("BeamHitboxProjectile: Hit player! Collider disabled.\n");
#endif
            }
        }
    }
}

//==============================================================================
// ScratchProjectile - ひっかきエフェクト
//==============================================================================
ScratchProjectile::ScratchProjectile(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size,
    Boss* boss, Player* target)
    : BossProjectile(pos, size, boss, target) {
}

ScratchProjectile::~ScratchProjectile() {
}

void ScratchProjectile::Initialize() {
    if (m_textureId < 0) m_textureId = TextureManager::Instance().Get(TexID::BOSS_SCRATCH);

    m_state = State::READY;
    m_readyTimer = 0.0f;
    m_currentFrame = 0;
    m_frameTimer = 0.0f;
    m_hasHitPlayer = false;
    m_alpha = 0.0f;  // フェードイン用に0から開始

    GetColliders().clear();
}

void ScratchProjectile::Update() {
    if (m_state == State::INACTIVE) return;

    float deltaTime = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());

    switch (m_state) {
    case State::READY:
        m_readyTimer += deltaTime;

        // 1秒かけてフェードイン（0.0→1.0）
        m_alpha = m_readyTimer / m_readyDuration;
        if (m_alpha > 1.0f) m_alpha = 1.0f;

        if (m_readyTimer >= m_readyDuration) {
            m_state = State::ACTIVE;
            m_alpha = 1.0f;
            m_currentFrame = 0;
            m_frameTimer = 0.0f;

            // 当たり判定を0.8倍で中央に配置
            float drawW = GetSize().x;
            float drawH = GetSize().y;
            float colW = drawW * m_colliderScale;
            float colH = drawH * m_colliderScale;
            float offsetX = (drawW - colW) * 0.5f;
            float offsetY = (drawH - colH) * 0.5f;

            GetColliders().clear();
            AddCollider(
                { GetPosition().x + offsetX, GetPosition().y + offsetY },
                { colW, colH },
                ColliderType::BOSS_ATTACK
            );
            CollisionManager::GetInstance().AddObject(this);
        }
        break;

    case State::ACTIVE:
    {
        m_frameTimer += deltaTime;
        if (m_frameTimer >= m_frameSpeed) {
            m_frameTimer -= m_frameSpeed;
            m_currentFrame++;
        }

        // コライダー位置更新
        float drawW = GetSize().x;
        float drawH = GetSize().y;
        float colW = drawW * m_colliderScale;
        float colH = drawH * m_colliderScale;
        float offsetX = (drawW - colW) * 0.5f;
        float offsetY = (drawH - colH) * 0.5f;

        if (!GetColliders().empty()) {
            GetColliders()[0].SetColliderPos(
                { GetPosition().x + offsetX, GetPosition().y + offsetY });
        }

        // 全フレーム再生完了
        if (m_currentFrame >= ANIM_TOTAL) {
            for (auto& col : GetColliders()) {
                col.SetActive(false);
            }
            m_state = State::INACTIVE;
            SetActive(false);
            MarkForDeletion();
        }
        break;
    }

    default:
        break;
    }
}

void ScratchProjectile::Draw() {
    if (m_state == State::INACTIVE) return;
    if (m_textureId < 0) return;

    int frame = m_currentFrame;

    // READY状態は最初のフレーム
    if (m_state == State::READY) {
        frame = 0;
    }

    if (frame >= ANIM_TOTAL) frame = ANIM_TOTAL - 1;

    int col = frame % ANIM_COLS;
    int row = frame / ANIM_COLS;

    float uvX = col * FRAME_WIDTH;
    float uvY = row * FRAME_HEIGHT;

    // 加算合成で明るく描画
    Direct3D_SetAlphaBlend(BLEND_ADD);

    for (int i = 0; i < 3; ++i) {
        Sprite_ScrollDraw(m_textureId,
            GetPosition().x, GetPosition().y,
            GetSize().x, GetSize().y,
            uvX, uvY,
            FRAME_WIDTH, FRAME_HEIGHT,
            0.0f,
            { 1.0f, 1.0f, 1.0f, m_alpha });
    }

    // 元に戻す
    Direct3D_SetAlphaBlend(BLEND_TRANSPARENT);
}

void ScratchProjectile::OnCollision(const GameObject* otherObject, const Collider* myCollider,
    const Collider* otherCollider, COLLISION_VEC hitVec) {
    if (!otherObject || !otherCollider) return;
    if (m_hasHitPlayer) return;
    if (m_state != State::ACTIVE) return;

    if (otherObject->GetTag() == ObjectTag::PLAYER) {
        if (otherCollider->GetType() == ColliderType::BODY) {
            ApplyDamageToPlayer();
            m_hasHitPlayer = true;

            for (auto& col : GetColliders()) {
                col.SetActive(false);
            }
        }
    }
}

//==============================================================================
// 攻撃①: GuillotineDropAttack - ギロチン落とし
//==============================================================================
void GuillotineDropAttack::Start(Boss* boss, Player* target) {
    m_boss = boss;
    m_target = target;
    m_finished = false;
    m_timer = 0.0f;
    m_spawnedCount = 0;

    // テクスチャ読み込み
    if (m_bladeTexId < 0) m_bladeTexId = TextureManager::Instance().Get(TexID::BOSS_GUILLOTINE_BLADE);
    if (m_warningTexId < 0) m_warningTexId = TextureManager::Instance().Get(TexID::BOSS_WARNING);

    // 地面のY座標をステージから取得
    m_groundY = m_stageHeight - 64.0f;

    // スポーン位置を事前計算
    m_spawnPositions.clear();

    // ステージの有効範囲（左右64.0f内側）
    float minSpawnX = 64.0f;
    float maxSpawnX = m_stageWidth - 64.0f - m_bladeSize.x;  // ギロチンの幅も考慮
    float spawnRange = maxSpawnX - minSpawnX;

    // ランダム配置
    for (int i = 0; i < GUILLOTINE_COUNT; ++i) {
        float randomX = minSpawnX + static_cast<float>(rand() % static_cast<int>(spawnRange));
        m_spawnPositions.push_back(randomX);
    }

    // チャレンジモードなら時間スケール適用
    m_hardModeScale = GameManager::Instance().IsHardMode() ? 0.7f : 1.0f;

#ifdef _DEBUG
    OutputDebugStringA("GuillotineDropAttack: Started\n");
#endif
}

void GuillotineDropAttack::Update(float deltaTime) {
    if (m_finished) return;

    m_timer += deltaTime;

    // チャレンジモードならスポーン間隔を短縮
    float spawnInterval = SPAWN_INTERVAL * m_hardModeScale;
    float warningTime = WARNING_TIME * m_hardModeScale;

    // 新しいギロチンをスポーン
    while (m_spawnedCount < GUILLOTINE_COUNT &&
        m_timer >= spawnInterval * m_spawnedCount) {

        float targetX = m_spawnPositions[m_spawnedCount];
        DirectX::XMFLOAT2 startPos = { targetX, -200.0f };

        auto* blade = SpawnProjectile<GuillotineBladeProjectile>(
            startPos, m_bladeSize, m_boss, m_target, targetX, m_groundY);
        blade->SetWarningDuration(warningTime);
        blade->SetFallSpeed(FALL_SPEED);
        blade->SetDisappearDuration(DISAPPEAR_TIME);

        m_spawnedCount++;
    }

    // 全Projectileの更新
    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Update();
        }
    }

    // 不要なProjectileを削除
    CleanupProjectiles();

    // 全て完了したか確認
    if (m_spawnedCount >= GUILLOTINE_COUNT && m_projectiles.empty()) {
        m_finished = true;
    }
}

void GuillotineDropAttack::Draw() {
    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Draw();
        }
    }
}

void GuillotineDropAttack::Reset() {
    ClearAllProjectiles();
    m_spawnPositions.clear();
    m_finished = false;
    m_timer = 0.0f;
    m_spawnedCount = 0;
    m_boss = nullptr;
    m_target = nullptr;
}

//==============================================================================
// 攻撃②: MaskBulletHorizontalAttack - 仮面弾（左右）
//==============================================================================
void MaskBulletHorizontalAttack::Start(Boss* boss, Player* target) {
    m_boss = boss;
    m_target = target;
    m_finished = false;
    m_timer = 0.0f;
    m_spawnedCount = 0;

    if (m_maskTexId < 0) m_maskTexId = TextureManager::Instance().Get(TexID::BOSS_MASK_BULLET);

    // チャレンジモードなら時間スケール適用
    m_hardModeScale = GameManager::Instance().IsHardMode() ? 0.7f : 1.0f;

#ifdef _DEBUG
    OutputDebugStringA("MaskBulletHorizontalAttack: Started\n");
#endif
}

void MaskBulletHorizontalAttack::Update(float deltaTime) {
    if (m_finished) return;

    m_timer += deltaTime;

    // 境界計算
    float stageLeft = STAGE_MARGIN;
    float stageRight = m_stageWidth - STAGE_MARGIN;
    float stageBottom = m_stageHeight - STAGE_MARGIN;

    // チャレンジモードなら時間短縮
    float spawnInterval = SPAWN_INTERVAL * m_hardModeScale;
    float chargeTime = CHARGE_TIME * m_hardModeScale;

    // 新しい仮面弾をスポーン
    while (m_spawnedCount < MASK_COUNT &&
        m_timer >= spawnInterval * m_spawnedCount) {

        bool fromLeft = (m_spawnedCount % 2 == 0);
        float startX = fromLeft ? stageLeft : stageRight - m_maskSize.x;

        float playerY = m_target ? m_target->GetPosition().y : 500.0f;
        float yOffset = static_cast<float>((rand() % static_cast<int>(PLAYER_Y_OFFSET_RANGE * 2)) - static_cast<int>(PLAYER_Y_OFFSET_RANGE));

        DirectX::XMFLOAT2 startPos = { startX, playerY + yOffset };

        auto* bullet = SpawnProjectile<MaskBulletProjectile>(
            startPos, m_maskSize, m_boss, m_target, fromLeft, m_stageWidth);
        bullet->SetChargeDuration(chargeTime);
        bullet->SetBulletSpeed(BULLET_SPEED);
        bullet->SetTextureId(m_maskTexId);
        bullet->SetStageBounds(stageLeft, stageRight, stageBottom);

        m_spawnedCount++;
    }

    // 全Projectileの更新
    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Update();
        }
    }

    // 不要なProjectileを削除
    CleanupProjectiles();

    // 全て完了したか確認
    if (m_spawnedCount >= MASK_COUNT && m_projectiles.empty()) {
        m_finished = true;
    }
}

void MaskBulletHorizontalAttack::Draw() {
    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Draw();
        }
    }
}

void MaskBulletHorizontalAttack::Reset() {
    ClearAllProjectiles();
    m_finished = false;
    m_timer = 0.0f;
    m_spawnedCount = 0;
    m_boss = nullptr;
    m_target = nullptr;
}

//==============================================================================
// 攻撃③: MaskBulletDiagonalAttack - 仮面弾（斜め）
//==============================================================================
void MaskBulletDiagonalAttack::Start(Boss* boss, Player* target) {
    m_boss = boss;
    m_target = target;
    m_finished = false;
    m_timer = 0.0f;
    m_spawnedCount = 0;

    if (m_maskTexId < 0) m_maskTexId = TextureManager::Instance().Get(TexID::BOSS_MASK_BULLET);

    m_hardModeScale = GameManager::Instance().IsHardMode() ? 0.7f : 1.0f;

#ifdef _DEBUG
    OutputDebugStringA("MaskBulletDiagonalAttack: Started\n");
#endif
}

void MaskBulletDiagonalAttack::Update(float deltaTime) {
    if (m_finished) return;

    m_timer += deltaTime;

    // 境界計算
    float stageLeft = STAGE_MARGIN;
    float stageRight = m_stageWidth - STAGE_MARGIN;
    float stageBottom = m_stageHeight - STAGE_MARGIN;

    float spawnInterval = SPAWN_INTERVAL * m_hardModeScale;
    float chargeTime = CHARGE_TIME * m_hardModeScale;

    while (m_spawnedCount < MASK_COUNT &&
        m_timer >= spawnInterval * m_spawnedCount) {

        bool fromLeft = (m_spawnedCount % 2 == 0);
        float startX = fromLeft ? stageLeft : stageRight - m_maskSize.x;
        float startY = SPAWN_Y_BASE + static_cast<float>(rand() % static_cast<int>(SPAWN_Y_RANDOM_RANGE));

        DirectX::XMFLOAT2 startPos = { startX, startY };

        auto* bullet = SpawnProjectile<MaskBulletProjectile>(
            startPos, m_maskSize, m_boss, m_target, fromLeft, m_stageWidth);
        bullet->SetChargeDuration(chargeTime);
        bullet->SetBulletSpeed(BULLET_SPEED);
        bullet->SetTextureId(m_maskTexId);
        bullet->SetStageBounds(stageLeft, stageRight, stageBottom);

        float angle = 0.0f;
        if (m_target) {
            float playerCenterX = m_target->GetPosition().x + m_target->GetSize().x * 0.5f;
            float playerCenterY = m_target->GetPosition().y + m_target->GetSize().y * 0.5f;
            float bulletCenterX = startPos.x + m_maskSize.x * 0.5f;
            float bulletCenterY = startPos.y + m_maskSize.y * 0.5f;

            float dx = playerCenterX - bulletCenterX;
            float dy = playerCenterY - bulletCenterY;
            angle = atan2f(dy, dx);
        } else {
            angle = fromLeft ? DIAGONAL_ANGLE : (3.14159f - DIAGONAL_ANGLE);
        }
        bullet->SetMoveAngle(angle);

        m_spawnedCount++;
    }

    // 全Projectileの更新
    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Update();
        }
    }

    // 不要なProjectileを削除
    CleanupProjectiles();

    // 全て完了したか確認
    if (m_spawnedCount >= MASK_COUNT && m_projectiles.empty()) {
        m_finished = true;
    }
}

void MaskBulletDiagonalAttack::Draw() {
    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Draw();
        }
    }
}

void MaskBulletDiagonalAttack::Reset() {
    ClearAllProjectiles();
    m_finished = false;
    m_timer = 0.0f;
    m_spawnedCount = 0;
    m_boss = nullptr;
    m_target = nullptr;
}

//==============================================================================
// 攻撃④: MaskBeamAttack - 仮面からビーム
//==============================================================================
void MaskBeamAttack::Start(Boss* boss, Player* target) {
    m_boss = boss;
    m_target = target;
    m_finished = false;
    m_timer = 0.0f;
    m_loopCount = 0;
    m_currentBeam = nullptr;

    if (m_maskTexId < 0) m_maskTexId = TextureManager::Instance().Get(TexID::BOSS_MASK_BEAM);
    if (m_maskOpenTexId < 0) m_maskOpenTexId = TextureManager::Instance().Get(TexID::BOSS_MASK_BEAM_OPEN);
    if (m_beamTexId < 0) m_beamTexId = TextureManager::Instance().Get(TexID::BOSS_BEAM);
    if (m_aimLineTexId < 0) m_aimLineTexId = TextureManager::Instance().Get(TexID::BOSS_AIM_LINE);

    // ビームアニメーション作成（512x512、6x6=36パターン）
    if (!m_beamAnim) {
        m_beamAnim = new AnimPattern(
            m_beamTexId,
            30, 6,              // 36パターン、横6
            (FIRE_TIME * m_hardModeScale) / 30.0f,               // アニメーション速度
            { 0, 0 },           // 開始位置
            { 512 / 6, 512 / 5 },  // 1パターンのサイズ（約85x85）
            false				// ループしない
        );
    }

    m_beamAnimPlayer = new AnimPatternPlayer(m_beamAnim);

    SpawnNewMask();

#ifdef _DEBUG
    OutputDebugStringA("MaskBeamAttack: Started\n");
#endif
}

void MaskBeamAttack::Update(float deltaTime) {
    if (m_finished) return;

    m_timer += deltaTime;

    UpdatePhase(deltaTime);

    // ビームアニメーション更新（FIRE中のみ）
    if (m_currentMask.phase == Phase::FIRE && m_beamAnimPlayer) {
        m_beamAnimPlayer->Update();
    }

    // Projectileの更新
    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Update();
        }
    }

    // 不要なProjectileを削除
    CleanupProjectiles();
}

void MaskBeamAttack::UpdatePhase(float deltaTime) {
    auto& mask = m_currentMask;

    if (!mask.isActive) {
        // 次のループへ
        m_loopCount++;
        if (m_loopCount >= MAX_LOOPS) {
            m_finished = true;
            return;
        }
        SpawnNewMask();
        return;
    }

    mask.phaseTimer += deltaTime;

    float spawnTime = SPAWN_TIME * m_hardModeScale;
    float aimTime = AIM_TIME * m_hardModeScale;
    float lockTime = LOCK_TIME * m_hardModeScale;
    float fireTime = FIRE_TIME * m_hardModeScale;
    float disappearTime = DISAPPEAR_TIME * m_hardModeScale;

    switch (mask.phase) {
    case Phase::SPAWN:
        if (mask.phaseTimer >= spawnTime) {
            mask.phase = Phase::AIM;
            mask.phaseTimer = 0.0f;
        }
        break;

    case Phase::AIM:
        mask.angle = CalculateAngleToPlayer();
        if (mask.phaseTimer >= aimTime) {
            mask.phase = Phase::LOCK;
            mask.phaseTimer = 0.0f;
        }
        break;

    case Phase::LOCK:
        if (mask.phaseTimer >= lockTime) {
            mask.phase = Phase::FIRE;
            mask.phaseTimer = 0.0f;
            if (m_beamAnimPlayer) {
                m_beamAnimPlayer->Reset();
            }
            CreateBeamProjectile();
            Audio_PlaySEMulti(AudioID::SE_BOSS_BEAM);
        }
        break;

    case Phase::FIRE:
        if (mask.phaseTimer >= fireTime) {
            mask.phase = Phase::DISAPPEAR;
            mask.phaseTimer = 0.0f;
            RemoveBeamProjectile();
        }
        break;

    case Phase::DISAPPEAR:
        if (mask.phaseTimer >= disappearTime) {
            mask.isActive = false;
        }
        break;
    }
}

void MaskBeamAttack::SpawnNewMask() {
    // ランダムな位置に出現
    float x = 200.0f + static_cast<float>(rand() % 1000);
    float y = 100.0f + static_cast<float>(rand() % 400);

    m_currentMask.pos = { x, y };
    m_currentMask.angle = CalculateAngleToPlayer();
    m_currentMask.phase = Phase::SPAWN;
    m_currentMask.phaseTimer = 0.0f;
    m_currentMask.isActive = true;

    // 新しいマスクのためにアニメーションリセット
    if (m_beamAnimPlayer) {
        m_beamAnimPlayer->Reset();
    }

#ifdef _DEBUG
    OutputDebugStringA(("MaskBeamAttack: New mask spawned at (" +
        std::to_string(x) + ", " + std::to_string(y) + ")\n").c_str());
#endif
}

float MaskBeamAttack::CalculateAngleToPlayer() {
    if (!m_target) return 0.0f;

    auto playerPos = m_target->GetPosition();
    auto playerCenter = DirectX::XMFLOAT2{
        playerPos.x + m_target->GetSize().x * 0.5f,
        playerPos.y + m_target->GetSize().y * 0.5f
    };

    auto maskCenter = DirectX::XMFLOAT2{
        m_currentMask.pos.x + m_maskSize.x * 0.5f,
        m_currentMask.pos.y + m_maskSize.y * 0.5f
    };

    float dx = playerCenter.x - maskCenter.x;
    float dy = playerCenter.y - maskCenter.y;

    return atan2f(dy, dx);
}

void MaskBeamAttack::CreateBeamProjectile() {
    DirectX::XMFLOAT2 maskCenter = {
        m_currentMask.pos.x + m_maskSize.x * 0.5f,
        m_currentMask.pos.y + m_maskSize.y * 0.5f
    };

    m_currentBeam = SpawnProjectile<BeamHitboxProjectile>(
        maskCenter, m_currentMask.angle, m_boss, m_target, BEAM_LENGTH, BEAM_WIDTH);
    m_currentBeam->SetLifeDuration(FIRE_TIME);
}

void MaskBeamAttack::RemoveBeamProjectile() {
    if (m_currentBeam) {
        m_currentBeam->MarkForDeletion();
        m_currentBeam = nullptr;
    }
}

void MaskBeamAttack::DrawBeam(float centerX, float centerY, float angle) {
    if (!m_beamAnimPlayer || m_beamTexId < 0) return;

    // 1パターンを大きく表示
    float drawW = BEAM_LENGTH;
    float drawH = BEAM_WIDTH;

    // 現在のパターン番号を取得
    int pattern = m_beamAnimPlayer->GetPattern();
    int col = pattern % 6;
    int row = pattern / 6;
    float frameSizeX = 512.0f / 6.0f;
    float frameSizeY = 512.0f / 5.0f;

    // ビームの左端を仮面の中心に合わせる
    // Sprite_ScrollDrawは中心で回転するので位置調整
    float halfW = drawW * 0.5f;
    float beamCenterX = centerX + cosf(angle) * halfW;
    float beamCenterY = centerY + sinf(angle) * halfW;

    float drawX = beamCenterX - halfW;
    float drawY = beamCenterY - drawH * 0.5f;

    Sprite_ScrollDraw(m_beamTexId,
        drawX, drawY,
        drawW, drawH,
        col * frameSizeX, row * frameSizeY,
        frameSizeX, frameSizeY,
        angle,
        { 1.0f, 1.0f, 1.0f, 1.0f });
}


void MaskBeamAttack::Draw() {
    auto& mask = m_currentMask;

    if (!mask.isActive) return;

    float maskCenterX = mask.pos.x + m_maskSize.x * 0.5f;
    float maskCenterY = mask.pos.y + m_maskSize.y * 0.5f;

    // 照準線（AIM、LOCK中）
    if ((mask.phase == Phase::AIM || mask.phase == Phase::LOCK) && m_aimLineTexId >= 0) {
        float endX = maskCenterX + cosf(mask.angle) * BEAM_LENGTH;
        float endY = maskCenterY + sinf(mask.angle) * BEAM_LENGTH;

        // 点線で表示（簡易実装）
        float alpha = (mask.phase == Phase::LOCK) ? 1.0f : 0.5f;

        for (float t = 0.0f; t < 1.0f; t += 0.05f) {
            float x = maskCenterX + (endX - maskCenterX) * t;
            float y = maskCenterY + (endY - maskCenterY) * t;

            Sprite_ScrollDraw(m_aimLineTexId,
                x - 4.0f, y - 4.0f, 24.0f, 24.0f,
                0.0f, 0.0f, 1024.0f, 1024.0f,
                0.0f,
                { 1.0f, 1.0f, 1.0f, alpha });
        }
    }

    // ビーム（FIRE中）
    if (mask.phase == Phase::FIRE) {
        DrawBeam(maskCenterX, maskCenterY, mask.angle);
    }

    // 仮面
    int texId = (mask.phase == Phase::FIRE) ? m_maskOpenTexId : m_maskTexId;
    if (texId >= 0) {
        float alpha = 1.0f;
        if (mask.phase == Phase::SPAWN) {
            alpha = mask.phaseTimer / SPAWN_TIME;
        } else if (mask.phase == Phase::DISAPPEAR) {
            alpha = 1.0f - mask.phaseTimer / DISAPPEAR_TIME;
        }

        // テクスチャの向きを調整（元画像が上向きなので、ビーム方向に合わせて回転）
        float drawAngle = mask.angle - 1.5708f;  // -90度 = -π/2

        Sprite_ScrollDraw(texId,
            mask.pos.x, mask.pos.y,
            m_maskSize.x, m_maskSize.y,
            0.0f, 0.0f, 512.0f, 512.0f,
            drawAngle,
            { 1.0f, 1.0f, 1.0f, alpha });
    }
}

void MaskBeamAttack::Reset() {
    RemoveBeamProjectile();
    ClearAllProjectiles();
    m_currentMask.isActive = false;
    m_finished = false;
    m_timer = 0.0f;
    m_loopCount = 0;

    // アニメーション解放
    if (m_beamAnimPlayer) {
        delete m_beamAnimPlayer;
        m_beamAnimPlayer = nullptr;
    }
    if (m_beamAnim) {
        delete m_beamAnim;
        m_beamAnim = nullptr;
    }

    m_boss = nullptr;
    m_target = nullptr;
}

//==============================================================================
// 攻撃⑤: ScratchAttack - ひっかき攻撃
//==============================================================================
void ScratchAttack::Start(Boss* boss, Player* target) {
    m_boss = boss;
    m_target = target;
    m_finished = false;
    m_timer = 0.0f;
    m_spawnedCount = 0;
    m_elapsed = 0.0f;
    m_posHistory.clear();

    if (m_scratchTexId < 0) m_scratchTexId = TextureManager::Instance().Get(TexID::BOSS_SCRATCH);

    m_hardModeScale = GameManager::Instance().IsHardMode() ? 0.7f : 1.0f;

#ifdef _DEBUG
    OutputDebugStringA("ScratchAttack: Started\n");
#endif
}

DirectX::XMFLOAT2 ScratchAttack::GetDelayedPlayerPos() {
    // 0.3秒前の座標を探す
    float targetTime = m_elapsed - DELAY_TIME;

    // 履歴がない or まだ0.3秒経ってない場合は現在位置
    if (m_posHistory.empty() || targetTime <= 0.0f) {
        if (m_target) {
            return {
                m_target->GetPosition().x + m_target->GetSize().x * 0.5f,
                m_target->GetPosition().y + m_target->GetSize().y * 0.5f
            };
        }
        return { m_stageWidth * 0.5f, m_stageHeight * 0.5f };
    }

    // targetTime以前の最も近いレコードを探す
    DirectX::XMFLOAT2 result = m_posHistory.front().pos;
    for (size_t i = 0; i < m_posHistory.size(); ++i) {
        if (m_posHistory[i].time <= targetTime) {
            result = m_posHistory[i].pos;
        } else {
            break;
        }
    }

    return result;
}

void ScratchAttack::Update(float deltaTime) {
    if (m_finished) return;

    m_timer += deltaTime;
    m_elapsed += deltaTime;

    // プレイヤー座標を履歴に記録
    if (m_target) {
        DirectX::XMFLOAT2 center = {
            m_target->GetPosition().x + m_target->GetSize().x * 0.5f,
            m_target->GetPosition().y + m_target->GetSize().y * 0.5f
        };
        m_posHistory.push_back({ center, m_elapsed });

        // 古い履歴を削除（0.5秒分あれば十分）
        while (!m_posHistory.empty() && m_posHistory.front().time < m_elapsed - 0.5f) {
            m_posHistory.erase(m_posHistory.begin());
        }
    }

    float spawnInterval = SPAWN_INTERVAL * m_hardModeScale;
    float readyTime = READY_TIME * m_hardModeScale;

    // ひっかきをスポーン
    while (m_spawnedCount < SCRATCH_COUNT &&
        m_timer >= spawnInterval * m_spawnedCount) {

        DirectX::XMFLOAT2 targetPos = GetDelayedPlayerPos();

        float offsetX = static_cast<float>((rand() % 200) - 100);
        float offsetY = static_cast<float>((rand() % 200) - 100);

        DirectX::XMFLOAT2 spawnPos = {
            targetPos.x - m_scratchSize.x * 0.5f + offsetX,
            targetPos.y - m_scratchSize.y * 0.5f + offsetY
        };

        if (spawnPos.x < 64.0f) spawnPos.x = 64.0f;
        if (spawnPos.y < 64.0f) spawnPos.y = 64.0f;
        if (spawnPos.x + m_scratchSize.x > m_stageWidth - 64.0f)
            spawnPos.x = m_stageWidth - 64.0f - m_scratchSize.x;
        if (spawnPos.y + m_scratchSize.y > m_stageHeight - 64.0f)
            spawnPos.y = m_stageHeight - 64.0f - m_scratchSize.y;

        auto* scratch = SpawnProjectile<ScratchProjectile>(
            spawnPos, m_scratchSize, m_boss, m_target);
        scratch->SetReadyDuration(readyTime);
        scratch->SetFrameSpeed(FRAME_SPEED);
        scratch->SetTextureId(m_scratchTexId);

        m_spawnedCount++;

        Audio_PlaySEMulti(AudioID::SE_BOSS_SCRATCH, 0.06f);
    }

    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Update();
        }
    }

    CleanupProjectiles();

    if (m_spawnedCount >= SCRATCH_COUNT && m_projectiles.empty()) {
        m_finished = true;
    }
}

void ScratchAttack::Draw() {
    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Draw();
        }
    }
}

void ScratchAttack::Reset() {
    ClearAllProjectiles();
    m_posHistory.clear();
    m_finished = false;
    m_timer = 0.0f;
    m_elapsed = 0.0f;
    m_spawnedCount = 0;
    m_boss = nullptr;
    m_target = nullptr;
}



//==============================================================================
// 必殺①: SpecialBeamAttack - 仮面からビーム（強）
//==============================================================================
void SpecialBeamAttack::StartRound(int round) {
    // プレイヤーのY座標を取得してビーム位置を決定
    float playerCenterY = m_stageHeight * 0.5f;
    if (m_target) {
        playerCenterY = m_target->GetPosition().y + m_target->GetSize().y * 0.5f;
    }

    float maxY = m_stageHeight - 64.0f - m_maskSize.y * 0.5f;
    float minY = m_maskSize.y * 0.5f;

    // 上のビーム：プレイヤーより少し上
    float upperY = playerCenterY - 150.0f - m_maskSize.y * 0.5f;
    upperY = fmaxf(minY - m_maskSize.y * 0.5f, fminf(upperY, maxY - m_maskSize.y * 0.5f));

    // 下のビーム：プレイヤーのY位置に合わせる
    float lowerY = playerCenterY - m_maskSize.y * 0.5f;
    lowerY = fmaxf(minY - m_maskSize.y * 0.5f, fminf(lowerY, maxY - m_maskSize.y * 0.5f));

    if (round == 0) {
        // 1回目: 上段=左から右へ、下段=右から左へ
        // 上の仮面（左側、右方向にビーム）
        m_masks[0].pos = { -m_maskSize.x + 32.0f, upperY };
        m_masks[0].angle = 0.0f;
        m_masks[0].isLeft = true;

        // 下の仮面（右側、左方向にビーム）
        m_masks[1].pos = { m_stageWidth - 32.0f, lowerY };
        m_masks[1].angle = 3.14159f;
        m_masks[1].isLeft = false;
    } else {
        // 2回目: 上段=右から左へ、下段=左から右へ（逆パターン）
        // 上の仮面（右側、左方向にビーム）
        m_masks[0].pos = { m_stageWidth - 32.0f, upperY };
        m_masks[0].angle = 3.14159f;
        m_masks[0].isLeft = false;

        // 下の仮面（左側、右方向にビーム）
        m_masks[1].pos = { -m_maskSize.x + 32.0f, lowerY };
        m_masks[1].angle = 0.0f;
        m_masks[1].isLeft = true;
    }

    for (int i = 0; i < 2; ++i) {
        m_masks[i].phase = Phase::SPAWN;
        m_masks[i].phaseTimer = 0.0f;
        m_masks[i].isActive = true;
    }

    // アニメーションプレイヤー作成
    for (int i = 0; i < 2; ++i) {
        if (m_beamAnimPlayers[i]) {
            delete m_beamAnimPlayers[i];
        }
        m_beamAnimPlayers[i] = new AnimPatternPlayer(m_beamAnim);
    }
}

void SpecialBeamAttack::Start(Boss* boss, Player* target) {
    m_boss = boss;
    m_target = target;
    m_finished = false;
    m_timer = 0.0f;
    m_currentRound = 0;
    m_waitingForNextRound = false;
    m_roundGapTimer = 0.0f;

    if (m_maskTexId < 0) m_maskTexId = TextureManager::Instance().Get(TexID::BOSS_MASK_BEAM);
    if (m_maskOpenTexId < 0) m_maskOpenTexId = TextureManager::Instance().Get(TexID::BOSS_MASK_BEAM_OPEN);
    if (m_beamTexId < 0) m_beamTexId = TextureManager::Instance().Get(TexID::BOSS_BEAM);
    if (m_chargeEffectTexId < 0) m_chargeEffectTexId = TextureManager::Instance().Get(TexID::BOSS_CHARGE_EFFECT);
    if (m_aimLineTexId < 0) m_aimLineTexId = TextureManager::Instance().Get(TexID::BOSS_AIM_LINE);

    // ビームアニメーション作成
    if (!m_beamAnim) {
        m_beamAnim = new AnimPattern(
            m_beamTexId,
            30, 6,
            FIRE_TIME / 30.0f,
            { 0, 0 },
            { 512 / 6, 512 / 5 },
            false
        );
    }

    // 1回目開始
    StartRound(0);

#ifdef _DEBUG
    OutputDebugStringA("SpecialBeamAttack: Started (targeting player)\n");
#endif
}

void SpecialBeamAttack::Update(float deltaTime) {
    if (m_finished) return;

    m_timer += deltaTime;

    // ラウンド間の待機処理
    if (m_waitingForNextRound) {
        m_roundGapTimer += deltaTime;
        if (m_roundGapTimer >= ROUND_GAP_TIME) {
            m_waitingForNextRound = false;
            m_roundGapTimer = 0.0f;
            StartRound(m_currentRound);
        }

        // 待機中もProjectileの更新・クリーンアップは行う
        for (auto* proj : m_projectiles) {
            if (proj && proj->IsActive()) {
                proj->Update();
            }
        }
        CleanupProjectiles();
        return;
    }

    UpdatePhase(deltaTime);

    // ビームアニメーション更新
    for (int i = 0; i < 2; ++i) {
        if (m_masks[i].phase == Phase::FIRE && m_beamAnimPlayers[i]) {
            m_beamAnimPlayers[i]->Update();
        }
    }

    // Projectile更新
    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Update();
        }
    }

    CleanupProjectiles();
}

void SpecialBeamAttack::UpdatePhase(float deltaTime) {
    bool allInactive = true;

    for (int i = 0; i < 2; ++i) {
        auto& mask = m_masks[i];
        if (!mask.isActive) continue;

        allInactive = false;
        mask.phaseTimer += deltaTime;

        switch (mask.phase) {
        case Phase::SPAWN:
            if (mask.phaseTimer >= SPAWN_TIME) {
                mask.phase = Phase::CHARGE;
                mask.phaseTimer = 0.0f;
            }
            break;

        case Phase::CHARGE:
            if (mask.phaseTimer >= CHARGE_TIME) {
                mask.phase = Phase::FIRE;
                mask.phaseTimer = 0.0f;
                if (m_beamAnimPlayers[i]) {
                    m_beamAnimPlayers[i]->Reset();
                }
                // 最初の方だけ発射
                if (i == 0) {
                    Audio_PlaySEMulti(AudioID::SE_BOSS_BEAM);
                    CreateBeamProjectiles();
                }
            }
            break;

        case Phase::FIRE:
            if (mask.phaseTimer >= FIRE_TIME) {
                mask.phase = Phase::DISAPPEAR;
                mask.phaseTimer = 0.0f;
                if (i == 0) {
                    RemoveBeamProjectiles();
                }
            }
            break;

        case Phase::DISAPPEAR:
            if (mask.phaseTimer >= DISAPPEAR_TIME) {
                mask.isActive = false;
            }
            break;
        }
    }

    if (allInactive) {
        // 現在のラウンドが終了
        m_currentRound++;
        if (m_currentRound >= TOTAL_ROUNDS) {
            m_finished = true;
        } else {
            // 次のラウンドへの待機開始
            m_waitingForNextRound = true;
            m_roundGapTimer = 0.0f;
        }
    }
}

void SpecialBeamAttack::CreateBeamProjectiles() {
    for (int i = 0; i < 2; ++i) {
        if (m_beams[i]) continue;

        DirectX::XMFLOAT2 maskCenter = {
            m_masks[i].pos.x + m_maskSize.x * 0.5f,
            m_masks[i].pos.y + m_maskSize.y * 0.5f
        };

        float beamAngle = m_masks[i].isLeft ? 0.0f : 3.14159f;

        auto* beam = SpawnProjectile<BeamHitboxProjectile>(
            maskCenter, beamAngle, m_boss, m_target, BEAM_LENGTH, BEAM_WIDTH);
        beam->SetLifeDuration(FIRE_TIME);
        beam->SetDamage(DAMAGE);
        m_beams[i] = beam;
    }
}

void SpecialBeamAttack::RemoveBeamProjectiles() {
    for (int i = 0; i < 2; ++i) {
        if (m_beams[i]) {
            m_beams[i]->MarkForDeletion();
            m_beams[i] = nullptr;
        }
    }
}

void SpecialBeamAttack::DrawChargeEffect(float cx, float cy, float progress) {
    if (m_chargeEffectTexId < 0) return;

    float scale = 32.0f + progress * 64.0f;
    float alpha = 0.5f + progress * 0.5f;
    float rotation = m_timer * 5.0f;

    Sprite_ScrollDraw(m_chargeEffectTexId,
        cx - scale * 0.5f, cy - scale * 0.5f,
        scale, scale,
        0.0f, 0.0f, 1024.0f, 1024.0f,
        rotation,
        { 1.0f, 1.0f, 1.0f, alpha });
}

void SpecialBeamAttack::Draw() {
    // ラウンド間待機中は何も描画しない（Projectileは別途描画される）
    for (int i = 0; i < 2; ++i) {
        auto& mask = m_masks[i];
        if (!mask.isActive) continue;

        float maskCenterX = mask.pos.x + m_maskSize.x * 0.5f;
        float maskCenterY = mask.pos.y + m_maskSize.y * 0.5f;

        // 照準線描画（SPAWN + CHARGE時）
        if (mask.phase == Phase::SPAWN || mask.phase == Phase::CHARGE) {
            float aimAlpha = 0.0f;

            if (mask.phase == Phase::SPAWN) {
                aimAlpha = mask.phaseTimer / SPAWN_TIME * 0.4f;
            } else if (mask.phase == Phase::CHARGE) {
                float progress = mask.phaseTimer / CHARGE_TIME;
                float blinkSpeed = 6.0f + progress * 20.0f;
                float blink = (sinf(mask.phaseTimer * blinkSpeed) + 1.0f) * 0.5f;

                aimAlpha = 0.4f + progress * 0.6f;
                aimAlpha *= 0.5f + 0.5f * blink;
            }

            DrawAimLine(maskCenterX, maskCenterY, mask.angle, aimAlpha);
        }

        // チャージエフェクト
        if (mask.phase == Phase::CHARGE) {
            float progress = mask.phaseTimer / CHARGE_TIME;
            DrawChargeEffect(maskCenterX, maskCenterY, progress);
        }

        // ビーム描画（発射中）
        if (mask.phase == Phase::FIRE && m_beamAnimPlayers[i] && m_beamTexId >= 0) {
            int pattern = m_beamAnimPlayers[i]->GetPattern();
            int col = pattern % 6;
            int row = pattern / 6;
            float frameSizeX = 512.0f / 6.0f;
            float frameSizeY = 512.0f / 5.0f;

            float beamStartX, beamY;
            if (mask.isLeft) {
                beamStartX = maskCenterX;
                beamY = maskCenterY - BEAM_WIDTH * 0.5f;
            } else {
                beamStartX = maskCenterX - BEAM_LENGTH;
                beamY = maskCenterY - BEAM_WIDTH * 0.5f;
            }

            Sprite_ScrollDraw(m_beamTexId,
                beamStartX, beamY,
                BEAM_LENGTH, BEAM_WIDTH,
                col * frameSizeX, row * frameSizeY,
                frameSizeX, frameSizeY,
                mask.angle,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        }

        // 仮面描画
        int texId = (mask.phase == Phase::FIRE) ? m_maskOpenTexId : m_maskTexId;
        if (texId >= 0) {
            float alpha = 1.0f;
            if (mask.phase == Phase::SPAWN) {
                alpha = mask.phaseTimer / SPAWN_TIME;
            } else if (mask.phase == Phase::DISAPPEAR) {
                alpha = 1.0f - mask.phaseTimer / DISAPPEAR_TIME;
            }

            float drawAngle = mask.isLeft ? -1.5708f : 1.5708f;

            Sprite_ScrollDraw(texId,
                mask.pos.x, mask.pos.y,
                m_maskSize.x, m_maskSize.y,
                0.0f, 0.0f, 512.0f, 512.0f,
                drawAngle,
                { 1.0f, 1.0f, 1.0f, alpha });
        }
    }
}

void SpecialBeamAttack::Reset() {
    RemoveBeamProjectiles();
    ClearAllProjectiles();

    for (int i = 0; i < 2; ++i) {
        m_masks[i].isActive = false;
        m_beams[i] = nullptr;
        if (m_beamAnimPlayers[i]) {
            delete m_beamAnimPlayers[i];
            m_beamAnimPlayers[i] = nullptr;
        }
    }

    if (m_beamAnim) {
        delete m_beamAnim;
        m_beamAnim = nullptr;
    }

    m_finished = false;
    m_timer = 0.0f;
    m_currentRound = 0;
    m_waitingForNextRound = false;
    m_roundGapTimer = 0.0f;
    m_boss = nullptr;
    m_target = nullptr;
}

void SpecialBeamAttack::DrawAimLine(float cx, float cy, float angle, float alpha) {
    if (m_aimLineTexId < 0) return;

    float endX = cx + cosf(angle) * BEAM_LENGTH;
    float endY = cy + sinf(angle) * BEAM_LENGTH;

    float lineWidth = 28.0f;
    float halfW = lineWidth * 0.5f;

    int segments = 50;
    for (int s = 0; s < segments; ++s) {
        float t0 = static_cast<float>(s) / segments;
        float t1 = static_cast<float>(s + 1) / segments;

        float x0 = cx + (endX - cx) * t0;
        float y0 = cy + (endY - cy) * t0;
        float x1 = cx + (endX - cx) * t1;
        float y1 = cy + (endY - cy) * t1;

        float segCX = (x0 + x1) * 0.5f;
        float segCY = (y0 + y1) * 0.5f;
        float segLen = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));

        Sprite_ScrollDraw(m_aimLineTexId,
            segCX - segLen * 0.5f, segCY - halfW,
            segLen, lineWidth,
            0.0f, 0.0f, 1024.0f, 1024.0f,
            angle,
            { 1.0f, 1.0f, 1.0f, alpha });
    }
}



//==============================================================================
// 大技②: SpecialBulletStormAttack - お面弾乱れうち
//==============================================================================
void SpecialBulletStormAttack::Start(Boss* boss, Player* target) {
    m_boss = boss;
    m_target = target;
    m_finished = false;
    m_timer = 0.0f;
    m_currentWave = 0;
    m_waveTimer = 0.0f;
    m_bulletsSpawnedInWave = 0;

    if (m_maskTexId < 0) m_maskTexId = TextureManager::Instance().Get(TexID::BOSS_MASK_BULLET);
    if (m_warningTexId < 0) m_warningTexId = TextureManager::Instance().Get(TexID::BOSS_WARNING);

    m_hardModeScale = GameManager::Instance().IsHardMode() ? 0.7f : 1.0f;

    GenerateWavePattern(0);

#ifdef _DEBUG
    OutputDebugStringA("SpecialBulletStormAttack: Started\n");
#endif
}

void SpecialBulletStormAttack::GenerateWavePattern(int waveIndex) {
    m_spawnPoints.clear();
    m_warningPositions.clear();

    float stageLeft = 64.0f;
    float stageRight = m_stageWidth - 64.0f;
    float stageTop = 64.0f;
    float stageBottom = m_stageHeight - 128.0f;  // 下64.0f余裕

    // プレイヤーの中心座標を取得
    float playerCenterX = m_stageWidth * 0.5f;
    float playerCenterY = m_stageHeight * 0.5f;
    if (m_target) {
        playerCenterX = m_target->GetPosition().x + m_target->GetSize().x * 0.5f;
        playerCenterY = m_target->GetPosition().y + m_target->GetSize().y * 0.5f;
    }

    // プレイヤーY座標をステージ下限で制限
    if (playerCenterY > stageBottom) {
        playerCenterY = stageBottom;
    }

    switch (waveIndex % 4) {  // 3 → 4に変更
    case 0:
    {
        // 左右からプレイヤーを中心に挟み撃ち
        float spreadY = 200.0f;
        for (int i = 0; i < BULLETS_PER_WAVE; ++i) {
            SpawnPoint point;
            bool fromLeft = (i % 2 == 0);
            point.pos.x = fromLeft ? stageLeft : stageRight - m_bulletSize.x;

            float t = (static_cast<float>(i) / (BULLETS_PER_WAVE - 1)) - 0.5f;
            float spawnY = playerCenterY + t * spreadY * 2.0f;

            if (spawnY > stageBottom - m_bulletSize.y) {
                spawnY = stageBottom - m_bulletSize.y;
            }
            if (spawnY < stageTop) {
                spawnY = stageTop;
            }

            point.pos.y = spawnY;

            float dx = playerCenterX - point.pos.x;
            float dy = playerCenterY - point.pos.y;
            point.angle = atan2f(dy, dx);

            point.showWarning = true;
            m_spawnPoints.push_back(point);
            m_warningPositions.push_back(point.pos);
        }
        break;
    }

    case 1:
    {
        // 上からプレイヤーを中心に降ってくる
        float spreadX = 300.0f;
        for (int i = 0; i < BULLETS_PER_WAVE; ++i) {
            SpawnPoint point;

            float t = (static_cast<float>(i) / (BULLETS_PER_WAVE - 1)) - 0.5f;
            float spawnX = playerCenterX + t * spreadX * 2.0f;

            if (spawnX < stageLeft) spawnX = stageLeft;
            if (spawnX > stageRight - m_bulletSize.x) spawnX = stageRight - m_bulletSize.x;

            point.pos.x = spawnX;
            point.pos.y = -50.0f;

            float dx = playerCenterX - point.pos.x;
            float dy = playerCenterY - point.pos.y;
            float baseAngle = atan2f(dy, dx);
            float randomOffset = (static_cast<float>(rand() % 100) - 50.0f) / 100.0f * 0.3f;
            point.angle = baseAngle + randomOffset;

            point.showWarning = true;
            m_spawnPoints.push_back(point);

            float warningY = stageTop;
            if (warningY > stageBottom) warningY = stageBottom;
            m_warningPositions.push_back({ spawnX, warningY });
        }
        break;
    }

    case 2:
    {
        // プレイヤーを中心に円形から内側へ
        float radius = 500.0f;
        for (int i = 0; i < BULLETS_PER_WAVE; ++i) {
            SpawnPoint point;
            float angleStep = 2.0f * 3.14159f / BULLETS_PER_WAVE;
            float spawnAngle = angleStep * i;

            float spawnX = playerCenterX + cosf(spawnAngle) * radius;
            float spawnY = playerCenterY + sinf(spawnAngle) * radius;

            if (spawnY > stageBottom - m_bulletSize.y) {
                spawnY = stageBottom - m_bulletSize.y;
            }
            if (spawnY < -100.0f) spawnY = -100.0f;
            if (spawnX < stageLeft - 100.0f) spawnX = stageLeft - 100.0f;
            if (spawnX > stageRight + 100.0f) spawnX = stageRight + 100.0f;

            point.pos.x = spawnX;
            point.pos.y = spawnY;

            float dx = playerCenterX - spawnX;
            float dy = playerCenterY - spawnY;
            point.angle = atan2f(dy, dx);

            point.showWarning = true;
            m_spawnPoints.push_back(point);
            m_warningPositions.push_back(point.pos);
        }
        break;
    }

    case 3:
    {
        // X字クロス攻撃：プレイヤーを中心に4つの斜め方向から交差するように飛来
        float radius = 550.0f;
        // 4本の斜めライン（45°, 135°, 225°, 315°）
        float crossAngles[4] = {
            0.7854f,    // 45° (右下から左上へ → プレイヤーへ)
            2.3562f,    // 135° (左下から右上へ → プレイヤーへ)
            3.9270f,    // 225° (左上から右下へ → プレイヤーへ)
            5.4978f     // 315° (右上から左下へ → プレイヤーへ)
        };

        int bulletsPerLine = BULLETS_PER_WAVE / 4;  // 各ライン3発
        int remainder = BULLETS_PER_WAVE % 4;       // 余り分

        int bulletIndex = 0;
        for (int line = 0; line < 4; ++line) {
            int count = bulletsPerLine + (line < remainder ? 1 : 0);

            for (int j = 0; j < count; ++j) {
                SpawnPoint point;

                // ライン上で距離をずらして配置（近い弾と遠い弾）
                float distOffset = radius + static_cast<float>(j) * 80.0f;

                float spawnX = playerCenterX + cosf(crossAngles[line]) * distOffset;
                float spawnY = playerCenterY + sinf(crossAngles[line]) * distOffset;

                // ステージ下限制限
                if (spawnY > stageBottom - m_bulletSize.y) {
                    spawnY = stageBottom - m_bulletSize.y;
                }
                if (spawnY < -100.0f) spawnY = -100.0f;
                if (spawnX < stageLeft - 100.0f) spawnX = stageLeft - 100.0f;
                if (spawnX > stageRight + 100.0f) spawnX = stageRight + 100.0f;

                point.pos.x = spawnX;
                point.pos.y = spawnY;

                // プレイヤーに向かう角度（スポーン位置の逆方向＝中心へ）
                float dx = playerCenterX - spawnX;
                float dy = playerCenterY - spawnY;
                point.angle = atan2f(dy, dx);

                point.showWarning = true;
                m_spawnPoints.push_back(point);
                m_warningPositions.push_back(point.pos);

                bulletIndex++;
            }
        }
        break;
    }
    }
}


void SpecialBulletStormAttack::SpawnBullet(const SpawnPoint& point) {
    bool fromLeft = (point.pos.x < m_stageWidth * 0.5f);

    auto* bullet = SpawnProjectile<MaskBulletProjectile>(
        point.pos, m_bulletSize, m_boss, m_target, fromLeft, m_stageWidth);

    bullet->SetChargeDuration(0.0f);
    bullet->SetBulletSpeed(BULLET_SPEED);
    bullet->SetMoveAngle(point.angle);
    bullet->SetTextureId(m_maskTexId);
    bullet->SetStageBounds(64.0f, m_stageWidth - 64.0f, m_stageHeight - 64.0f);
    bullet->Fire();
}

void SpecialBulletStormAttack::Update(float deltaTime) {
    if (m_finished) return;

    m_timer += deltaTime;
    m_waveTimer += deltaTime;

    float chargeTime = CHARGE_TIME * m_hardModeScale;

    // チャレンジモードならチャージ時間を短縮
    if (m_waveTimer < chargeTime) {
        m_warningAlpha = (sinf(m_waveTimer * 10.0f) + 1.0f) * 0.5f;
    } else {
        float spawnTime = m_waveTimer - chargeTime;
        int shouldSpawn = static_cast<int>(spawnTime / BULLET_SPAWN_INTERVAL);

        while (m_bulletsSpawnedInWave < shouldSpawn &&
            m_bulletsSpawnedInWave < static_cast<int>(m_spawnPoints.size())) {
            SpawnBullet(m_spawnPoints[m_bulletsSpawnedInWave]);
            m_bulletsSpawnedInWave++;
        }

        if (m_bulletsSpawnedInWave >= static_cast<int>(m_spawnPoints.size())) {
            float waveEndTime = chargeTime + BULLET_SPAWN_INTERVAL * m_spawnPoints.size() + 1.0f;

            if (m_waveTimer >= waveEndTime) {
                m_currentWave++;
                if (m_currentWave >= WAVE_COUNT) {
                    if (m_projectiles.empty()) {
                        m_finished = true;
                    }
                } else {
                    m_waveTimer = 0.0f;
                    m_bulletsSpawnedInWave = 0;
                    GenerateWavePattern(m_currentWave);
                }
            }
        }
    }
    
    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Update();
        }
    }

    CleanupProjectiles();
}

void SpecialBulletStormAttack::DrawWarnings() {
    if (m_warningTexId < 0) return;
    if (m_waveTimer >= CHARGE_TIME) return;

    for (const auto& pos : m_warningPositions) {
        Sprite_ScrollDraw(m_warningTexId,
            pos.x - 16.0f, pos.y - 16.0f,
            32.0f, 32.0f,
            0.0f, 0.0f, 1024.0f, 1024.0f,
            0.0f,
            { 1.0f, 0.3f, 0.3f, m_warningAlpha });
    }
}

void SpecialBulletStormAttack::Draw() {
    DrawWarnings();

    for (auto* proj : m_projectiles) {
        if (proj && proj->IsActive()) {
            proj->Draw();
        }
    }
}

void SpecialBulletStormAttack::Reset() {
    ClearAllProjectiles();
    m_spawnPoints.clear();
    m_warningPositions.clear();
    m_finished = false;
    m_timer = 0.0f;
    m_currentWave = 0;
    m_waveTimer = 0.0f;
    m_bulletsSpawnedInWave = 0;
    m_boss = nullptr;
    m_target = nullptr;
}
