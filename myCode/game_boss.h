/*********************************************************************
 * \file   game_boss.h
 * \brief  ボス
 *
 * \author Ryoto Kikuchi
 * \date   2026/2/14
 *********************************************************************/
#ifndef GAME_BOSS_H
#define GAME_BOSS_H

#include "game_object.h"
#include "game_boss_attack.h"
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include "sprite_anim.h"
#include "aura.h"

class IBossState;
class BossMask;
class Stage;
class BossHand;
class MediaPlayer;

//==============================================================================
// Boss
//==============================================================================
class Boss : public GameObject {
private:
    static constexpr int BASE_MAX_HP = 550;  // 基本最大HP
    int m_maxHp{ BASE_MAX_HP };
    int m_hp{ BASE_MAX_HP };
    bool m_isDead{ false };
    int m_textureId{ -1 };
    int m_barrierTextureId{ -1 };
    int m_hpBarTextureId{ -1 };
    int m_hpBarBgTextureId{ -1 };

    //ボスエフェクト演出管理用
    float m_chargeEffect{ 0.0f };
    float m_readyEffect{ 0.0f };
    float m_stunnedEffect{ 0.0f };
    bool m_isReadyEffect{ false };
    bool m_isStunnedEffect{ false };

    // ステート管理
    IBossState* m_currentState{ nullptr };

    // ボスアイドルアニメーション
    int m_idleAnimTexIds[3]{ -1, -1, -1 };  // IDLE_ANIM1, 2, 3
    AnimPattern* m_idleAnim[3]{ nullptr, nullptr, nullptr };
    AnimPatternPlayer* m_idleAnimPlayer{ nullptr };
    int m_currentIdleIndex{ 0 };

    // ボスアイドルアニメ定数
    static constexpr float BOSS_FRAME_WIDTH = 400.0f;
    static constexpr float BOSS_FRAME_HEIGHT = 320.0f;
    static constexpr int BOSS_ANIM_COLS = 6;
    static constexpr int BOSS_ANIM_TOTAL = 30;

    // ボスダウンアニメーション
    int m_downAnimTexIds[3]{ -1, -1, -1 };  // DOWN_ANIM1, 2, 3
    AnimPattern* m_downAnim[3]{ nullptr, nullptr, nullptr };
    AnimPatternPlayer* m_downAnimPlayer{ nullptr };
    int m_currentDownIndex{ 0 };

    // ダウン状態フラグ
    bool m_isDown{ false };
    
    // バリア表示
    bool m_showBarrier{ false };

    // 仮面管理
    std::vector<BossMask*> m_masks;
    static constexpr int MASK_COUNT = 5;

    // 無敵時間
    float m_damageInvincibleTimer{ 0.0f };
    static constexpr float DAMAGE_INVINCIBLE_TIME = 0.5f;  // ダメージ後の無敵時間

    // ターゲット渡す用
    Player* m_player{ nullptr };

    // 攻撃管理
    BossAttackManager m_normalAttackManager;
    BossAttackManager m_specialAttackManager;

    // ステージ参照
    Stage* m_stage{ nullptr };

    // バリアアニメーション
    AnimPattern* m_barrierAnim{ nullptr };
    AnimPatternPlayer* m_barrierAnimPlayer{ nullptr };
    bool m_isBarrierAnimPlaying{ false };  // アニメーション再生中フラグ
    bool m_barrierAnimPlayed{ false };

    // バリアサイズ
    static constexpr float BARRIER_FRAME_WIDTH = 227.2f;
    static constexpr float BARRIER_FRAME_HEIGHT = 157.3f;
    static constexpr int BARRIER_ANIM_COLS = 5;
    static constexpr int BARRIER_ANIM_ROWS = 6;
    static constexpr int BARRIER_ANIM_TOTAL = 30;  // 5 * 6

    // 手
    BossHand* m_rightHand{ nullptr };
    BossHand* m_leftHand{ nullptr };
    DirectX::XMFLOAT2 m_rightHandPos{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 m_leftHandPos{ 0.0f, 0.0f };

    // ボス浮遊移動
    DirectX::XMFLOAT2 m_basePos{ 0.0f, 0.0f };  // 初期位置
    float m_floatTimer{ 0.0f };
    static constexpr float BOSS_FLOAT_SPEED_X = 0.5f;    // 横の速度（ゆっくり）
    static constexpr float BOSS_FLOAT_SPEED_Y = 1.2f;    // 縦の速度
    static constexpr float BOSS_FLOAT_RANGE_X = 200.0f;   // 横の移動範囲（大きめ）
    static constexpr float BOSS_FLOAT_RANGE_Y = 15.0f;    // 縦の移動範囲（極小）

    // マスク遅延破壊用
    std::vector<BossMask*> m_masksToDestroy;

    int m_maskPattern{ 0 };  // マスク配置パターン

    bool m_hideHpBar{ false };

    // イントロ動画関連
    MediaPlayer* m_introMovie{ nullptr };
    bool m_introPlaying{ false };
    bool m_introPlayed{ false };  // 一度再生したらtrue（コンティニュー時スキップ用）

    // オーラ
    Aura* m_aura{ nullptr };

    // ダメージテーブル [武器タイプ][通常/特殊]
    static constexpr int DAMAGE_TABLE[4][2] = {
        { 7, 28 },  // KATANA:  通常10, 特殊30
        { 7, 28 },  // SPEAR:   通常10, 特殊30
        { 10, 40 },  // KANABO:  通常12, 特殊30
        { 7, 28 },  // BOW:     通常10, 特殊30
    };

    bool m_shouldResetHp{ false };  // HPをリセットするか（コンティニュー用）

    // リセット時のスムーズ移動
    bool m_isResetting{ false };
    float m_resetTimer{ 0.0f };
    static constexpr float RESET_MOVE_TIME = 1.0f;  // 0.5秒かけて戻る
    DirectX::XMFLOAT2 m_resetStartPos{ 0.0f, 0.0f };

public:
    Boss(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size);
    ~Boss();

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override;

    // ステート変更
    void ChangeState(IBossState* newState);
    IBossState* GetCurrentState() const { return m_currentState; }

    // ダメージを受ける
    void TakeDamage(int damage);

    // ゲッター
    int GetHp() const { return m_hp; }
    int GetMaxHp() const { return m_maxHp; }
    bool IsDead() const { return m_isDead; }
    // ダメージ計算（プレイヤーの武器タイプと攻撃方法から）
    int CalcDamageFromPlayer() const;

    // バリア
    void ShowBarrier(bool show) { m_showBarrier = show; }
    bool IsBarrierActive() const { return m_showBarrier; }

    // 仮面関連
    void SpawnMasks();
    void DestroyAllMasks();
    int GetDestroyedMaskCount() const;
    bool AreAllMasksDestroyed() const;

    // 無敵時間
    bool IsInvincible() const { return m_damageInvincibleTimer > 0.0f; }
    void SetInvincible(float time) { m_damageInvincibleTimer = time; }

    // プレイヤー参照
    void SetPlayer(Player* player);
    Player* GetPlayer() const { return m_player; }

    // 攻撃管理
    BossAttackManager* GetNormalAttackManager() { return &m_normalAttackManager; }
    BossAttackManager* GetSpecialAttackManager() { return &m_specialAttackManager; }

    // ステージ参照
    void SetStage(Stage* stage);

    // バリアアニメーション関連
    void PlayBarrierBreakAnim();  // マスク破壊時に呼ぶ
    bool IsBarrierAnimPlaying() const { return m_isBarrierAnimPlaying; }

    // マスク取得用ゲッター
    const std::vector<BossMask*>& GetMasks() const { return m_masks; }
    std::vector<BossMask*>& GetMasks() { return m_masks; }

    // アクティブなマスクのみ取得
    std::vector<BossMask*> GetActiveMasks() const;

    // マスク数取得
    int GetMaskCount() const { return MASK_COUNT; }
    int GetActiveMaskCount() const;

    // 手の取得
    BossHand* GetRightHand() const { return m_rightHand; }
    BossHand* GetLeftHand() const { return m_leftHand; }
    void SetHandPositions(DirectX::XMFLOAT2 rightPos, DirectX::XMFLOAT2 leftPos) {
        m_rightHandPos = rightPos;
        m_leftHandPos = leftPos;
    }

    void Reset();

    // マスク破壊リクエスト（即座に破壊せずキューに入れる）
    void RequestMaskDestroy(BossMask* mask);

    // イントロ動画関連
    bool IsIntroPlaying() const { return m_introPlaying; }
    bool HasIntroPlayed() const { return m_introPlayed; }
    void SetIntroPlayed(bool played) { m_introPlayed = played; }
    void StartIntroMovie();
    void SkipIntroMovie();

    // 動画のSRV取得（描画用）
    ID3D11ShaderResourceView* GetIntroMovieSRV() const;

    UINT GetIntroMovieWidth() const;
    UINT GetIntroMovieHeight() const;

    // オーラ
    Aura* GetAura() const { return m_aura; }
    
	// ダメージ計算
    void SetShouldResetHp(bool reset) { m_shouldResetHp = reset; }
	bool GetShouldResetHp() const { return m_shouldResetHp; }
    void SetHp(int hp) { m_hp = hp; }
    void SetMaxHp(int maxHp) { m_maxHp = maxHp; m_hp = maxHp; }

private:
    // マスクの遅延破壊処理
    void ProcessPendingMaskDestroys();

public:
    void SetHideHpBar(bool hide) { m_hideHpBar = hide; }

    // 撃破演出のフラッシュ等、最前面に描画するもの
    void DrawOverlay();
};

//==============================================================================
// BossMask - 仮面
//==============================================================================
class BossMask : public GameObject {
public:
    enum class MaskType {
        KATANA,
        KANABO,
        BOW,
        SPEAR
    };

private:
    Boss* m_owner{ nullptr };
    bool m_isDestroyed{ false };
    bool m_pendingDestroy{ false };
    int m_textureId{ -1 };
    MaskType m_maskType{ MaskType::KATANA };

    // フェード関連
    float m_alpha{ 0.0f };
    bool m_isFadingIn{ false };
    bool m_isFadingOut{ false };
    static constexpr float FADE_IN_TIME = 0.5f;
    static constexpr float FADE_OUT_TIME = 0.3f;
    float m_fadeTimer{ 0.0f };

public:
    BossMask(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size, Boss* owner, MaskType type);

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override;

    bool IsDestroyed() const { return m_isDestroyed; }
    void Destroy();
    void Reset();

    // Active操作
    void Activate();
    void Deactivate();

    // Position操作
    void SetMaskPosition(DirectX::XMFLOAT2 pos) { SetPosition(pos); }
    void SetMaskSize(DirectX::XMFLOAT2 size) { SetSize(size); }

    // タイプ取得
    MaskType GetMaskType() const { return m_maskType; }
    void SetMaskType(MaskType type) { m_maskType = type; }

    // 破壊フラグ操作
    void MarkAsDestroyed() { m_isDestroyed = true; }
    bool IsPendingDestroy() const { return m_pendingDestroy; }

	// フェード操作
    bool IsFading() const { return m_isFadingIn || m_isFadingOut; }
};


//==============================================================================
// BossHand - ボスの手
//==============================================================================
class BossHand : public GameObject {
public:
    enum class HandSide {
        RIGHT,
        LEFT
    };

private:
    Boss* m_owner{ nullptr };
    HandSide m_side{ HandSide::RIGHT };
    int m_textureId{ -1 };

    // 八の字移動用
    DirectX::XMFLOAT2 m_basePos{ 0.0f, 0.0f };  // 基準位置
    float m_moveTimer{ 0.0f };

    // ランダム化用パラメータ
    float m_moveSpeedX{ 1.5f };     // X方向の移動速度
    float m_moveSpeedY{ 3.0f };     // Y方向の移動速度
    float m_moveRangeX{ 150.0f };   // X方向の移動範囲
    float m_moveRangeY{ 80.0f };    // Y方向の移動範囲
    float m_phaseOffsetX{ 0.0f };   // X位相オフセット
    float m_phaseOffsetY{ 0.0f };   // Y位相オフセット

    void RandomizeParameters();  // パラメータをランダム化

    // 手の当たり判定
    bool m_hasHitPlayer{ false };
    float m_hitCooldown{ 0.0f };
    static constexpr float HIT_COOLDOWN_TIME = 1.0f;  // 連続ヒット防止

    // 手のアニメーション
    int m_startAnimTexId{ -1 };
    int m_idleAnimTexIds[3]{ -1, -1, -1 };
    AnimPattern* m_startAnim{ nullptr };
    AnimPatternPlayer* m_startAnimPlayer{ nullptr };
    AnimPattern* m_idleAnim[3]{ nullptr, nullptr, nullptr };
    AnimPatternPlayer* m_idleAnimPlayer{ nullptr };
    int m_currentIdleIndex{ 0 };
    float m_idleAnimSwitchTimer{ 0.0f };
    float m_idleAnimSwitchInterval{ 2.0f };

    bool m_isStartAnimPlaying{ false };
    bool m_startAnimFinished{ false };

    // 手のアニメ定数（横6×縦5、テクスチャ1024x1024）
    static constexpr float HAND_FRAME_WIDTH = 170.667f;
    static constexpr float HAND_FRAME_HEIGHT = 204.8f;
    static constexpr int HAND_ANIM_COLS = 6;
    static constexpr int HAND_ANIM_TOTAL = 30;

    // ダウンアニメーション
    int m_downAnimTexIds[3]{ -1, -1, -1 };
    AnimPattern* m_downAnim[3]{ nullptr, nullptr, nullptr };
    AnimPatternPlayer* m_downAnimPlayer{ nullptr };
    int m_currentDownIndex{ 0 };
    bool m_isDown{ false };

    // バリア関連
    bool m_hasBarrier{ false };
    int m_barrierTextureId{ -1 };
    AnimPattern* m_barrierAnim{ nullptr };
    AnimPatternPlayer* m_barrierAnimPlayer{ nullptr };
    bool m_isBarrierAnimPlaying{ false };

    static constexpr float BARRIER_FRAME_WIDTH = 227.2f;
    static constexpr float BARRIER_FRAME_HEIGHT = 157.3f;
    static constexpr int BARRIER_ANIM_COLS = 5;
    static constexpr int BARRIER_ANIM_TOTAL = 30;

    // リセット時のスムーズ移動
    bool m_isResetting{ false };
    float m_resetTimer{ 0.0f };
    static constexpr float RESET_MOVE_TIME = 1.0f;
    DirectX::XMFLOAT2 m_resetStartPos{ 0.0f, 0.0f };

public:
    BossHand(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size, Boss* owner, HandSide side);

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override;

    void Reset();
    void SetBasePosition(DirectX::XMFLOAT2 pos) { m_basePos = pos; }
    HandSide GetSide() const { return m_side; }

    // バリア関連
    void SetBarrier(bool active) { m_hasBarrier = active; }
    bool HasBarrier() const { return m_hasBarrier; }
    void PlayBarrierBreakAnim();
    // バリア破壊アニメを停止（バリア張る時に前回の残りを消す）
    void StopBarrierAnim();

	// アニメーション関連
    void StartAppearAnim();  // 登場アニメ開始
    bool IsStartAnimFinished() const { return m_startAnimFinished; }

    // ボス暴走（憑依演出）
private:
    // 暴れモード
    bool m_isRaging{ false };
    float m_rageTimer{ 0.0f };

    // 暴れ用：直線移動＋壁反射
    DirectX::XMFLOAT2 m_rageVelocity{ 0.0f, 0.0f };
    float m_rageSpeed{ 2500.0f };

    // 壁ヒット演出
    float m_wallHitTimer{ 0.0f };
    static constexpr float WALL_HIT_SHAKE_POWER = 10.0f;
    static constexpr float WALL_HIT_SHAKE_DURATION = 0.1f;

    // ステージ範囲（壁の内側）
    float m_stageLeft{ 64.0f };
    float m_stageRight{ 1856.0f };
    float m_stageTop{ 64.0f };
    float m_stageBottom{ 1024.0f };

public:
    void StartRaging(float stageLeft, float stageRight, float stageTop, float stageBottom);
    void StopRaging();
    bool IsRaging() const { return m_isRaging; }

public:
    void ResetSmooth();  // スムーズリセット開始
    void SetSkipStartAnim(bool skip) { m_skipStartAnim = skip; }

private:
    bool m_skipStartAnim{ false };  // スタートアニメをスキップするか
};

#endif // GAME_BOSS_H
