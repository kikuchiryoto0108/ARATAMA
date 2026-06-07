//==============================================================================
//
//   当たり判定の管理 [CollisionManager.h]
// Author : Yumito Yokozuka / Ryoto Kikuchi
// Date   : 2025/11/07
//------------------------------------------------------------------------------
//
//==============================================================================
#include <vector>
#include "game_object.h"
#include "collision.h"
#include "camera.h"
#include <DirectXMath.h>

class CollisionManager {
private:
    std::vector<GameObject*> m_objects; // 当たり判定を行うオブジェクトのリスト
    Camera* m_camera{}; // カメラの当たり判定用コライダー
    Collision m_collision;              // 実際の当たり判定処理を持つクラス

    CollisionManager() = default;

public:
    static CollisionManager& GetInstance() {
        static CollisionManager collisionInstance;
        return collisionInstance;
    }

    // 非コピー
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    void AddObject(GameObject* obj) { m_objects.push_back(obj); } // オブジェクト追加
    void SetCamera(Camera* camera) { m_camera = camera; } // カメラの登録

    // ※登録済みのオブジェクトをdeleteする一行前あたりでこれ使ってリストから消去する
    // (deleteしたオブジェクトの当たり判定が残ってるとクラッシュする)
    void RemoveObject(GameObject* obj) {
        m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), obj), m_objects.end());
    }

    void Update();
    void Draw();

    void CheckUpdateRange();

    //全オブジェクトをリストから消去
    void Finalize() { m_objects.clear(); }

    // 前フレーム位置を保存（トンネリング防止用）
    void SavePrevPositions();

    // 外部からCollisionを使いたい場合
    Collision& GetCollision() { return m_collision; }
};
