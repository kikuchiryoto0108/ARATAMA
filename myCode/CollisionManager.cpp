//==============================================================================
//
//   当たり判定の管理 [CollisionManager.cpp]
// Author : Yumito Yokozuka / Ryoto Kikuchi
// Date   : 2025/11/07
//------------------------------------------------------------------------------
//
//==============================================================================
#include "CollisionManager.h"
#include "Sprite.h"
#include "texture_manager.h"


void CollisionManager::SavePrevPositions() {
    for (auto* obj : m_objects) {
        if (obj && obj->IsActive()) {
            obj->SavePrevPosition();
        }
    }
}

void CollisionManager::Update() {
    // カメラの更新範囲に入っているかどうかのチェック
    CollisionManager::CheckUpdateRange();

    // 毎フレーム、オブジェクトを「動くもの」と「動かないブロック」に分ける
    std::vector<GameObject*> dynamics;
    std::vector<GameObject*> statics;

    auto isBlock = [](ObjectTag tag) {
        return tag == ObjectTag::BLOCK_NORMAL || tag == ObjectTag::BLOCK_GRASS ||
            tag == ObjectTag::BLOCK_STONE || tag == ObjectTag::BLOCK_SLIP_THROUGH ||
            tag == ObjectTag::BLOCK_DAMAGE || tag == ObjectTag::BLOCK_AIR ||
            tag == ObjectTag::BLOCK_WOOD || tag == ObjectTag::BLOCK_SIGN;
        };

    for (auto* obj : m_objects) {
        if (!obj || !obj->IsActive() || !obj->IsUpdatable()) continue;

        if (isBlock(obj->GetTag())) {
            statics.push_back(obj); // 動かないブロック
        }
        else {
            dynamics.push_back(obj); // プレイヤー、敵、ギミックなど
        }
    }

    // ========================================================
    // 「動くもの」同士の当たり判定
    // ========================================================
    for (size_t i = 0; i < dynamics.size(); ++i) {
        GameObject* objA = dynamics[i];
        ObjectTag tagA = objA->GetTag();

        for (size_t j = i + 1; j < dynamics.size(); ++j) {
            GameObject* objB = dynamics[j];
            ObjectTag tagB = objB->GetTag();

            // ギミック同士はスキップ
            bool isGimmickA = (tagA == ObjectTag::GIMMICK_SPEED_UP || tagA == ObjectTag::GIMMICK_GUILLOTINE || tagA == ObjectTag::GIMMICK_JUMP_PAD || tagA == ObjectTag::GIMMICK_STAKE || tagA == ObjectTag::GIMMICK_DOOR || tagA == ObjectTag::CHECKPOINT);
            bool isGimmickB = (tagB == ObjectTag::GIMMICK_SPEED_UP || tagB == ObjectTag::GIMMICK_GUILLOTINE || tagB == ObjectTag::GIMMICK_JUMP_PAD || tagB == ObjectTag::GIMMICK_STAKE || tagB == ObjectTag::GIMMICK_DOOR || tagB == ObjectTag::CHECKPOINT);
            if (isGimmickA && isGimmickB) continue;

            // 敵同士はスキップ
            bool isEnemyA = (tagA == ObjectTag::ENEMY_KATANA || tagA == ObjectTag::ENEMY_BOW || tagA == ObjectTag::ENEMY_KANABO || tagA == ObjectTag::ENEMY_YARI);
            bool isEnemyB = (tagB == ObjectTag::ENEMY_KATANA || tagB == ObjectTag::ENEMY_BOW || tagB == ObjectTag::ENEMY_KANABO || tagB == ObjectTag::ENEMY_YARI);
            if (isEnemyA && isEnemyB) continue;

            // プレイヤー vs ギロチンの特殊処理
            bool isPlayerVsGuillotine = (tagA == ObjectTag::PLAYER && tagB == ObjectTag::GIMMICK_GUILLOTINE) || (tagB == ObjectTag::PLAYER && tagA == ObjectTag::GIMMICK_GUILLOTINE);

            auto& collidersA = objA->GetColliders();
            auto& collidersB = objB->GetColliders();

            for (auto& colA : collidersA) {
                if (!colA.IsActive()) continue;
                for (auto& colB : collidersB) {
                    if (!colB.IsActive()) continue;

                    if (isPlayerVsGuillotine) {
                        GameObject* player = (tagA == ObjectTag::PLAYER) ? objA : objB;
                        GameObject* guillotine = (tagA == ObjectTag::PLAYER) ? objB : objA;
                        Collider* playerCol = (tagA == ObjectTag::PLAYER) ? &colA : &colB;
                        Collider* guillotineCol = (tagA == ObjectTag::PLAYER) ? &colB : &colA;

                        if (playerCol->GetType() != ColliderType::BODY_UD || guillotineCol->GetType() != ColliderType::GUILLOTINE_BLADE_TOP) continue;

                        DirectX::XMFLOAT2 pPos = playerCol->GetColliderPos();
                        DirectX::XMFLOAT2 pSize = playerCol->GetColliderSize();
                        DirectX::XMFLOAT2 gPos = guillotineCol->GetColliderPos();
                        DirectX::XMFLOAT2 gSize = guillotineCol->GetColliderSize();

                        if (pPos.x + pSize.x < gPos.x || pPos.x > gPos.x + gSize.x) continue;

                        float playerCurrentBottom = pPos.y + pSize.y;
                        bool isOnTop = (playerCurrentBottom >= gPos.y - 2.0f && playerCurrentBottom <= gPos.y + 2.0f);
                        bool isFallingOrStopped = (player->GetVelocity().y >= 0.0f);

                        if (isOnTop && isFallingOrStopped) {
                            player->OnCollision(guillotine, playerCol, guillotineCol, COLLISION_VEC::BOTTOM);
                            guillotine->OnCollision(player, guillotineCol, playerCol, COLLISION_VEC::TOP);
                        }
                        continue;
                    }

                    // 通常の衝突判定
                    if (m_collision.CheckBoxCollider(&colA, &colB)) {
                        COLLISION_VEC hitVecA = m_collision.CheckCollisionDirection(&colA, &colB);
                        COLLISION_VEC hitVecB = m_collision.CheckCollisionDirection(&colB, &colA);
                        objA->OnCollision(objB, &colA, &colB, hitVecA);
                        objB->OnCollision(objA, &colB, &colA, hitVecB);
                    }
                }
            }
        }
    }

    // ========================================================
    // 「動くもの」vs「ブロック」の判定
    // (ブロック同士の無駄なループをここで消滅)
    // ========================================================
    for (size_t i = 0; i < dynamics.size(); ++i) {
        GameObject* objA = dynamics[i];
        ObjectTag tagA = objA->GetTag();

        // ギミックはブロックと当たり判定しない（元の仕様を再現）
        bool isExcludedGimmick = (tagA == ObjectTag::GIMMICK_SPEED_UP || tagA == ObjectTag::GIMMICK_JUMP_PAD || tagA == ObjectTag::GIMMICK_STAKE || tagA == ObjectTag::GIMMICK_DOOR || tagA == ObjectTag::CHECKPOINT);
        if (isExcludedGimmick) continue;

        for (size_t j = 0; j < statics.size(); ++j) {
            GameObject* objB = statics[j]; // これは必ずブロック

            auto& collidersA = objA->GetColliders();
            auto& collidersB = objB->GetColliders();

            for (auto& colA : collidersA) {
                if (!colA.IsActive()) continue;
                for (auto& colB : collidersB) {
                    if (!colB.IsActive()) continue;

                    if (m_collision.CheckBoxCollider(&colA, &colB)) {
                        COLLISION_VEC hitVecA = m_collision.CheckCollisionDirection(&colA, &colB);
                        COLLISION_VEC hitVecB = m_collision.CheckCollisionDirection(&colB, &colA);
                        objA->OnCollision(objB, &colA, &colB, hitVecA);
                        objB->OnCollision(objA, &colB, &colA, hitVecB);
                    }
                }
            }
        }
    }
}

void CollisionManager::Draw() {
    if (!m_camera) return;

    std::vector<InstanceData> debugColliders;

    for (size_t i = 0; i < m_objects.size(); ++i) {
        GameObject* obj = m_objects[i];

        if (!obj || !obj->IsActive() || !obj->IsUpdatable()) continue;

        ObjectTag tag = obj->GetTag();
        if (tag == ObjectTag::BLOCK_NORMAL || tag == ObjectTag::BLOCK_GRASS ||
            tag == ObjectTag::BLOCK_STONE || tag == ObjectTag::BLOCK_SLIP_THROUGH ||
            tag == ObjectTag::BLOCK_WOOD || tag == ObjectTag::BLOCK_DAMAGE ||
            tag == ObjectTag::BLOCK_SIGN) continue;

        auto& colliders = obj->GetColliders();
        for (auto& col : colliders) {
            if (!col.IsActive()) continue;

            if (m_collision.CheckBoxCollider(m_camera->GetCollider(), &col)) {

                DirectX::XMFLOAT4 colColor = { 1.0f, 0.0f, 0.0f, 0.4f }; 

                if (col.GetType() == ColliderType::ATTACK) {
                    colColor = { 1.0f, 1.0f, 0.0f, 0.4f }; // 攻撃判定は黄色
                }
                else if (col.GetType() == ColliderType::BODY) {
                    colColor = { 0.0f, 1.0f, 0.0f, 0.4f }; // ボディ判定は緑
                }

                // リストに追加
                debugColliders.push_back({
                    col.GetColliderPos(),
                    col.GetColliderSize(),
                    colColor
                    });
            }
        }
    }

    // 集めたコライダーを一気に描画！（真っ白のテクスチャを使います）
    if (!debugColliders.empty()) {
        Sprite_DrawInstancedEx(TextureManager::Instance().Get(TexID::WHITE), debugColliders);
    }
}
//カメラの更新範囲に入っているかどうかのチェック
void CollisionManager::CheckUpdateRange()
{
    if (!m_camera) return;
    for (size_t i = 0; i < m_objects.size(); ++i) {
        GameObject* obj = m_objects[i];
        if (!obj) continue;

        auto& colliders = obj->GetColliders();
        for (auto& col : colliders) {
            if (!col.IsActive()) continue;

            if (m_collision.CheckBoxCollider(m_camera->GetCollider(), &col)) {
                // カメラの更新範囲内にある場合はアクティブにする
                obj->SetUpdatable(true);
            }
            else {
                // カメラの更新範囲外にある場合は非アクティブにする
                obj->SetUpdatable(false);
            }
        }
    }
}

