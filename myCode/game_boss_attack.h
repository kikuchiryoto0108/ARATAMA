/*********************************************************************
 * \file   game_boss_attack.h
 * \brief  ボスの攻撃マネージャー
 *
 * \author Ryoto Kikuchi
 * \date   2026/2/14
 *********************************************************************/
#ifndef GAME_BOSS_ATTACK_H
#define GAME_BOSS_ATTACK_H

#include <vector>
#include <string>
#include <random>
#include <DirectXMath.h>
#include "collision.h"
#include "game_object.h"

class Boss;
class Player;
class GameObject;

//==============================================================================
// BossProjectile - ボス攻撃弾の基底クラス
//==============================================================================
class BossProjectile : public GameObject {
protected:
    Boss* m_boss{ nullptr };
    Player* m_target{ nullptr };
    int m_textureId{ -1 };
    int m_damage{ 1 };
    bool m_shouldDelete{ false };
    float m_rotation{ 0.0f };

    // フェード関連
    float m_alpha{ 1.0f };
    float m_fadeTimer{ 0.0f };
    float m_fadeInDuration{ 0.3f };
    float m_fadeOutDuration{ 0.3f };
    bool m_isFadingIn{ false };
    bool m_isFadingOut{ false };

public:
    BossProjectile(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size, Boss* boss, Player* target);
    virtual ~BossProjectile();

    void Initialize() override;
    void Finalize() override;
    void Update() override = 0;
    void Draw() override = 0;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override;

    // 削除フラグ
    bool ShouldDelete() const { return m_shouldDelete; }
    void MarkForDeletion() { m_shouldDelete = true; }

    // ダメージ設定
    void SetDamage(int damage) { m_damage = damage; }
    int GetDamage() const { return m_damage; }

    // 描画用
    void SetAlpha(float alpha) { m_alpha = alpha; }
    float GetAlpha() const { return m_alpha; }
    void SetRotation(float rot) { m_rotation = rot; }
    float GetRotation() const { return m_rotation; }

    // テクスチャ
    void SetTextureId(int id) { m_textureId = id; }
    int GetTextureId() const { return m_textureId; }

    // Position設定(publicラッパー)
    void SetProjectilePosition(DirectX::XMFLOAT2 pos) { SetPosition(pos); }
    void SetProjectileVelocity(DirectX::XMFLOAT2 vel) { SetVelocity(vel); }

    // コライダー更新
    void UpdateColliderPosition();

    // フェード関連
    void SetFadeInDuration(float dur) { m_fadeInDuration = dur; }
    void SetFadeOutDuration(float dur) { m_fadeOutDuration = dur; }
    float GetFadeInDuration() const { return m_fadeInDuration; }
    float GetFadeOutDuration() const { return m_fadeOutDuration; }

    void StartFadeIn();
    void StartFadeOut();
    bool IsFadingIn() const { return m_isFadingIn; }
    bool IsFadingOut() const { return m_isFadingOut; }
    bool IsFadeInComplete() const { return !m_isFadingIn && m_alpha >= 1.0f; }
    bool IsFadeOutComplete() const { return m_isFadingOut && m_alpha <= 0.0f; }

protected:
    // 派生クラスがUpdate内で呼ぶ
    void UpdateFade(float deltaTime);

    // 派生クラス用ヘルパー
    void ApplyDamageToPlayer();
    void ApplyKnockbackToPlayer(float dirX, float force = 500.0f);
};

//==============================================================================
// IBossAttack - ボス攻撃インターフェース
//==============================================================================
class IBossAttack {
public:
    virtual ~IBossAttack() = default;

    virtual void Start(Boss* boss, Player* target) = 0;  // 攻撃開始
    virtual void Update(float deltaTime) = 0;             // 毎フレーム更新
    virtual void Draw() = 0;                              // 描画
    virtual bool IsFinished() const = 0;                  // 攻撃終了したか
    virtual void Reset() = 0;                             // リセット

    virtual const char* GetName() const = 0;              // 攻撃名（デバッグ用）
    virtual int GetId() const = 0;                        // 攻撃ID

    // ステージ情報設定（BossAttackManagerから呼ばれる）
    virtual void SetStageInfo(float stageWidth, float stageHeight) {
        m_stageWidth = stageWidth;
        m_stageHeight = stageHeight;
        m_groundY = stageHeight - 64.0f;  // 地面は最下端から1マス上
    }

    // Projectile管理
    std::vector<BossProjectile*>& GetProjectiles() { return m_projectiles; }
    void CleanupProjectiles();
    void ClearAllProjectiles();

protected:
    float m_stageWidth{ 1920.0f };   // デフォルト値
    float m_stageHeight{ 1088.0f };  // デフォルト値（17マス）
    float m_groundY{ 1024.0f };      // デフォルト値

    // Projectileリスト
    std::vector<BossProjectile*> m_projectiles;

    // Projectile生成ヘルパー
    template<typename T, typename... Args>
    T* SpawnProjectile(Args&&... args) {
        T* proj = new T(std::forward<Args>(args)...);
        proj->Initialize();
        m_projectiles.push_back(proj);
        return proj;
    }
};

//==============================================================================
// BossAttackManager - ボス攻撃管理クラス
//==============================================================================
class BossAttackManager {
private:
    // 攻撃リスト（通常・必殺まとめて管理）
    std::vector<IBossAttack*> m_attacks;
    int m_lastIndex{ -1 };  // 前回選んだインデックス（連続防止）

    // 現在実行中の攻撃
    IBossAttack* m_activeAttack{ nullptr };

    // クールダウン
    float m_attackCooldown{ 0.0f };
    float m_cooldownTime{ 2.0f };    // 攻撃間のクールダウン

    // 参照
    Boss* m_boss{ nullptr };
    Player* m_player{ nullptr };

    // 状態
    bool m_isActive{ false };

    // 乱数生成
    std::mt19937 m_rng;

    // ステージ情報
    float m_stageWidth{ 1920.0f };
    float m_stageHeight{ 1088.0f };

	float m_timeScale{ 1.0f }; // 時間スケール(チャレンジ用)

public:
    BossAttackManager();
    ~BossAttackManager();

    // 初期化・終了
    void Initialize(Boss* boss, Player* player);
    void Finalize();

    // 更新・描画
    void Update(float deltaTime);
    void Draw();

    // 攻撃の追加・削除
    void AddAttack(IBossAttack* attack);
    void RemoveAttack(int id);
    void RemoveAttackAt(int index);
    void ClearAttacks();

    // 攻撃の開始（ランダム）
    void StartRandomAttack();           // ランダムで攻撃を開始
    void StopCurrentAttack();           // 現在の攻撃を停止

    // 状態取得
    bool IsAttacking() const { return m_activeAttack != nullptr; }
    bool IsActive() const { return m_isActive; }
    IBossAttack* GetActiveAttack() const { return m_activeAttack; }

    // クールダウン設定
    void SetCooldown(float time) { m_cooldownTime = time; }
    float GetCooldownRemaining() const { return m_attackCooldown; }

    // 攻撃数取得
    int GetAttackCount() const { return static_cast<int>(m_attacks.size()); }

    // 有効化・無効化
    void SetActive(bool active) { m_isActive = active; }

    // プレイヤー参照更新
    void SetPlayer(Player* player) { m_player = player; }

    // ステージサイズ設定
    void SetStageSize(float width, float height);

	// 時間スケール設定
    void SetTimeScale(float scale) { m_timeScale = scale; }
    float GetTimeScale() const { return m_timeScale; }

private:
    void OnAttackFinished();
    int GetRandomIndex();  // 前回と違うランダムインデックスを取得
};

//==============================================================================
// 攻撃ID定義（複数の攻撃用）
//==============================================================================
namespace BossAttackId {
    // 通常攻撃 (1 ~)
    constexpr int NORMAL_1 = 1;     // ギロチン落とし
    constexpr int NORMAL_2 = 2;     // 仮面弾（左右）
    constexpr int NORMAL_3 = 3;     // 仮面弾（斜め）
    constexpr int NORMAL_4 = 4;     // 仮面ビーム
    constexpr int NORMAL_5 = 5;     // 予備

    // 必殺 (100 ~)
    constexpr int SPECIAL_1 = 100;  // 必殺1
    constexpr int SPECIAL_2 = 101;  // 必殺2（要らなければ削除）
}

#endif // GAME_BOSS_ATTACK_H
