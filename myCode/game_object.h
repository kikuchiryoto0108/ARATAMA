//==============================================================================
// ゲームオブジェクト基底クラス [game_object.h]
// Author : yokozuka yumito / Ryoto Kikuchi
// Date   : 2025/11/07
//==============================================================================
#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H

#include <DirectXMath.h>
#include <vector>
#include "collision.h"
#include "time_manager.h"

// オブジェクトの種類
enum class ObjectTag {
    NONE,
    PLAYER,
    ENEMY_KATANA,
    ENEMY_BOW,
    ENEMY_KANABO,
    ENEMY_YARI,
    BLOCK_NORMAL,
    BLOCK_GRASS,
    BLOCK_STONE,
    BLOCK_WOOD,
    BLOCK_SLIP_THROUGH,
    BLOCK_DAMAGE,
    BLOCK_AIR,
    BLOCK_SIGN,
    BULLET,
    WEAPON,
    CHECKPOINT,
    GIMMICK_ARENA,   // アリーナ
    GIMMICK_SPEED_UP,   // スピードアップ
    GIMMICK_GUILLOTINE, // ギロチン
    GIMMICK_JUMP_PAD,   // ジャンプ台
    GIMMICK_SWITCH,     // スイッチ
    GIMMICK_CANNON,     // 大砲
    GIMMICK_STAKE,      // 杭
	GIMMICK_DOOR,       // ドア
	GIMMICK_NEXT_STAGE, // 次ステージ
	GIMMICK_BLOCK, // 次ステージ
	BOSS,               // ボス本体
	BOSS_MASK,          // ボスのお面
	BOSS_PROJECTILE,    // ボスの攻撃弾
};

class GameObject {
public:
    static constexpr float GRAVITY{ 9.8f * 60.0f * 6.0f };

private:
    DirectX::XMFLOAT2 m_position{};
    DirectX::XMFLOAT2 m_prevPosition{};  // 前フレーム位置（トンネリング防止用）
    DirectX::XMFLOAT2 m_velocity{};
    DirectX::XMFLOAT2 m_size{};
    DirectX::XMFLOAT4 m_color{ 1.0f,1.0f,1.0f,1.0f };
    int m_hp{};
    bool m_active{ true };
    bool m_delete{ false };
	bool m_updatable{ false }; // 更新、描画可能か

    float m_hitStopTimer{ 0.0f };

    std::vector<Collider> m_colliders;
    DirectX::XMFLOAT2 m_colliderSize{}; //ブロック用の当たり判定サイズ
    DirectX::XMFLOAT2 m_colliderOffset{}; //ブロック用の当たり判定オフセット
    ObjectTag m_tag;

public:
    GameObject(DirectX::XMFLOAT2 position, DirectX::XMFLOAT2 size, ObjectTag tag = ObjectTag::NONE)
        : m_position(position), m_prevPosition(position), m_size(size), m_active(true), m_delete(false), m_tag(tag) {
    }
    virtual ~GameObject() {}

    virtual void Initialize() = 0;
    virtual void Finalize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;

    // 当たり判定イベント
    virtual void OnCollision(const GameObject*, const Collider*, const Collider*, COLLISION_VEC = COLLISION_VEC::NONE) = 0;

    // 前フレーム位置を保存（CollisionManagerから呼ばれる）
    void SavePrevPosition() {
        m_prevPosition = m_position;
        // Colliderの前フレーム位置も更新
        for (auto& col : m_colliders) {
            col.SavePrevPos();
        }
    }

    // 前フレーム位置を取得
    DirectX::XMFLOAT2 GetPrevPosition() const { return m_prevPosition; }

protected:
    // ヒットストップ時間設定(ヒットストップスタート)
    void HitStop(float duration) { m_hitStopTimer = duration; }
    // ヒットストップ(Updateの先頭に "if(HitStopUpdate()) return;" を書く)
    bool HitStopUpdate() {
        if (m_hitStopTimer > 0.0f) {
            // タイマーを減らす
            m_hitStopTimer -= (float)TimeManager::GetInstance().GetElapsedTime();

            return true;
        }
        return false;
    }

    //====================================================
    // Collider管理
    // 
    // ※当たり判定の分だけ事前にx個容量確保(コンストラクタ)
    // GetColliders().reserve(x);
    // 
    // Collider追加
    void AddCollider(DirectX::XMFLOAT2 pos, DirectX::XMFLOAT2 size, ColliderType type) {
        m_colliders.emplace_back(this, pos, size, type);
    }

public:
    std::vector<Collider>& GetColliders() { return m_colliders; }
    const std::vector<Collider>& GetColliders() const { return m_colliders; }
    //====================================================

    //ゲッター
    DirectX::XMFLOAT2 GetCenter() const { return { m_position.x + m_size.x * 0.5f,m_position.y + m_size.y * 0.5f }; }
    DirectX::XMFLOAT2 GetPosition() const { return m_position; }
    float GetPositionX() const { return m_position.x; }
    float GetPositionY() const { return m_position.y; }
    DirectX::XMFLOAT2 GetVelocity() const { return m_velocity; }
    float GetVelocityX() const { return m_velocity.x; }
    float GetVelocityY() const { return m_velocity.y; }
    DirectX::XMFLOAT2 GetSize() const { return m_size; }
    DirectX::XMFLOAT2 GetColliderSize() const { return m_colliderSize; }
    DirectX::XMFLOAT2 GetColliderOffset() const { return m_colliderOffset; }
    DirectX::XMFLOAT4 GetColor() const { return m_color; }
    int GetHp() const { return m_hp; }
    ObjectTag GetTag() const { return m_tag; }

    bool IsDelete() const { return m_delete; }
    bool IsActive() const { return m_active; }
    virtual bool IsUpdatable() const { return m_updatable; }

    void SetDelete(bool isDelete) { m_delete = isDelete; }
    void SetActive(bool active) { m_active = active; }

	//CollisionManagerから呼ぶ
    //使わない！
    void SetUpdatable(bool updatable) { m_updatable = updatable; }

protected:
    //セッター
    void SetPosition(DirectX::XMFLOAT2 position) { m_position = position; }
    void SetPositionX(float positionX) { m_position.x = positionX; }
    void SetPositionY(float positionY) { m_position.y = positionY; }
    void SetVelocity(DirectX::XMFLOAT2 velocity) { m_velocity = velocity; }
    void SetVelocityX(float velocityX) { m_velocity.x = velocityX; }
    void SetVelocityY(float velocityY) { m_velocity.y = velocityY; }
    void SetSize(DirectX::XMFLOAT2 size) { m_size = size; }
    void SetTag(ObjectTag tag) { m_tag = tag; }
    void SetColor(DirectX::XMFLOAT4 color) { m_color = color; }
    void SetHp(int hp) { m_hp = hp; }

    void SetColliderSize(DirectX::XMFLOAT2 size) { m_colliderSize = size; }
    void SetColliderOffset(DirectX::XMFLOAT2 offset) { m_colliderOffset = offset; }

    //値の追加
    //void AddPosition(DirectX::XMFLOAT2 position) { m_position.x += position.x * (float)TimeManager::GetInstance().GetScaledElapsedTime();; m_position.y += position.y * (float)TimeManager::GetInstance().GetScaledElapsedTime();;}
    //void AddPositionX(float positionX) { m_position.x += positionX * (float)TimeManager::GetInstance().GetScaledElapsedTime();}
    //void AddPositionY(float positionY) { m_position.y += positionY * (float)TimeManager::GetInstance().GetScaledElapsedTime();;}

    //速度加算
    void AddVelocity(DirectX::XMFLOAT2 velocity) { m_velocity.x += velocity.x * (float)TimeManager::GetInstance().GetScaledElapsedTime(); m_velocity.y += velocity.y * (float)TimeManager::GetInstance().GetScaledElapsedTime(); }
    void AddVelocityX(float velocityX) { m_velocity.x += velocityX * (float)TimeManager::GetInstance().GetScaledElapsedTime(); }
    void AddVelocityY(float velocityY) { m_velocity.y += velocityY * (float)TimeManager::GetInstance().GetScaledElapsedTime(); }
    //速度抵抗計算
    void ResistanceVelocity(DirectX::XMFLOAT2 velocity) { m_velocity.x *= velocity.x; m_velocity.y *= velocity.y; }
    void ResistanceVelocityX(float velocityX) { m_velocity.x *= velocityX; }
    void ResistanceVelocityY(float velocityY) { m_velocity.y *= velocityY; }
    //速度分座標を移動
    void Move();
    void MoveX();
    void MoveY();

    void AddHp(int hp) { m_hp += hp; }

public:
    //速度Xが引き数以下でtrue
    bool VelocityXUnder(float velocityX) const { return m_velocity.x <= velocityX; }
    //速度Xが引き数以上でtrue
    bool VelocityXAbove(float velocityX) const { return m_velocity.x >= velocityX; }
    //速度Yが引き数以下でtrue
    bool VelocityYUnder(float velocityY) const { return m_velocity.y <= velocityY; }
    //速度Yが引き数以上でtrue
    bool VelocityYAbove(float velocityY) const { return m_velocity.y >= velocityY; }
    //速度Xが0か？
    bool VelocityXIsZero() const { return m_velocity.x < 0.1 && m_velocity.x > -0.1; }
    bool VelocityYIsZero() const { return m_velocity.y < 0.1 && m_velocity.y > -0.1; }

};

#endif // GAME_OBJECT_H

