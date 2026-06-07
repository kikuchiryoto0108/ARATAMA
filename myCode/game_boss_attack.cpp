/*********************************************************************
 * \file   game_boss_attack.cpp
 * \brief  ボスの攻撃マネージャー
 *
 * \author Ryoto Kikuchi
 * \date   2026/2/14
 *********************************************************************/
#include "game_boss_attack.h"
#include "game_boss.h"
#include "game_player.h"
#include "CollisionManager.h"

#ifdef _DEBUG
#include <Windows.h>
#include <string>
#endif

//==============================================================================
// BossProjectile - ボス攻撃弾の基底クラス
//==============================================================================
BossProjectile::BossProjectile(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size, Boss* boss, Player* target)
    : GameObject(pos, size, ObjectTag::BOSS_PROJECTILE)
    , m_boss(boss)
    , m_target(target) {
}

BossProjectile::~BossProjectile() {
}

void BossProjectile::Initialize() {
    GetColliders().clear();
    AddCollider(GetPosition(), GetSize(), ColliderType::BOSS_ATTACK);
    CollisionManager::GetInstance().AddObject(this);
}

void BossProjectile::Finalize() {
    CollisionManager::GetInstance().RemoveObject(this);
}

void BossProjectile::OnCollision(const GameObject* otherObject, const Collider* myCollider,
    const Collider* otherCollider, COLLISION_VEC hitVec) {
    if (!otherObject || !otherCollider) return;

    // プレイヤーとの衝突
    if (otherObject->GetTag() == ObjectTag::PLAYER) {
        if (otherCollider->GetType() == ColliderType::BODY) {
            ApplyDamageToPlayer();

#ifdef _DEBUG
            OutputDebugStringA("BossProjectile: Hit player!\n");
#endif
        }
    }
}

void BossProjectile::UpdateColliderPosition() {
    if (!GetColliders().empty()) {
        GetColliders()[0].SetColliderPos(GetPosition());
        GetColliders()[0].SetColliderSize(GetSize());
    }
}

void BossProjectile::ApplyDamageToPlayer() {
    if (m_target) {
        m_target->BossAttacksHit();
    }
}

void BossProjectile::ApplyKnockbackToPlayer(float dirX, float force) {
    if (m_target) {
        m_target->ApplyImpulseX(force * dirX);
    }
}


//==============================================================================
// フェード処理
//==============================================================================
void BossProjectile::StartFadeIn() {
    m_isFadingIn = true;
    m_isFadingOut = false;
    m_fadeTimer = 0.0f;
    m_alpha = 0.0f;
}

void BossProjectile::StartFadeOut() {
    m_isFadingIn = false;
    m_isFadingOut = true;
    m_fadeTimer = 0.0f;
    // 現在のalphaから開始（途中でフェードアウト開始する場合も対応）
}

void BossProjectile::UpdateFade(float deltaTime) {
    if (m_isFadingIn) {
        m_fadeTimer += deltaTime;
        if (m_fadeTimer >= m_fadeInDuration) {
            m_alpha = 1.0f;
            m_isFadingIn = false;
        } else {
            m_alpha = m_fadeTimer / m_fadeInDuration;
        }
    } else if (m_isFadingOut) {
        m_fadeTimer += deltaTime;
        if (m_fadeTimer >= m_fadeOutDuration) {
            m_alpha = 0.0f;
            // フェードアウト完了
        } else {
            m_alpha = 1.0f - (m_fadeTimer / m_fadeOutDuration);
        }
    }
}

//==============================================================================
// IBossAttack - Projectileクリーンアップ
//==============================================================================
void IBossAttack::CleanupProjectiles() {
    for (auto it = m_projectiles.begin(); it != m_projectiles.end(); ) {
        if ((*it)->ShouldDelete() || !(*it)->IsActive()) {
            (*it)->Finalize();
            delete* it;
            it = m_projectiles.erase(it);
        } else {
            ++it;
        }
    }
}

void IBossAttack::ClearAllProjectiles() {
    for (auto* proj : m_projectiles) {
        if (proj) {
            proj->Finalize();
            delete proj;
        }
    }
    m_projectiles.clear();
}

//==============================================================================
// BossAttackManager
//==============================================================================
BossAttackManager::BossAttackManager()
    : m_rng(std::random_device{}()) {
}

BossAttackManager::~BossAttackManager() {
    Finalize();
}

void BossAttackManager::Initialize(Boss* boss, Player* player) {
    m_boss = boss;
    m_player = player;
    m_isActive = true;
    m_attackCooldown = 2.0f;  // 開始時のクールダウン
    m_lastIndex = -1;
    m_activeAttack = nullptr;
}

void BossAttackManager::Finalize() {
    StopCurrentAttack();
    ClearAttacks();
    m_boss = nullptr;
    m_player = nullptr;
    m_isActive = false;
}

void BossAttackManager::Update(float deltaTime) {
    if (!m_isActive) return;

    // クールダウン中
    if (m_attackCooldown > 0.0f) {
        m_attackCooldown -= deltaTime;
        if (m_attackCooldown < 0.0f) {
            m_attackCooldown = 0.0f;
        }
        return;
    }

    // 攻撃実行中
    if (m_activeAttack) {
        m_activeAttack->Update(deltaTime);

        // 攻撃終了チェック
        if (m_activeAttack->IsFinished()) {
            OnAttackFinished();
        }
    } else {
        // 攻撃していない場合、ランダムで攻撃を開始
        if (!m_attacks.empty()) {
            StartRandomAttack();
        }
    }
}

void BossAttackManager::Draw() {
    if (m_activeAttack) {
        m_activeAttack->Draw();
    }
}

//==============================================================================
// 攻撃の管理
//==============================================================================
void BossAttackManager::AddAttack(IBossAttack* attack) {
    if (attack) {
        attack->SetStageInfo(m_stageWidth, m_stageHeight);
        m_attacks.push_back(attack);

#ifdef _DEBUG
        OutputDebugStringA(("BossAttackManager: Added attack - " +
            std::string(attack->GetName()) + "\n").c_str());
#endif
    }
}

void BossAttackManager::RemoveAttack(int id) {
    for (auto it = m_attacks.begin(); it != m_attacks.end(); ++it) {
        if ((*it)->GetId() == id) {
            // 実行中の攻撃なら停止
            if (m_activeAttack == *it) {
                StopCurrentAttack();
            }

            (*it)->Reset();
            delete* it;
            m_attacks.erase(it);

            // lastIndexを調整
            if (m_lastIndex >= static_cast<int>(m_attacks.size())) {
                m_lastIndex = -1;
            }

#ifdef _DEBUG
            OutputDebugStringA(("BossAttackManager: Removed attack ID " +
                std::to_string(id) + "\n").c_str());
#endif
            return;
        }
    }
}

void BossAttackManager::RemoveAttackAt(int index) {
    if (index < 0 || index >= static_cast<int>(m_attacks.size())) return;

    // 実行中の攻撃なら停止
    if (m_activeAttack == m_attacks[index]) {
        StopCurrentAttack();
    }

    m_attacks[index]->Reset();
    delete m_attacks[index];
    m_attacks.erase(m_attacks.begin() + index);

    // lastIndexを調整
    if (m_lastIndex >= static_cast<int>(m_attacks.size())) {
        m_lastIndex = -1;
    }
}

void BossAttackManager::ClearAttacks() {
    for (auto* attack : m_attacks) {
        if (attack) {
            attack->Reset();
            delete attack;
        }
    }
    m_attacks.clear();
    m_lastIndex = -1;
    m_activeAttack = nullptr;
}

//==============================================================================
// 攻撃の開始・停止
//==============================================================================
int BossAttackManager::GetRandomIndex() {
    int count = static_cast<int>(m_attacks.size());
    if (count <= 0) return -1;
    if (count == 1) return 0;

    // 前回と違うものをランダムに
    std::uniform_int_distribution<int> dist(0, count - 2);
    int randIndex = dist(m_rng);

    if (m_lastIndex >= 0 && randIndex >= m_lastIndex) {
        randIndex++;
    }

    return randIndex;
}

void BossAttackManager::StartRandomAttack() {
    if (m_attacks.empty()) return;
    if (m_activeAttack) return;  // 既に攻撃中

    int index = GetRandomIndex();
    if (index < 0) return;

    m_lastIndex = index;
    m_activeAttack = m_attacks[index];

    if (m_activeAttack) {
        m_activeAttack->Start(m_boss, m_player);

#ifdef _DEBUG
        OutputDebugStringA(("BossAttackManager: Starting random attack [" +
            std::to_string(index) + "] - " +
            std::string(m_activeAttack->GetName()) + "\n").c_str());
#endif
    }
}

void BossAttackManager::StopCurrentAttack() {
    if (m_activeAttack) {
        m_activeAttack->Reset();
        m_activeAttack = nullptr;

#ifdef _DEBUG
        OutputDebugStringA("BossAttackManager: Attack stopped\n");
#endif
    }
}

void BossAttackManager::OnAttackFinished() {
    if (!m_activeAttack) return;

#ifdef _DEBUG
    OutputDebugStringA(("BossAttackManager: Attack finished - " +
        std::string(m_activeAttack->GetName()) + "\n").c_str());
#endif

    m_activeAttack->Reset();
    m_activeAttack = nullptr;

    // クールダウン設定
    m_attackCooldown = m_cooldownTime * m_timeScale;
}

// ステージサイズ設定
void BossAttackManager::SetStageSize(float width, float height) {
    m_stageWidth = width;
    m_stageHeight = height;

    // 既存の攻撃にも反映
    for (auto* attack : m_attacks) {
        if (attack) {
            attack->SetStageInfo(width, height);
        }
    }
}
