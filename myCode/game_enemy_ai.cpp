//==============================================================================
// 敵AI [game_enemy_ai.cpp]
// Author : Ryoto Kikuchi
// Date   : 2025/12/23
//==============================================================================
#include "game_enemy_ai.h"
#include "game_enemy.h"
#include "game_stage.h"
#include "time_manager.h"
#include <cmath>

using namespace DirectX;

EnemyAI::EnemyAI(Enemy* owner, Stage* stage, EnemyAIType type)
    : m_owner(owner), m_stage(stage), m_aiType(type) {

    std::random_device rd;
    m_rng.seed(rd() ^ reinterpret_cast<uintptr_t>(owner));

    SetAIType(type);
    DecideNextMove();

    m_patrolDir = (RandomRange(0.0f, 1.0f) > 0.5f) ? 1.0f : -1.0f;
}

EnemyAI::~EnemyAI() = default;

float EnemyAI::RandomRange(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_rng);
}

void EnemyAI::SetAIType(EnemyAIType type) {
    m_aiType = type;

    switch (type) {
    case EnemyAIType::KATANA:
        // === パトロール時のパラメータ ===
        m_baseMoveSpeed = 64.0f * 2.0f;   // パトロール時の基本移動速度（px/秒）
        m_baseTurnInterval = 1.0;          // パトロール時に方向転換するまでの基本時間（秒）
        m_stopChance = 0.2f;               // パトロール時にランダムで立ち止まる確率（20%）

        // === 検知・攻撃範囲 ===
        m_detectRange = 390.0f;            // プレイヤーを検知する範囲（この距離以内で追跡開始）
        m_attackRange = 280.0f;            // 攻撃を開始する範囲（この距離以内で攻撃）
        m_keepDistance = 70.0f;            // これ以上近づかない距離（近づきすぎ防止）

        // === 追跡時のパラメータ ===
        m_chaseSpeed = 250.0f;             // プレイヤー追跡時の移動速度（px/秒）

        // === 攻撃時のパラメータ ===
        m_attackDuration = 0.4;            // 攻撃モーションの継続時間（秒）
        m_attackCooldown = 2.0;            // 攻撃後の待機時間（次の攻撃までのクールダウン）
        break;

    case EnemyAIType::KANABO:
        // === パトロール時のパラメータ ===
        m_baseMoveSpeed = 64.0f * 1.2f;   // パトロール時の基本移動速度（px/秒）- 重いので遅め
        m_baseTurnInterval = 1.5;          // パトロール時に方向転換するまでの基本時間（秒）
        m_stopChance = 0.25f;              // パトロール時にランダムで立ち止まる確率（25%）- 重装備イメージ

        // === 検知・攻撃範囲 ===
        m_detectRange = 350.0f;            // プレイヤーを検知する範囲（この距離以内で追跡開始）
        m_attackRange = 280.0f;            // 攻撃を開始する範囲（この距離以内で攻撃）- 大振りで広い
        m_keepDistance = 100.0f;           // これ以上近づかない距離（振り回すスペース確保）

        // === 追跡時のパラメータ ===
        m_chaseSpeed = 180.0f;             // プレイヤー追跡時の移動速度（px/秒）- 遅い

        // === 攻撃時のパラメータ ===
        m_attackDuration = 0.8;            // 攻撃モーションの継続時間（秒）- 大振りで長い
        m_attackCooldown = 3.0;            // 攻撃後の待機時間（次の攻撃までのクールダウン）- 長い
        break;

    case EnemyAIType::BOW:
        // === パトロール時のパラメータ ===
        m_baseMoveSpeed = 64.0f;           // パトロール時の基本移動速度（px/秒）- 最も遅い
        m_baseTurnInterval = 2.0;          // パトロール時に方向転換するまでの基本時間（秒）
        m_stopChance = 0.25f;              // パトロール時にランダムで立ち止まる確率（25%）- 狙撃イメージ

        // === 検知・攻撃範囲 ===
        m_detectRange = 500.0f;            // プレイヤーを検知する範囲（この距離以内で追跡開始）- 最も広い
        m_attackRange = 400.0f;            // 攻撃を開始する範囲（この距離以内で攻撃）- 遠距離攻撃
        m_keepDistance = 200.0f;           // これ以上近づかない距離（距離を保つ）

        // === 追跡時のパラメータ ===
        m_chaseSpeed = 100.0f;             // プレイヤー追跡時の移動速度（px/秒）- あまり追わない

        // === 攻撃時のパラメータ ===
        m_attackDuration = 0.8;            // 攻撃モーションの継続時間（秒）- 弓を引く時間
        m_attackCooldown = 2.4;            // 攻撃後の待機時間（次の攻撃までのクールダウン）
        break;

    case EnemyAIType::SPEAR:
        // === パトロール時のパラメータ ===
        m_baseMoveSpeed = 64.0f * 1.8f;   // パトロール時の基本移動速度（px/秒）- 中程度
        m_baseTurnInterval = 1.2;          // パトロール時に方向転換するまでの基本時間（秒）
        m_stopChance = 0.15f;              // パトロール時にランダムで立ち止まる確率（15%）- 警戒姿勢

        // === 検知・攻撃範囲 ===
        m_detectRange = 540.0f;            // プレイヤーを検知する範囲（この距離以内で追跡開始）- 広め
        m_attackRange = 350.0f;            // 攻撃を開始する範囲（この距離以内で攻撃）- 槍のリーチ
        m_keepDistance = 120.0f;           // これ以上近づかない距離（適度な距離を保つ）

        // === 追跡時のパラメータ ===
        m_chaseSpeed = 220.0f;             // プレイヤー追跡時の移動速度（px/秒）- 中程度

        // === 攻撃時のパラメータ ===
        m_attackDuration = 0.5;            // 攻撃モーションの継続時間（秒）- 突きは素早い
        m_attackCooldown = 1.8;            // 攻撃後の待機時間（次の攻撃までのクールダウン）- 連続攻撃可能
        break;
    }

    // オーナーのデバッグ描画用範囲を更新
    if (m_owner) {
        m_owner->SetDetectRange(m_detectRange);
        m_owner->SetAttackRange(m_attackRange);
        m_owner->SetKeepDistance(m_keepDistance);
    }

    DecideNextMove();
}

void EnemyAI::DecideNextMove() {
    float timeVariation = RandomRange(0.5f, 1.5f);
    m_currentTurnTime = m_baseTurnInterval * timeVariation;

    float speedVariation = RandomRange(0.7f, 1.3f);
    m_currentSpeed = m_baseMoveSpeed * speedVariation;

    if (RandomRange(0.0f, 1.0f) < m_stopChance) {
        m_currentSpeed = 0.0f;
        m_currentTurnTime = RandomRange(0.3f, 0.8f);
    }
}

void EnemyAI::Update() {
    if (!m_owner || !m_stage) return;

    double elapsedTime = TimeManager::GetInstance().GetScaledElapsedTime();

    float distToPlayer = GetDistanceToPlayer();
    bool canSee = CanSeePlayer();

    // 状態遷移
    switch (m_state) {
    case EnemyAIState::PATROL:
        // プレイヤーが検知範囲内かつ視認可能なら追跡開始
        if (distToPlayer < m_detectRange && distToPlayer > 0.0f && canSee) {
            m_state = EnemyAIState::CHASE;
        }
        DoPatrol(elapsedTime);
        break;

    case EnemyAIState::CHASE:
        // プレイヤーが見えなくなったらパトロールに戻る
        if (!canSee) {
            m_state = EnemyAIState::PATROL;
            m_patrolTimer = 0.0;
            DecideNextMove();
        }
        // 攻撃範囲内かつ視認可能なら攻撃
        else if (distToPlayer < m_attackRange && distToPlayer > 0.0f) {
            m_state = EnemyAIState::ATTACK;
            m_attackTimer = 0.0;
            auto dir = GetDirectionToPlayer();
            m_attackDir = (dir.x >= 0.0f) ? 1.0f : -1.0f;
        }
        // 検知範囲外に出たらパトロールに戻る
        else if (distToPlayer > m_detectRange * 1.5f) {
            m_state = EnemyAIState::PATROL;
            m_patrolTimer = 0.0;
            DecideNextMove();
        }
        DoChase(elapsedTime);
        break;

    case EnemyAIState::ATTACK:
        DoAttack(elapsedTime);
        break;

    case EnemyAIState::COOLDOWN:
        DoCooldown(elapsedTime);
        break;
    }
}

void EnemyAI::DoPatrol(double elapsedTime) {
    m_patrolTimer += elapsedTime;

    if (m_turnCooldown > 0.0) {
        m_turnCooldown -= elapsedTime;
    }

    // 落下・壁チェックを毎フレーム行う（クールダウン無視）
    bool shouldTurn = false;

    if (WillFallAhead()) {
        shouldTurn = true;
    }

    if (IsWallAhead()) {
        shouldTurn = true;
    }

    if (m_patrolTimer >= m_currentTurnTime && m_turnCooldown <= 0.0) {
        shouldTurn = true;
    }

    if (shouldTurn) {
        m_patrolDir *= -1.0f;
        m_patrolTimer = 0.0;
        m_turnCooldown = 0.5;  // 少し長めに
        DecideNextMove();

        // 即停止して反転
        XMFLOAT2 vel = m_owner->GetVelocity();
        vel.x = 0.0f;
        m_owner->SetVelocity(vel);
        return;
    }

    XMFLOAT2 vel = m_owner->GetVelocity();
    vel.x = m_patrolDir * m_currentSpeed;
    m_owner->SetVelocity(vel);
}


void EnemyAI::DoChase(double elapsedTime) {
    auto dirToPlayer = GetDirectionToPlayer();
    float distToPlayer = GetDistanceToPlayer();

    float moveDir = (dirToPlayer.x >= 0.0f) ? 1.0f : -1.0f;

    XMFLOAT2 vel = m_owner->GetVelocity();

    // 近づきすぎたら後退
    if (distToPlayer < m_keepDistance) {
        vel.x = -moveDir * m_chaseSpeed * 0.3f;
    } else if (distToPlayer < m_attackRange) {
        vel.x = 0.0f;
    } else {
        vel.x = moveDir * m_chaseSpeed;
    }

    m_owner->SetVelocity(vel);

    // 崖があったら停止
    if (WillFallAhead()) {
        vel.x = 0.0f;
        m_owner->SetVelocity(vel);
    }
}

void EnemyAI::DoAttack(double elapsedTime) {
    m_attackTimer += elapsedTime;

    XMFLOAT2 vel = m_owner->GetVelocity();
    vel.x = 0.0f;
    m_owner->SetVelocity(vel);

    if (m_attackTimer >= m_attackDuration) {
        m_state = EnemyAIState::COOLDOWN;
        m_cooldownTimer = 0.0;
    }
}

void EnemyAI::DoCooldown(double elapsedTime) {
    m_cooldownTimer += elapsedTime;

    auto dirToPlayer = GetDirectionToPlayer();
    float moveDir = (dirToPlayer.x >= 0.0f) ? 1.0f : -1.0f;

    XMFLOAT2 vel = m_owner->GetVelocity();
    vel.x = -moveDir * m_baseMoveSpeed * 0.5f;

    if (WillFallAhead()) {
        vel.x = 0.0f;
    }
    m_owner->SetVelocity(vel);

    if (m_cooldownTimer >= m_attackCooldown) {
        float distToPlayer = GetDistanceToPlayer();
        bool canSee = CanSeePlayer();

        // プレイヤーが見えない場合はパトロールに戻る
        if (!canSee) {
            m_state = EnemyAIState::PATROL;
            m_patrolTimer = 0.0;
            DecideNextMove();
        } else if (distToPlayer < m_detectRange) {
            if (distToPlayer < m_attackRange) {
                m_state = EnemyAIState::ATTACK;
                m_attackTimer = 0.0;
                auto dir = GetDirectionToPlayer();
                m_attackDir = (dir.x >= 0.0f) ? 1.0f : -1.0f;
            } else {
                m_state = EnemyAIState::CHASE;
            }
        } else {
            m_state = EnemyAIState::PATROL;
            m_patrolTimer = 0.0;
            DecideNextMove();
        }
    }
}

bool EnemyAI::WillFallAhead() const {
    if (!m_owner || !m_stage) return false;

    auto pos = m_owner->GetPosition();
    auto size = m_owner->GetSize();

    float checkY = pos.y + size.y + 4.0f;

    float checkDir = m_patrolDir;
    if (m_state == EnemyAIState::CHASE || m_state == EnemyAIState::COOLDOWN) {
        auto dir = GetDirectionToPlayer();
        checkDir = (dir.x >= 0.0f) ? 1.0f : -1.0f;
        if (m_state == EnemyAIState::COOLDOWN) {
            checkDir *= -1.0f;
        }
    }

    // 進行方向の足元をチェック（敵の端）
    float checkX;
    if (checkDir > 0.0f) {
        // 右に進む → 右足元をチェック
        checkX = pos.x + size.x - 16.0f;
    } else {
        // 左に進む → 左足元をチェック
        checkX = pos.x + 16.0f;
    }

    // ブロック判定（すり抜けブロックも含む）
    bool hasGround = m_stage->IsBlockAt(checkX, checkY);

    // すり抜けブロックも足場として判定
    if (!hasGround) {
        hasGround = m_stage->IsSlipThroughBlockAt(checkX, checkY);
    }

    // ギミックも足場として判定
    if (!hasGround) {
        hasGround = m_stage->IsGimmickAt(checkX, checkY);
    }

    if (!hasGround) {
        return true;  // 足元に何もない→落ちる
    }

    return false;
}


bool EnemyAI::IsWallAhead() const {
    if (!m_owner || !m_stage) return false;

    auto pos = m_owner->GetPosition();
    auto size = m_owner->GetSize();

    float checkDir = m_patrolDir;
    if (m_state == EnemyAIState::CHASE) {
        auto dir = GetDirectionToPlayer();
        checkDir = (dir.x >= 0.0f) ? 1.0f : -1.0f;
    }

    float checkX;
    if (checkDir > 0.0f) {
        checkX = pos.x + size.x + 16.0f;  // 右向き：右端から先
    } else {
        checkX = pos.x - 16.0f;            // 左向き：左端から先
    }
    float checkY = pos.y + size.y * 0.5f;

    return m_stage->IsObstacleAt(checkX, checkY);
}

float EnemyAI::GetDistanceToPlayer() const {
    if (!m_owner) return FLT_MAX;

    XMFLOAT2 playerPos = m_owner->GetPlayerPos();
    XMFLOAT2 enemyPos = m_owner->GetPosition();
    XMFLOAT2 enemySize = m_owner->GetSize();

    // 敵の中心から計算
    float enemyCenterX = enemyPos.x + enemySize.x * 0.5f;
    float enemyCenterY = enemyPos.y + enemySize.y * 0.5f;

    // プレイヤーの中心（サイズは64x64と仮定）
    float playerCenterX = playerPos.x + 32.0f;
    float playerCenterY = playerPos.y + 32.0f;

    float dx = playerCenterX - enemyCenterX;
    float dy = playerCenterY - enemyCenterY;

    return sqrtf(dx * dx + dy * dy);
}

DirectX::XMFLOAT2 EnemyAI::GetDirectionToPlayer() const {
    if (!m_owner) return { 0.0f, 0.0f };

    XMFLOAT2 playerPos = m_owner->GetPlayerPos();
    XMFLOAT2 enemyPos = m_owner->GetPosition();
    XMFLOAT2 enemySize = m_owner->GetSize();

    float enemyCenterX = enemyPos.x + enemySize.x * 0.5f;
    float enemyCenterY = enemyPos.y + enemySize.y * 0.5f;

    float playerCenterX = playerPos.x + 32.0f;
    float playerCenterY = playerPos.y + 32.0f;

    float dx = playerCenterX - enemyCenterX;
    float dy = playerCenterY - enemyCenterY;
    float length = sqrtf(dx * dx + dy * dy);

    if (length < 0.001f) return { 0.0f, 0.0f };

    return { dx / length, dy / length };
}

//==============================================================================
// レイキャスト：プレイヤーが見えるかどうか
//==============================================================================
bool EnemyAI::CanSeePlayer() const {
    if (!m_owner || !m_stage) return false;

    XMFLOAT2 playerPos = m_owner->GetPlayerPos();
    XMFLOAT2 enemyPos = m_owner->GetPosition();
    XMFLOAT2 enemySize = m_owner->GetSize();

    // 敵の中心
    XMFLOAT2 start = {
        enemyPos.x + enemySize.x * 0.5f,
        enemyPos.y + enemySize.y * 0.5f
    };

    // プレイヤーの中心
    XMFLOAT2 end = {
        playerPos.x + 32.0f,
        playerPos.y + 32.0f
    };

    return RaycastToPlayer(start, end);
}

bool EnemyAI::RaycastToPlayer(DirectX::XMFLOAT2 start, DirectX::XMFLOAT2 end) const {
    if (!m_stage) return true;

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float distance = sqrtf(dx * dx + dy * dy);

    if (distance < 1.0f) return true;

    // レイの方向を正規化
    float dirX = dx / distance;
    float dirY = dy / distance;

    // ステップサイズ（ブロックサイズの半分程度）
    const float stepSize = 32.0f;
    int steps = static_cast<int>(distance / stepSize) + 1;

    for (int i = 1; i < steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        float checkX = start.x + dx * t;
        float checkY = start.y + dy * t;

        // ブロックがあるかチェック
        if (m_stage->IsBlockAt(checkX, checkY)) {
            return false;  // 障壁がある、見えない
        }

        // すり抜けブロックもチェック（追加）
        if (m_stage->IsSlipThroughBlockAt(checkX, checkY)) {
            return false;  // すり抜けブロックも視線を遮る
        }
    }

    return true;  // 障壁がない、見える
}
