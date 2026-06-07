//==============================================================================
// 敵AI [game_enemy_ai.h]
// Author : Ryoto Kikuchi
// Date   : 2025/12/23
//==============================================================================
#pragma once
#include <DirectXMath.h>
#include <random>

class Enemy;
class Stage;

// 敵の種類
enum class EnemyAIType {
    KATANA,
    KANABO,
    BOW,
    SPEAR
};

// AIの状態
enum class EnemyAIState {
    PATROL,     // パトロール
    CHASE,      // 追跡
    ATTACK,     // 攻撃
    COOLDOWN    // 攻撃後のクールダウン
};

class EnemyAI {
public:
    EnemyAI(Enemy* owner, Stage* stage, EnemyAIType type = EnemyAIType::KATANA);
    ~EnemyAI();

    void Update();

    // AIタイプ設定
    void SetAIType(EnemyAIType type);
    EnemyAIType GetAIType() const { return m_aiType; }

    // 状態取得
    EnemyAIState GetState() const { return m_state; }

    void ForceReverse() {
        m_patrolDir *= -1.0f;
        m_turnCooldown = 0.3;
        m_patrolTimer = 0.0;
    }

    // 攻撃中かどうか
    bool IsAttacking() const { return m_state == EnemyAIState::ATTACK; }

    // 攻撃方向取得
    float GetAttackDirection() const { return m_attackDir; }

private:
    Enemy* m_owner;
    Stage* m_stage;
    EnemyAIType m_aiType;
    EnemyAIState m_state{ EnemyAIState::PATROL };

    float m_patrolDir{ 1.0f };
    double m_patrolTimer{ 0.0 };
    double m_currentTurnTime{ 1.0 };
    float m_currentSpeed{ 128.0f };

    // 基本パラメータ
    float m_baseMoveSpeed{ 128.0f };
    double m_baseTurnInterval{ 1.0 };
    float m_stopChance{ 0.2f };

    // 追跡・攻撃パラメータ
    float m_detectRange{ 400.0f };
    float m_attackRange{ 80.0f };
    float m_keepDistance{ 60.0f };
    float m_chaseSpeed{ 200.0f };

    // 攻撃関連
    float m_attackDir{ 1.0f };
    double m_attackTimer{ 0.0 };
    double m_attackDuration{ 0.4 };
    double m_attackCooldown{ 2.0 };
    double m_cooldownTimer{ 0.0 };

    // ランダム用
    std::mt19937 m_rng;

    // 補助関数
    bool WillFallAhead() const;
    bool IsWallAhead() const;
    void DoPatrol(double elapsedTime);
    void DoChase(double elapsedTime);
    void DoAttack(double elapsedTime);
    void DoCooldown(double elapsedTime);
    void DecideNextMove();
    float RandomRange(float min, float max);

    double m_turnCooldown{ 0.0 };

    // プレイヤーとの距離計算
    float GetDistanceToPlayer() const;
    DirectX::XMFLOAT2 GetDirectionToPlayer() const;

    // レイキャスト：プレイヤーが見えるかどうか
    bool CanSeePlayer() const;
    bool RaycastToPlayer(DirectX::XMFLOAT2 start, DirectX::XMFLOAT2 end) const;
};
