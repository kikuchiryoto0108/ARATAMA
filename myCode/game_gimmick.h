/*****************************************************************//**
 * \file   game_gimmick.h
 * \brief  各種ギミック
 *
 * \author 菊池凌斗
 * \date   2026/1/13
 *
 * 基本的な機能のギミック基底クラス->派生クラスで各種ギミック実装
 *********************************************************************/
#ifndef GAME_GIMMICK_H
#define GAME_GIMMICK_H

#include "game_object.h"
#include "collision.h"
#include "game_block.h"
#include "game_boss.h"
#include <DirectXMath.h>
#include <string>
#include <vector>
#include "game_boss_attacks.h"

class Player;
class MidBossWall;

class GameGimmick : public GameObject {
public:
    // ギミックの種類を定義
    enum class Type {
        NONE,
        SPEED_UP,      // スピードアップ
        GUILLOTINE,    // ギロチン 
        JUMP_PAD,      // ジャンプ台
        SWITCH,        // スイッチ 
        CANNON,        // 大砲 
        STAKE,         // 杭
        DOOR,          // ドア  
        WALL,          // ドア  
    };

protected:
    Type m_type{ Type::NONE };
    int m_textureId{ -1 };
    Collider* m_bodyCollider{ nullptr };

public:
    GameGimmick(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size, Type type)
        : GameObject(pos, size, ObjectTag::NONE), m_type(type) {
        // コライダーを1つ確保（reserve でポインタ安定化）
        GetColliders().reserve(1);
        AddCollider(GetPosition(), GetSize(), ColliderType::BODY);
        m_bodyCollider = &GetColliders().back();
    }

    virtual ~GameGimmick() {}

    virtual void Initialize() override;
    virtual void Finalize() override;
    virtual void Update() override;
    virtual void Draw() override;

    virtual void OnCollision(const GameObject* otherObject, const Collider* myCollider, const Collider* otherCollider, COLLISION_VEC hitVec = COLLISION_VEC::NONE) override;

    virtual void UpdateColliderPosition() {
        if (m_bodyCollider) {
            m_bodyCollider->SetColliderPos(GetPosition());
        }
    }

    Type GetGimmickType() const { return m_type; }
};

//==============================================================================
// SpeedUpGimmick - スピードアップギミック
//==============================================================================
class SpeedUpGimmick : public GameGimmick {
public:
    enum class Direction { Horizontal, Vertical };
private:
    static constexpr float BOOST_VELOCITY_X = 40.0f * 60.0f;  // 押し出す速度
    static constexpr float BOOST_VELOCITY_Y = 20.0f * 60.0f;  // 押し出す速度
    static constexpr float APPLY_COOLDOWN = 0.2f;     // クールダウン

    Direction m_direction;
    // 横
    int m_texFrontH = -1;
    int m_texBackH = -1;
    // 縦
    int m_texFrontV = -1;
    int m_texBackV = -1;
    DirectX::XMFLOAT2 m_effectRegionOffset{};
    DirectX::XMFLOAT2 m_effectRegionSize{};
    float m_cooldownTimer{ 0.0f };

public:
    SpeedUpGimmick(
        DirectX::XMFLOAT2 pos,
        DirectX::XMFLOAT2 size,
        Direction direction,
        DirectX::XMFLOAT2 effectRegionOffset,
        DirectX::XMFLOAT2 effectRegionSize
    )
        : GameGimmick(pos, size, Type::SPEED_UP),
        m_direction(direction),
        m_effectRegionOffset(effectRegionOffset),
        m_effectRegionSize(effectRegionSize) {
        GetColliders().reserve(1);
        SetTag(ObjectTag::GIMMICK_SPEED_UP);
    }

    virtual ~SpeedUpGimmick() {}

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    //void DrawBack();   // 背面レイヤーだけ描画
    void DrawFront();  // 前面レイヤーだけ描画

    void OnCollision(const GameObject* otherObject, const Collider* myCollider, const Collider* otherCollider, COLLISION_VEC hitVec = COLLISION_VEC::NONE) override;
};

//==============================================================================
// JumpPadGimmick - ジャンプ台ギミック
//==============================================================================
class JumpPadGimmick : public GameGimmick {
private:
    static constexpr float JUMP_POWER_BOOST = -35.0f * 60.0f;   // 大ジャンプ

    static constexpr float TEXTURE_WIDTH = 662.0f;
    static constexpr float TEXTURE_HEIGHT = 198.0f;

    static constexpr float DRAW_WIDTH = 192.0f;
    static constexpr float DRAW_HEIGHT = DRAW_WIDTH * (TEXTURE_HEIGHT / TEXTURE_WIDTH);

    static constexpr float OFFSET_Y = 8.0f;  // 下にずらすオフセット
public:
    JumpPadGimmick(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size)
        : GameGimmick(pos, { DRAW_WIDTH, DRAW_HEIGHT }, Type::JUMP_PAD) {
        SetTag(ObjectTag::GIMMICK_JUMP_PAD);
    }

    virtual ~JumpPadGimmick() {}

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject*, const Collider*, const Collider*, COLLISION_VEC) override;
};


//==============================================================================
// DoorGimmick - ドアギミック
//==============================================================================
class DoorGimmick : public GameGimmick {
private:
    int m_openTextureId{ -1 };
    bool m_playerEntered{ false };
    bool m_isOpen = false;

    // バリアロック
    bool m_isLocked{ false };
    int m_barrierTextureId{ -1 };

    // バリア破壊アニメーション
    AnimPattern* m_barrierAnim{ nullptr };
    AnimPatternPlayer* m_barrierAnimPlayer{ nullptr };
    bool m_isBarrierAnimPlaying{ false };

    static constexpr float DRAW_SIZE = 64.0f * 6.0f;
    static constexpr float COLLIDER_RATIO = 2.0f / 3.0f;

    // バリアアニメ定数
    static constexpr float BARRIER_FRAME_WIDTH = 227.2f;
    static constexpr float BARRIER_FRAME_HEIGHT = 157.3f;
    static constexpr int BARRIER_ANIM_COLS = 5;
    static constexpr int BARRIER_ANIM_TOTAL = 30;

public:
    DoorGimmick(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size)
        : GameGimmick(pos, size, Type::DOOR) {
        SetTag(ObjectTag::GIMMICK_DOOR);
    }

    virtual ~DoorGimmick() {}

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider, const Collider* otherCollider, COLLISION_VEC hitVec = COLLISION_VEC::NONE) override;

    void Open() { m_isOpen = true; }
    void Close() { m_isOpen = false; }
    bool IsOpen() const { return m_isOpen; }

    bool HasPlayerEntered() const { return m_playerEntered; }
    void ResetPlayerEntered() { m_playerEntered = false; }

    // バリアロック（アニメーション対応）
    void SetLocked(bool locked);
    bool IsLocked() const { return m_isLocked; }

    void Reset() {
        m_isOpen = false;
        m_playerEntered = false;
        m_isLocked = false;
        m_isBarrierAnimPlaying = false;
    }
};



//==============================================================================
// StakeGimmick - 杭ギミック
//==============================================================================
class StakeGimmick : public GameGimmick {
private:
    DoorGimmick* m_linkedDoor = nullptr;
    std::vector<MidBossWall*> m_linkedWalls;
    std::vector<class NextStageGimmick*> m_linkedNextStages;
    std::string m_linkId;
    bool m_isActivated = false;

    BossMask::MaskType m_type{};

    // 仮面モード
    bool m_isMaskMode{ false };
    int m_barrierTextureId{ -1 };

public:
    StakeGimmick(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size, DoorGimmick* linkedDoor, const std::string& linkId)
        : GameGimmick(pos, size, Type::STAKE), m_linkedDoor(linkedDoor), m_linkId(linkId) {
        SetTag(ObjectTag::GIMMICK_STAKE);
    }

    virtual ~StakeGimmick() {}

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider, const Collider* otherCollider, COLLISION_VEC hitVec = COLLISION_VEC::NONE) override;

    void SetLinkedDoor(DoorGimmick* door) { m_linkedDoor = door; }
    void AddLinkedWall(MidBossWall* wall) { m_linkedWalls.push_back(wall); }
    void AddLinkedNextStage(NextStageGimmick* ns) { m_linkedNextStages.push_back(ns); }
    const std::string& GetLinkId() const { return m_linkId; }

    void OnHitByWeapon();

    void Reset() { m_isActivated = false; }
    bool IsActivated() const { return m_isActivated; }

    // 仮面モード設定
    void SetMaskMode(bool mask) { m_isMaskMode = mask; }
    bool IsMaskMode() const { return m_isMaskMode; }
};


//==============================================================================
// GuillotineGimmick - ギロチン親ギミック
//==============================================================================
class GuillotineGimmick : public GameGimmick {
private:
    enum class PartType { Top, Middle, Blade };

    struct Part {
        PartType type;
        DirectX::XMFLOAT2 pos;
        DirectX::XMFLOAT2 size;
        int texId = -1;
        bool isActive = true;
        bool isCut = false;    // 中間ロープのみ
        bool isFalling = false;// 刃のみ
        float fallVelY = 0.0f; // 刃のみ
    };

    Part m_top;
    Part m_middle;
    Part m_blade;

    // パーツごとに独立コライダー
    DirectX::XMFLOAT2 midColPos{};
    DirectX::XMFLOAT2 midColSize{};
    DirectX::XMFLOAT2 bottomColPos{};
    DirectX::XMFLOAT2 bottomColSize{};

    // ★ 刃下部（刃）用コライダー
    DirectX::XMFLOAT2 bladeBottomColPos{};
    DirectX::XMFLOAT2 bladeBottomColSize{};

    static constexpr float TOPCOLSIZE_X = 0.0f;
    static constexpr float TOPCOLSIZE_Y = 0.0f;

    static constexpr float MIDCOLPOS_X = 2.45f * 64.0f;
    static constexpr float MIDCOLPOS_Y = 1.0f * 64.0f;
    static constexpr float MIDCOLSIZE_X = 20.0f;
    static constexpr float MIDCOLSIZE_Y = 64.0f;

    static constexpr float BOTTOMCOLPOS_X = 64.0f * 0.15f;
    static constexpr float BOTTOMCOLPOS_Y = 3.4f * 64.0f;
    static constexpr float BOTTOMCOLSIZE_X = 64.0f * 4.6f;
    static constexpr float BOTTOMCOLSIZE_Y = 64.0f * 1.1f;

    static constexpr float BLADECOLPOS_X = 64.0f * 0.15f;
    static constexpr float BLADECOLPOS_Y = 4.5f * 64.0f;
    static constexpr float BLADECOLSIZE_X = 64.0f * 4.6f;
    static constexpr float BLADECOLSIZE_Y = 64.0f * 0.2f;

public:
    GuillotineGimmick(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size)
        : GameGimmick(pos, size, Type::GUILLOTINE) {
        SetTag(ObjectTag::GIMMICK_GUILLOTINE);
        CreateParts(pos, size);
    }
    ~GuillotineGimmick() override {}

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider, const Collider* otherCollider, COLLISION_VEC hitVec = COLLISION_VEC::NONE) override;

    static float GetBladePositionYOffset() { return BLADECOLPOS_Y; }

    // ロープが切れたか判定（弓のオートエイム除外用）
    bool IsRopeCut() const { return m_middle.isCut; }
private:
    void CreateParts(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size);
    void StartBladeFall();

    // ロープ切る
    void CutRope();
};

//==============================================================================
// NextStageGimmick - 次ステージトリガー
//==============================================================================
class NextStageGimmick : public GameGimmick {
private:
    static constexpr float BLOCK_WIDTH = 64.0f * 4.0f;
    static constexpr float BLOCK_HEIGHT = 64.0f * 4.0f;

    bool m_playerEntered{ false };
    int m_fadeTexId{ -1 };

    // バリアロック
    bool m_isLocked{ false };
    int m_barrierTextureId{ -1 };

    // リンクID
    std::string m_linkId;

    // ★ バリア破壊アニメーション追加
    AnimPattern* m_barrierAnim{ nullptr };
    AnimPatternPlayer* m_barrierAnimPlayer{ nullptr };
    bool m_isBarrierAnimPlaying{ false };

    // バリアアニメ定数（ボスと同じ値）
    static constexpr float BARRIER_FRAME_WIDTH = 227.2f;
    static constexpr float BARRIER_FRAME_HEIGHT = 157.3f;
    static constexpr int BARRIER_ANIM_COLS = 5;
    static constexpr int BARRIER_ANIM_TOTAL = 30;

public:
    NextStageGimmick(DirectX::XMFLOAT2 position, DirectX::XMFLOAT2 size, const std::string& linkId = "")
        : GameGimmick(position, { BLOCK_WIDTH, BLOCK_HEIGHT }, Type::NONE)
        , m_linkId(linkId) {
        SetTag(ObjectTag::GIMMICK_NEXT_STAGE);
    }

    virtual ~NextStageGimmick() {}

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override;

    bool HasPlayerEntered() const { return m_playerEntered; }
    void Reset() { m_playerEntered = false; m_isLocked = false; }

    // バリアロック
    void SetLocked(bool locked);  // ★ 宣言を変更（実装をcppへ移動）
    bool IsLocked() const { return m_isLocked; }

    // リンクID
    const std::string& GetLinkId() const { return m_linkId; }
};



//==============================================================================
// NextStageEntrance - 次ステージ入口（描画のみ）
//==============================================================================
class NextStageEntrance : public GameGimmick {
private:
    static constexpr float BLOCK_WIDTH = 64.0f * 4.0f;
    static constexpr float BLOCK_HEIGHT = 64.0f * 4.0f;

    int m_texId{ -1 };

public:
    NextStageEntrance(DirectX::XMFLOAT2 position, DirectX::XMFLOAT2 size)
        : GameGimmick(position, { BLOCK_WIDTH, BLOCK_HEIGHT }, Type::NONE) {
        SetTag(ObjectTag::NONE);
    }

    virtual ~NextStageEntrance() {}

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override {
    }
};

//==============================================================================
// MidBossWall - ボス部屋前の壁ギミック
//==============================================================================
class MidBossWall : public GameGimmick {
private:
    std::string m_linkId;
    int m_texId{ -1 };

    float m_totalTime{};
    float m_alpha{};

    bool m_isFadeOut{ false };

    Block* m_gridBlock = nullptr;

public:
    MidBossWall(DirectX::XMFLOAT2 position, DirectX::XMFLOAT2 size, const std::string& linkId, Block* gridBlock = nullptr)
        : GameGimmick(position, size, Type::WALL), m_linkId(linkId), m_gridBlock(gridBlock) {
        SetTag(ObjectTag::GIMMICK_BLOCK);
    }

    virtual ~MidBossWall() {}

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject* otherObject, const Collider* myCollider,
        const Collider* otherCollider, COLLISION_VEC hitVec) override {
    }

    const std::string& GetLinkId() const { return m_linkId; }

    // 壁を出す
    void Appear();

    // 壁を消す
    void Destroy();
};

//==============================================================================
// CorridorMaskBarrage - 廊下弾幕ギミック
//==============================================================================
class CorridorMaskBarrage : public GameGimmick {
private:
    float m_triggerRightX{ 0.0f };   // この位置より左にいたら弾が来る

    float m_spawnTimer{ 0.0f };
    float m_spawnInterval{ 1.5f };      // 弾の発射間隔（秒）
    float m_bulletSpeed{ 700.0f };
    DirectX::XMFLOAT2 m_bulletSize{ 96.0f, 96.0f };

    float m_spawnX{ 0.0f };
    float m_stageWidth{ 0.0f };
    float m_stageHeight{ 0.0f };

    std::vector<MaskBulletProjectile*> m_projectiles;

    Player* m_player{ nullptr };
    bool m_isBarrageActive{ false };

    static constexpr float SPAWN_OFFSET_X = 64.0f * 8.0f;
    static constexpr float MAX_ANGLE_OFFSET = 0.35f;
    static constexpr float Y_RANDOM_RANGE = 100.0f;

public:
    CorridorMaskBarrage(DirectX::XMFLOAT2 basePos, float stageWidth, float stageHeight)
        : GameGimmick(basePos, { 64.0f, 64.0f }, Type::NONE)
        , m_stageWidth(stageWidth)
        , m_stageHeight(stageHeight) {
        SetTag(ObjectTag::NONE);
        m_triggerRightX = basePos.x;    // cmbの位置より左なら有効
        m_spawnX = basePos.x + SPAWN_OFFSET_X;
    }

    virtual ~CorridorMaskBarrage() {}

    bool IsUpdatable() const override { return true; }

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void OnCollision(const GameObject*, const Collider*, const Collider*, COLLISION_VEC) override {}

    void SetPlayer(Player* player) { m_player = player; }
    void SetSpawnInterval(float interval) { m_spawnInterval = interval; }
    void SetBulletSpeed(float speed) { m_bulletSpeed = speed; }

private:
    void SpawnMaskBullet();
    void CleanupProjectiles();
};

#endif // GAME_GIMMICK_H