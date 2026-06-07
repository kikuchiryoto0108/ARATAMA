/*********************************************************************
 * \file   game_boss_state.h
 * \brief  ボスのステート管理
 *
 * \author Ryoto Kikuchi
 * \date   2026/2/14
 *********************************************************************/
#ifndef GAME_BOSS_STATE_H
#define GAME_BOSS_STATE_H

#include <DirectXMath.h> 

class Boss;
class BossAttackManager;
class Stage;
class Player;
class Camera;

//==============================================================================
// IBossState - ボス状態インターフェース
//==============================================================================
class IBossState {
public:
    virtual ~IBossState() = default;

    virtual void Enter(Boss* boss) = 0;     // 入った瞬間
    virtual void Update(Boss* boss, float deltaTime) = 0;
    virtual void Draw(Boss* boss) = 0;
    virtual void Exit(Boss* boss) = 0;      // 出る瞬間
    virtual void OnDamage(Boss* boss, int damage) = 0;

    virtual bool CanTakeDamage() const = 0;
    virtual const char* GetStateName() const = 0;
};

//==============================================================================
// BossWaitState - 待機状態（壁が消えるまで何もしない）
//==============================================================================
class BossWaitState : public IBossState {
private:
    class Stage* m_stage{ nullptr };
    class Boss* m_boss{ nullptr };
    bool m_wallsDestroyed{ false };         // 壁が消えたか
    bool m_playerEnteredBossRoom{ false };  // プレイヤーがボス部屋に入ったか
    float m_wallPositionX{ 0.0f };          // 壁のX座標（判定用）

public:
    BossWaitState(Stage* stage) : m_stage(stage) {}

    void Enter(Boss* boss) override;
    void Update(Boss* boss, float deltaTime) override;
    void Draw(Boss* boss) override;
    void Exit(Boss* boss) override;
    void OnDamage(Boss* boss, int damage) override;

    bool CanTakeDamage() const override { return false; }
    const char* GetStateName() const override { return "Wait"; }

private:
    bool AreAllWallsDestroyed() const;
    bool HasAnyWalls() const;   // 壁あるかチェック
    float GetWallPositionX() const;
    bool HasPlayerPassedWall() const;
    void SpawnBlocksAtWallPositions();
};

//==============================================================================
// BossNormalState - 通常攻撃状態
//==============================================================================
class BossNormalState : public IBossState {
private:
    float m_stateTimer{ 0.0f };
    int m_damageAccumulated{ 0 };

    static constexpr float STATE_DURATION = 12.0f;    // この時間経過で必殺溜め状態
    static constexpr int DAMAGE_THRESHOLD = 36;      // このダメージで必殺溜め状態

public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float deltaTime) override;
    void Draw(Boss* boss) override;
    void Exit(Boss* boss) override;
    void OnDamage(Boss* boss, int damage) override;

    bool CanTakeDamage() const override { return true; }
    const char* GetStateName() const override { return "Normal"; }
};

//==============================================================================
// BossChargeState - 必殺溜め状態（バリア）
//==============================================================================
class BossChargeState : public IBossState {
private:
    float m_stateTimer{ 0.0f };

    static constexpr float CHARGE_DURATION = 10.0f;    // 仮面を壊す猶予時間

public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float deltaTime) override;
    void Draw(Boss* boss) override;
    void Exit(Boss* boss) override;
    void OnDamage(Boss* boss, int damage) override;

    bool CanTakeDamage() const override { return false; }  // バリア中
    const char* GetStateName() const override { return "Charge"; }

    float GetStateTime() const { return m_stateTimer; }
    float GetChargeDuration() const;
};

//==============================================================================
// BossSpecialState - 必殺技状態
//==============================================================================
class BossSpecialState : public IBossState {
private:
    float m_stateTimer{ 0.0f };
    bool m_hasAttacked{ false };

    static constexpr float ATTACK_DURATION = 2.0f;

public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float deltaTime) override;
    void Draw(Boss* boss) override;
    void Exit(Boss* boss) override;
    void OnDamage(Boss* boss, int damage) override;

    bool CanTakeDamage() const override { return false; }
    const char* GetStateName() const override { return "Special"; }
};

//==============================================================================
// BossStunnedState - スタン状態（被ダメージ増）
//==============================================================================
class BossStunnedState : public IBossState {
private:
    float m_stateTimer{ 0.0f };

    static constexpr float STUN_DURATION = 5.0f;        // スタン時間
    static constexpr float DAMAGE_MULTIPLIER = 3.0f;    // スタン中のダメージ倍率

public:
    void Enter(Boss* boss) override;
    void Update(Boss* boss, float deltaTime) override;
    void Draw(Boss* boss) override;
    void Exit(Boss* boss) override;
    void OnDamage(Boss* boss, int damage) override;

    bool CanTakeDamage() const override { return true; }
    const char* GetStateName() const override { return "Stunned"; }
};

//==============================================================================
// BossDefeatedState - 撃破演出状態
//==============================================================================
class BossDefeatedState : public IBossState {
public:
    enum class Phase {
        STAGGER,        // ボスよろめき
        PLAYER_MOVE,    // プレイヤーがボスに向かって移動
        POSSESS_FLASH,  // 憑依フラッシュ
        SCREAM,         // ボスが叫ぶ
        FADE_OUT,       // フェードアウト
        DOOR_CLOSE,     // 扉が閉じる演出
        DONE            // 完了
    };

private:
    Phase m_phase{ Phase::STAGGER };
    float m_phaseTimer{ 0.0f };
    float m_totalTimer{ 0.0f };

    // プレイヤー移動用
    DirectX::XMFLOAT2 m_playerStartPos{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_targetPos{ 0.0f, 0.0f };

    // フラッシュ
    float m_flashAlpha{ 0.0f };

    // プレイヤー表示制御
    bool m_playerHidden{ false };

    // テクスチャ
    int m_whiteTexId{ -1 };

    // 各フェーズ時間
    static constexpr float STAGGER_TIME = 3.0f;
    static constexpr float PLAYER_MOVE_TIME = 4.5f;
    static constexpr float POSSESS_FLASH_TIME = 5.0f;
    static constexpr float SCREAM_TIME = 6.0f;
    static constexpr float FADE_OUT_TIME = 3.5f;

    Player* m_player{ nullptr };
    Camera* m_camera{ nullptr };

    // 扉演出用テクスチャ
    int m_doorRightTexId{ -1 };
    int m_doorLeftTexId{ -1 };

    // 扉演出用パラメータ
    float m_doorProgress{ 0.0f };           // 0.0（全開）→ 1.0（完全に閉じた）
    static constexpr float DOOR_HALF_WIDTH = 960.0f;
    static constexpr float DOOR_HEIGHT = 1080.0f;

    // DOOR_CLOSE_TIMEを既存の定数群に追加
    static constexpr float DOOR_CLOSE_TIME = 1.35f;

    // 魂移動演出用
    bool m_soulStarted{ false };

public:
    BossDefeatedState(Player* player, Camera* camera)
        : m_player(player), m_camera(camera) {
    }

    void Enter(Boss* boss) override;
    void Update(Boss* boss, float deltaTime) override;
    void Draw(Boss* boss) override;
    void Exit(Boss* boss) override;
    void OnDamage(Boss* boss, int damage) override;

    bool CanTakeDamage() const override { return false; }
    const char* GetStateName() const override { return "Defeated"; }

    Phase GetPhase() const { return m_phase; }
    bool IsDone() const { return m_phase == Phase::DONE; }

private:
    void UpdateStagger(Boss* boss, float dt);
    void UpdatePlayerMove(Boss* boss, float dt);
    void UpdatePossessFlash(Boss* boss, float dt);
    void UpdateScream(Boss* boss, float dt);
    void UpdateDoorClose(Boss* boss, float dt);
    void UpdateFadeOut(Boss* boss, float dt);

    float EaseInOut(float t);
};


#endif // GAME_BOSS_STATE_H
