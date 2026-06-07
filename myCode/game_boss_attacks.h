/*********************************************************************
 * \file   game_boss_attacks.h
 * \brief  ボスの各種攻撃
 *
 * \author Ryoto Kikuchi
 * \date   2026/2/14
 *********************************************************************/
#ifndef GAME_BOSS_ATTACKS_H
#define GAME_BOSS_ATTACKS_H

#include "game_boss_attack.h"
#include "game_object.h"
#include <DirectXMath.h>
#include <vector>
#include "sprite.h"
#include "sprite_anim.h"

class Boss;
class Player;

//==============================================================================
// GuillotineBladeProjectile - ギロチンの刃（Projectile）
//==============================================================================
class GuillotineBladeProjectile : public BossProjectile {
public:
    enum class State {
        WARNING,     // 警告表示中
        FALLING,     // 落下中
        LANDED,      // 着地済み（消滅待ち）
        INACTIVE     // 非アクティブ
    };

private:
    State m_state{ State::WARNING };
    float m_targetX{ 0.0f };
    float m_groundY{ 1024.0f - 64.0f };
    float m_fallSpeed{ 1500.0f };
    float m_velocityY{ 0.0f };
    float m_warningTimer{ 0.0f };
    float m_warningDuration{ 3.0f };
    float m_disappearTimer{ 0.0f };
    float m_disappearDuration{ 2.0f };
    float m_bladeWidth{ 470.0f };
    float m_bladeHeight{ 163.0f };
    int m_warningTexId{ -1 };
    bool m_hasHitPlayer{ false };

public:
    GuillotineBladeProjectile(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size,
        Boss* boss, Player* target, float targetX, float groundY);

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override;

    State GetState() const { return m_state; }
    void SetWarningDuration(float dur) { m_warningDuration = dur; }
    void SetFallSpeed(float speed) { m_fallSpeed = speed; }
    void SetDisappearDuration(float dur) { m_disappearDuration = dur; }
    void StartFalling();
};

//==============================================================================
// MaskBulletProjectile - 仮面弾（Projectile）
//==============================================================================
class MaskBulletProjectile : public BossProjectile {
public:
    enum class State {
        CHARGING,    // チャージ中
        FIRED,       // 発射済み
        LANDED,      // 消滅待ち
        INACTIVE     // 非アクティブ
    };

private:
    State m_state{ State::CHARGING };
    float m_chargeTimer{ 0.0f };
    float m_chargeDuration{ 0.8f };
    float m_bulletSpeed{ 800.0f };
    float m_stageWidth{ 1920.0f };  // マップからステージ幅取得してる
    float m_stageLeft{ 0.0f };      // 左境界
    float m_stageRight{ 1920.0f };  // 右境界
    float m_stageBottom{ 1080.0f }; // 下境界
    bool m_fromLeft{ true };
    bool m_hasHitPlayer{ false };
    float m_moveAngle{ 0.0f };  // 斜め移動用

public:
    MaskBulletProjectile(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size,
        Boss* boss, Player* target, bool fromLeft, float stageWidth);

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override;

    State GetState() const { return m_state; }
    void SetChargeDuration(float dur) { m_chargeDuration = dur; }
    void SetBulletSpeed(float speed) { m_bulletSpeed = speed; }
    void SetMoveAngle(float angle) { m_moveAngle = angle; }
    float GetMoveAngle() const { return m_moveAngle; }
    bool IsFromLeft() const { return m_fromLeft; }
    void Fire();

    // 境界設定
    void SetStageBounds(float left, float right, float bottom) {
        m_stageLeft = left;
        m_stageRight = right;
        m_stageBottom = bottom;
    }
};

//==============================================================================
// BeamHitboxProjectile - ビーム当たり判定（Projectile）
//==============================================================================
class BeamHitboxProjectile : public BossProjectile {
private:
    float m_beamAngle{ 0.0f };
    float m_beamLength{ 2000.0f };
    float m_beamWidth{ 32.0f }; // 当たり判定の幅
    float m_lifeTimer{ 0.0f };
    float m_lifeDuration{ 0.8f };
    bool m_hasHitPlayer{ false };
    DirectX::XMFLOAT2 m_originPos{ 0.0f, 0.0f };

    // 当たり判定用の分割数
    static constexpr int SEGMENT_COUNT = 20;

public:
    BeamHitboxProjectile(DirectX::XMFLOAT2 originPos, float angle,
        Boss* boss, Player* target, float length, float width);

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override;

    void SetLifeDuration(float dur) { m_lifeDuration = dur; }
    void SetBeamAngle(float angle) { m_beamAngle = angle; }
    float GetBeamAngle() const { return m_beamAngle; }
};

//==============================================================================
// ScratchProjectile - ひっかきエフェクト（Projectile）
//==============================================================================
class ScratchProjectile : public BossProjectile {
public:
    enum class State {
        READY,       // 最初のフレーム表示で待機（これが予告）
        ACTIVE,      // アニメーション再生中（当たり判定あり）
        INACTIVE     // 終了
    };

private:
    State m_state{ State::READY };
    float m_readyTimer{ 0.0f };
    float m_readyDuration{ 1.0f };
    bool m_hasHitPlayer{ false };

    // アニメーション手動管理
    int m_currentFrame{ 0 };
    float m_frameSpeed{ 0.03f };
    float m_frameTimer{ 0.0f };

    // テクスチャ情報
    static constexpr float SHEET_WIDTH = 512.0f;
    static constexpr float SHEET_HEIGHT = 512.0f;
    static constexpr int ANIM_COLS = 6;
    static constexpr int ANIM_ROWS = 4;
    static constexpr int ANIM_TOTAL = ANIM_COLS * ANIM_ROWS;  // 24
    static constexpr float FRAME_WIDTH = SHEET_WIDTH / 6.0f;
    static constexpr float FRAME_HEIGHT = SHEET_HEIGHT / 5.0f;

    float m_colliderScale{ 0.8f };

public:
    ScratchProjectile(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size,
        Boss* boss, Player* target);
    ~ScratchProjectile();

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override;

    void SetReadyDuration(float dur) { m_readyDuration = dur; }
    void SetFrameSpeed(float speed) { m_frameSpeed = speed; }
    State GetState() const { return m_state; }
};


//==============================================================================
// 攻撃①: GuillotineDropAttack - ギロチン落とし
//==============================================================================
class GuillotineDropAttack : public IBossAttack {
private:
    Boss* m_boss{ nullptr };
    Player* m_target{ nullptr };

    bool m_finished{ false };
    float m_timer{ 0.0f };
    int m_spawnedCount{ 0 };

    // パラメータ
    static constexpr int GUILLOTINE_COUNT = 6;           // ギロチン数
    static constexpr float SPAWN_INTERVAL = 0.5f;        // 出現間隔
    static constexpr float WARNING_TIME = 1.0f;          // 警告表示時間
    static constexpr float FALL_SPEED = 1000.0f;         // 落下速度
    static constexpr float DISAPPEAR_TIME = 0.2f;        // 着地から消えるまでの時間

    // テクスチャ
    int m_bladeTexId{ -1 };
    int m_warningTexId{ -1 };

    // 当たり判定用
    DirectX::XMFLOAT2 m_bladeSize{ 470.0f * 0.6f, 163.0f * 0.6f };

    // スポーン位置記録
    std::vector<float> m_spawnPositions;

    // 難易度によるスケール
    float m_hardModeScale{ 1.0f };

public:
    void Start(Boss* boss, Player* target) override;
    void Update(float deltaTime) override;
    void Draw() override;
    bool IsFinished() const override { return m_finished; }
    void Reset() override;

    const char* GetName() const override { return "GuillotineDrop"; }
    int GetId() const override { return BossAttackId::NORMAL_1; }
};

//==============================================================================
// 攻撃②: MaskBulletHorizontalAttack - 仮面弾（左右）
//==============================================================================
class MaskBulletHorizontalAttack : public IBossAttack {
private:
    Boss* m_boss{ nullptr };
    Player* m_target{ nullptr };

    bool m_finished{ false };
    float m_timer{ 0.0f };
    int m_spawnedCount{ 0 };

    // パラメータ
    static constexpr int MASK_COUNT = 6;                 // 仮面数
    static constexpr float SPAWN_INTERVAL = 0.3f;        // 出現間隔
    static constexpr float CHARGE_TIME = 0.8f;           // チャージ時間
    static constexpr float BULLET_SPEED = 800.0f;        // 弾速
    static constexpr float STAGE_MARGIN = 64.0f;           // ステージ境界マージン
    static constexpr float PLAYER_Y_OFFSET_RANGE = 100.0f; // プレイヤーY位置からのランダム範囲

    int m_maskTexId{ -1 };

    // 当たり判定用
    DirectX::XMFLOAT2 m_maskSize{ 64.0f * 2, 64.0f * 2 };

    // 難易度によるスケール
    float m_hardModeScale{ 1.0f };

public:
    void Start(Boss* boss, Player* target) override;
    void Update(float deltaTime) override;
    void Draw() override;
    bool IsFinished() const override { return m_finished; }
    void Reset() override;

    const char* GetName() const override { return "MaskBulletHorizontal"; }
    int GetId() const override { return BossAttackId::NORMAL_2; }
};

//==============================================================================
// 攻撃③: MaskBulletDiagonalAttack - 仮面弾（斜め）
//==============================================================================
class MaskBulletDiagonalAttack : public IBossAttack {
private:
    Boss* m_boss{ nullptr };
    Player* m_target{ nullptr };

    bool m_finished{ false };
    float m_timer{ 0.0f };
    int m_spawnedCount{ 0 };

    // パラメータ
    static constexpr int MASK_COUNT = 6;
    static constexpr float SPAWN_INTERVAL = 0.3f;
    static constexpr float CHARGE_TIME = 0.8f;
    static constexpr float BULLET_SPEED = 700.0f;
    static constexpr float DIAGONAL_ANGLE = 0.5236f;       // 30度（ラジアン）
    static constexpr float STAGE_MARGIN = 64.0f;           // ステージ境界マージン
    static constexpr float SPAWN_Y_BASE = -100.0f;         // Y出現基準位置
    static constexpr float SPAWN_Y_RANDOM_RANGE = 200.0f;  // Yランダム範囲

    int m_maskTexId{ -1 };
    DirectX::XMFLOAT2 m_maskSize{ 64.0f * 2, 64.0f * 2 };

    // 難易度によるスケール
    float m_hardModeScale{ 1.0f };

public:
    void Start(Boss* boss, Player* target) override;
    void Update(float deltaTime) override;
    void Draw() override;
    bool IsFinished() const override { return m_finished; }
    void Reset() override;

    const char* GetName() const override { return "MaskBulletDiagonal"; }
    int GetId() const override { return BossAttackId::NORMAL_3; }
};

//==============================================================================
// 攻撃④: MaskBeamAttack - 仮面からビーム
//==============================================================================
class MaskBeamAttack : public IBossAttack {
private:
    Boss* m_boss{ nullptr };
    Player* m_target{ nullptr };

    bool m_finished{ false };
    float m_timer{ 0.0f };

    // フェーズ管理
    enum class Phase {
        SPAWN,          // 仮面出現
        AIM,            // 照準（プレイヤー追尾）
        LOCK,           // 照準固定
        FIRE,           // 発射
        DISAPPEAR       // 消滅
    };

    struct BeamMask {
        DirectX::XMFLOAT2 pos;
        float angle;                 // 発射角度
        Phase phase;
        float phaseTimer;
        bool isActive;
    };

    BeamMask m_currentMask;
    int m_loopCount{ 0 };
    BeamHitboxProjectile* m_currentBeam{ nullptr };

    // ビームアニメーション
    AnimPattern* m_beamAnim{ nullptr };
    AnimPatternPlayer* m_beamAnimPlayer{ nullptr };

    // パラメータ
    static constexpr int MAX_LOOPS = 3;                 // ループ回数
    static constexpr float SPAWN_TIME = 0.5f;           // 出現時間
    static constexpr float AIM_TIME = 1.0f;             // 追尾時間
    static constexpr float LOCK_TIME = 0.3f;            // 照準固定時間
    static constexpr float FIRE_TIME = 1.0;             // 発射時間
    static constexpr float DISAPPEAR_TIME = 0.3f;       // 消滅時間

    // ビームサイズ計算用パラメータ
    static constexpr float BEAM_SCALE = 22.0f;            // スケール値
    static constexpr float BEAM_LENGTH = (6.0f / 6.0f) * 64.0f * BEAM_SCALE;   // 横幅 = 1280
    static constexpr float BEAM_WIDTH = (6.0f / 5.0f) * 64.0f * BEAM_SCALE;    // 縦幅 = 1536（当たり判定もこれに合わせる）

    // テクスチャ
    int m_maskTexId{ -1 };
    int m_maskOpenTexId{ -1 };      // 口が開いた仮面
    int m_beamTexId{ -1 };
    int m_aimLineTexId{ -1 };       // 照準線

    DirectX::XMFLOAT2 m_maskSize{ 64.0f * 2.5f, 64.0f * 2.5f };

    // 難易度によるスケール
    float m_hardModeScale{ 1.0f };

public:
    void Start(Boss* boss, Player* target) override;
    void Update(float deltaTime) override;
    void Draw() override;
    bool IsFinished() const override { return m_finished; }
    void Reset() override;

    const char* GetName() const override { return "MaskBeam"; }
    int GetId() const override { return BossAttackId::NORMAL_4; }

private:
    void UpdatePhase(float deltaTime);
    void SpawnNewMask();
    float CalculateAngleToPlayer();
    void CreateBeamProjectile();
    void RemoveBeamProjectile();
    void DrawBeam(float centerX, float centerY, float angle);
};

//==============================================================================
// 攻撃⑤: ScratchAttack - ひっかき攻撃
//==============================================================================
class ScratchAttack : public IBossAttack {
private:
    Boss* m_boss{ nullptr };
    Player* m_target{ nullptr };

    bool m_finished{ false };
    float m_timer{ 0.0f };
    int m_spawnedCount{ 0 };

    static constexpr int SCRATCH_COUNT = 5;
    static constexpr float SPAWN_INTERVAL = 0.6f;
    static constexpr float READY_TIME = 1.5f;
    static constexpr float FRAME_SPEED = 0.03f;

    // プレイヤー座標を参照　今は0.2秒前のところ
    static constexpr float DELAY_TIME = 0.3f;
    struct PosRecord {
        DirectX::XMFLOAT2 pos;
        float time;
    };
    std::vector<PosRecord> m_posHistory;
    float m_elapsed{ 0.0f };

    int m_scratchTexId{ -1 };

    DirectX::XMFLOAT2 m_scratchSize{ 64.0f * 4.0f, 64.0f * 4.0f };

    // 難易度によるスケール
    float m_hardModeScale{ 1.0f };

public:
    void Start(Boss* boss, Player* target) override;
    void Update(float deltaTime) override;
    void Draw() override;
    bool IsFinished() const override { return m_finished; }
    void Reset() override;

    const char* GetName() const override { return "Scratch"; }
    int GetId() const override { return BossAttackId::NORMAL_5; }

private:
    DirectX::XMFLOAT2 GetDelayedPlayerPos();
};



//==============================================================================
// 必殺①: SpecialBeamAttack - 仮面からビーム（強）
//==============================================================================
class SpecialBeamAttack : public IBossAttack {
private:
    Boss* m_boss{ nullptr };
    Player* m_target{ nullptr };

    bool m_finished{ false };
    float m_timer{ 0.0f };

    // フェーズ管理
    enum class Phase {
        SPAWN,          // 仮面出現
        CHARGE,         // チャージ
        FIRE,           // 発射
        DISAPPEAR       // 消滅
    };

    struct BeamMask {
        DirectX::XMFLOAT2 pos;
        float angle;
        Phase phase;
        float phaseTimer;
        bool isActive;
        bool isLeft;
    };

    BeamMask m_masks[2];
    BeamHitboxProjectile* m_beams[2]{ nullptr, nullptr };

    // ビームアニメーション
    AnimPattern* m_beamAnim{ nullptr };
    AnimPatternPlayer* m_beamAnimPlayers[2]{ nullptr, nullptr };

    // ラウンド管理（2ラウンドに分けて発射）
    int m_currentRound{ 0 };
    static constexpr int TOTAL_ROUNDS = 2;
    bool m_waitingForNextRound{ false };
    float m_roundGapTimer{ 0.0f };
    static constexpr float ROUND_GAP_TIME = 0.2f;

    // パラメータ
    static constexpr float SPAWN_TIME = 0.2f;
    static constexpr float CHARGE_TIME = 0.5f;
    static constexpr float FIRE_TIME = 1.5f;
    static constexpr float DISAPPEAR_TIME = 0.3f;

    // ビームサイズ（④MaskBeamAttackと同じ計算式、ただし大きめ）
    static constexpr float BEAM_SCALE = 30.0f;
    static constexpr float BEAM_LENGTH = (6.0f / 6.0f) * 64.0f * BEAM_SCALE;   // 1600
    static constexpr float BEAM_WIDTH = (6.0f / 5.0f) * 64.0f * BEAM_SCALE;    // 1920

    // テクスチャ
    int m_maskTexId{ -1 };
    int m_maskOpenTexId{ -1 };
    int m_beamTexId{ -1 };
    int m_chargeEffectTexId{ -1 };
    int m_aimLineTexId{ -1 };

    DirectX::XMFLOAT2 m_maskSize{ 96.0f, 96.0f };

    // ダメージ
    static constexpr int DAMAGE = 3;
    static constexpr float KNOCKBACK_FORCE = 800.0f;

public:
    void Start(Boss* boss, Player* target) override;
    void Update(float deltaTime) override;
    void Draw() override;
    bool IsFinished() const override { return m_finished; }
    void Reset() override;

    const char* GetName() const override { return "SpecialBeam"; }
    int GetId() const override { return BossAttackId::SPECIAL_1; }

private:
    void UpdatePhase(float deltaTime);
    void StartRound(int round);
    void CreateBeamProjectiles();
    void RemoveBeamProjectiles();
    void DrawChargeEffect(float cx, float cy, float progress);
    void DrawAimLine(float cx, float cy, float angle, float alpha);
};


//==============================================================================
// 大技②: SpecialBulletStormAttack - お面弾乱れうち
//==============================================================================
class SpecialBulletStormAttack : public IBossAttack {
private:
    Boss* m_boss{ nullptr };
    Player* m_target{ nullptr };

    bool m_finished{ false };
    float m_timer{ 0.0f };

    // ウェーブ管理
    int m_currentWave{ 0 };
    float m_waveTimer{ 0.0f };
    int m_bulletsSpawnedInWave{ 0 };

    // パラメータ
    static constexpr int WAVE_COUNT = 4;
    static constexpr int BULLETS_PER_WAVE = 12;
    static constexpr float WAVE_INTERVAL = 2.0f;
    static constexpr float BULLET_SPAWN_INTERVAL = 0.1f;
    static constexpr float CHARGE_TIME = 1.0f;
    static constexpr float BULLET_SPEED = 600.0f;

    // 出現位置パターン
    struct SpawnPoint {
        DirectX::XMFLOAT2 pos;
        float angle;
        bool showWarning;
    };
    std::vector<SpawnPoint> m_spawnPoints;

    // テクスチャ
    int m_maskTexId{ -1 };
    int m_warningTexId{ -1 };

    DirectX::XMFLOAT2 m_bulletSize{ 48.0f, 48.0f };

    // 警告表示用
    std::vector<DirectX::XMFLOAT2> m_warningPositions;
    float m_warningAlpha{ 0.0f };

    // 難易度によるスケール
    float m_hardModeScale{ 1.0f };

public:
    void Start(Boss* boss, Player* target) override;
    void Update(float deltaTime) override;
    void Draw() override;
    bool IsFinished() const override { return m_finished; }
    void Reset() override;

    const char* GetName() const override { return "SpecialBulletStorm"; }
    int GetId() const override { return BossAttackId::SPECIAL_2; }

private:
    void GenerateWavePattern(int waveIndex);
    void SpawnBullet(const SpawnPoint& point);
    void DrawWarnings();
};



#endif // GAME_BOSS_ATTACKS_H
