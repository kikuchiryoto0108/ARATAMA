//==============================================================================
//  File   : game_object.cpp
//  Brief  : トンネリング防止当たり判定
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/1/24
//------------------------------------------------------------------------------
//
//==============================================================================
#include "game_object.h"
#include "game_manager.h"
#include "game_stage.h"
#include <cmath>
#include <algorithm>

// 指定位置が衝突しているかチェックするヘルパー関数
static bool IsPositionBlocked(
    DirectX::XMFLOAT2 testPos,
    DirectX::XMFLOAT2 size,
    const std::vector<Block*>& blocks,
    const std::vector<const Collider*>& bossColliders,
    const std::vector<const Collider*>& projectileColliders,
    float stageLeft, float stageRight, float stageTop, float stageBottom) {
    // ステージ境界チェック
    if (testPos.x < stageLeft || testPos.x + size.x > stageRight ||
        testPos.y < stageTop || testPos.y + size.y > stageBottom) {
        return true;
    }

    // ブロックとの重複チェック
    for (auto* block : blocks) {
        if (!block || !block->IsActive()) continue;
        DirectX::XMFLOAT2 bPos = block->GetPosition();
        DirectX::XMFLOAT2 bSize = block->GetSize();

        if (testPos.x + size.x > bPos.x && testPos.x < bPos.x + bSize.x &&
            testPos.y + size.y > bPos.y && testPos.y < bPos.y + bSize.y) {
            return true;
        }
    }

    // ボス本体との重複チェック
    //for (const auto* col : bossColliders) {
    //    if (!col) continue;
    //    DirectX::XMFLOAT2 cPos = col->GetColliderPos();
    //    DirectX::XMFLOAT2 cSize = col->GetColliderSize();

    //    if (testPos.x + size.x > cPos.x && testPos.x < cPos.x + cSize.x &&
    //        testPos.y + size.y > cPos.y && testPos.y < cPos.y + cSize.y) {
    //        return true;
    //    }
    //}

    // ボスProjectileとの重複チェック
    //for (const auto* col : projectileColliders) {
    //    if (!col) continue;
    //    DirectX::XMFLOAT2 cPos = col->GetColliderPos();
    //    DirectX::XMFLOAT2 cSize = col->GetColliderSize();

    //    if (testPos.x + size.x > cPos.x && testPos.x < cPos.x + cSize.x &&
    //        testPos.y + size.y > cPos.y && testPos.y < cPos.y + cSize.y) {
    //        return true;
    //    }
    //}

    return false;
}

// 脱出位置を探すヘルパー関数
static bool FindEscapePosition(
    DirectX::XMFLOAT2 currentPos,
    DirectX::XMFLOAT2 size,
    const std::vector<Block*>& blocks,
    const std::vector<const Collider*>& bossColliders,
    const std::vector<const Collider*>& projectileColliders,
    float stageLeft, float stageRight, float stageTop, float stageBottom,
    DirectX::XMFLOAT2& outEscapePos,
    DirectX::XMFLOAT2& outEscapeVelocity) {
    const float stepSize = 16.0f;
    const float maxSearchDistance = 512.0f;

    struct EscapeCandidate {
        DirectX::XMFLOAT2 pos;
        DirectX::XMFLOAT2 velocity;
        float distance;
    };

    std::vector<EscapeCandidate> candidates;

    // 上方向を探索
    for (float dist = stepSize; dist <= maxSearchDistance; dist += stepSize) {
        DirectX::XMFLOAT2 testPos = { currentPos.x, currentPos.y - dist };
        if (!IsPositionBlocked(testPos, size, blocks, bossColliders, projectileColliders,
            stageLeft, stageRight, stageTop, stageBottom)) {
            candidates.push_back({ testPos, { 0.0f, -300.0f }, dist });
            break;
        }
    }

    // 下方向を探索
    for (float dist = stepSize; dist <= maxSearchDistance; dist += stepSize) {
        DirectX::XMFLOAT2 testPos = { currentPos.x, currentPos.y + dist };
        if (!IsPositionBlocked(testPos, size, blocks, bossColliders, projectileColliders,
            stageLeft, stageRight, stageTop, stageBottom)) {
            candidates.push_back({ testPos, { 0.0f, 300.0f }, dist });
            break;
        }
    }

    // 左方向を探索
    for (float dist = stepSize; dist <= maxSearchDistance; dist += stepSize) {
        DirectX::XMFLOAT2 testPos = { currentPos.x - dist, currentPos.y };
        if (!IsPositionBlocked(testPos, size, blocks, bossColliders, projectileColliders,
            stageLeft, stageRight, stageTop, stageBottom)) {
            candidates.push_back({ testPos, { -300.0f, 0.0f }, dist });
            break;
        }
    }

    // 右方向を探索
    for (float dist = stepSize; dist <= maxSearchDistance; dist += stepSize) {
        DirectX::XMFLOAT2 testPos = { currentPos.x + dist, currentPos.y };
        if (!IsPositionBlocked(testPos, size, blocks, bossColliders, projectileColliders,
            stageLeft, stageRight, stageTop, stageBottom)) {
            candidates.push_back({ testPos, { 300.0f, 0.0f }, dist });
            break;
        }
    }

    // 斜め方向も探索
    // 左上
    for (float dist = stepSize; dist <= maxSearchDistance; dist += stepSize) {
        DirectX::XMFLOAT2 testPos = { currentPos.x - dist, currentPos.y - dist };
        if (!IsPositionBlocked(testPos, size, blocks, bossColliders, projectileColliders,
            stageLeft, stageRight, stageTop, stageBottom)) {
            candidates.push_back({ testPos, { -200.0f, -200.0f }, dist * 1.414f });
            break;
        }
    }

    // 右上
    for (float dist = stepSize; dist <= maxSearchDistance; dist += stepSize) {
        DirectX::XMFLOAT2 testPos = { currentPos.x + dist, currentPos.y - dist };
        if (!IsPositionBlocked(testPos, size, blocks, bossColliders, projectileColliders,
            stageLeft, stageRight, stageTop, stageBottom)) {
            candidates.push_back({ testPos, { 200.0f, -200.0f }, dist * 1.414f });
            break;
        }
    }

    // 左下
    for (float dist = stepSize; dist <= maxSearchDistance; dist += stepSize) {
        DirectX::XMFLOAT2 testPos = { currentPos.x - dist, currentPos.y + dist };
        if (!IsPositionBlocked(testPos, size, blocks, bossColliders, projectileColliders,
            stageLeft, stageRight, stageTop, stageBottom)) {
            candidates.push_back({ testPos, { -200.0f, 200.0f }, dist * 1.414f });
            break;
        }
    }

    // 右下
    for (float dist = stepSize; dist <= maxSearchDistance; dist += stepSize) {
        DirectX::XMFLOAT2 testPos = { currentPos.x + dist, currentPos.y + dist };
        if (!IsPositionBlocked(testPos, size, blocks, bossColliders, projectileColliders,
            stageLeft, stageRight, stageTop, stageBottom)) {
            candidates.push_back({ testPos, { 200.0f, 200.0f }, dist * 1.414f });
            break;
        }
    }

    // 一番近い脱出位置を選択
    if (candidates.empty()) {
        return false;
    }

    EscapeCandidate* best = &candidates[0];
    for (auto& candidate : candidates) {
        if (candidate.distance < best->distance) {
            best = &candidate;
        }
    }

    outEscapePos = best->pos;
    outEscapeVelocity = best->velocity;
    return true;
}

void GameObject::Move() {
    float deltaTime = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());
    DirectX::XMFLOAT2 desiredMove = {
        m_velocity.x * deltaTime,
        m_velocity.y * deltaTime
    };

    if (fabsf(desiredMove.x) < 0.001f && fabsf(desiredMove.y) < 0.001f) {
        return;
    }

    if (m_tag == ObjectTag::BLOCK_NORMAL ||
		m_tag == ObjectTag::BLOCK_GRASS ||
		m_tag == ObjectTag::BLOCK_STONE ||
		m_tag == ObjectTag::BLOCK_WOOD ||
		m_tag == ObjectTag::BLOCK_AIR ||
        m_tag == ObjectTag::BULLET ||
        m_tag == ObjectTag::WEAPON ||
        m_tag == ObjectTag::CHECKPOINT ||
        m_tag == ObjectTag::BOSS ||
        m_tag == ObjectTag::BOSS_MASK ||
        m_tag == ObjectTag::BOSS_PROJECTILE) {
        m_position.x += desiredMove.x;
        m_position.y += desiredMove.y;
        return;
    }

    Stage* stage = GameManager::Instance().GetStage();
    if (!stage) {
        m_position.x += desiredMove.x;
        m_position.y += desiredMove.y;
        return;
    }

    DirectX::XMFLOAT2 size = m_colliderSize;
    DirectX::XMFLOAT2 colOff = m_colliderOffset;
    DirectX::XMFLOAT2 prevPos = m_position;
    DirectX::XMFLOAT2 finalPos = prevPos;

    float padding = 128.0f;
    float minX = std::min(prevPos.x, prevPos.x + desiredMove.x) - padding;
    float maxX = std::max(prevPos.x + size.x, prevPos.x + size.x + desiredMove.x) + padding;
    float minY = std::min(prevPos.y, prevPos.y + desiredMove.y) - padding;
    float maxY = std::max(prevPos.y + size.y, prevPos.y + size.y + desiredMove.y) + padding;

    std::vector<Block*> nearbyBlocks;
    stage->GetBlocksInRange(minX, minY, maxX, maxY, nearbyBlocks);

    std::vector<GameGimmick*> nearbyGimmicks;
    stage->GetGimmicksInRange(minX, minY, maxX, maxY, nearbyGimmicks);

    // ボス関連のコライダーを取得
    std::vector<const Collider*> bossColliders;
    std::vector<const Collider*> bossProjectileColliders;
    GameManager::Instance().GetBossColliders(bossColliders);
    GameManager::Instance().GetBossProjectileColliders(bossProjectileColliders);

    // ステージ境界（挟まれ対策用）
    float stageLeft = 64.0f;
    float stageRight = stage->GetWidth() * 64.0f - 64.0f;
    float stageTop = -500.0f;
    float stageBottom = stage->GetHeight() * 64.0f - 64.0f;

    // ===== X軸の移動と衝突判定 =====
    finalPos.x = prevPos.x + desiredMove.x;

    // ブロックとの衝突
    for (auto* block : nearbyBlocks) {
        if (!block || !block->IsActive()) continue;

        DirectX::XMFLOAT2 bPos = block->GetPosition();
        DirectX::XMFLOAT2 bSize = block->GetSize();

        if (finalPos.y + colOff.y + size.y > bPos.y && finalPos.y + colOff.y < bPos.y + bSize.y) {
            if (finalPos.x + colOff.x + size.x > bPos.x && finalPos.x + colOff.x < bPos.x + bSize.x) {
                if (desiredMove.x > 0) {
                    finalPos.x = bPos.x - size.x - colOff.x;
                } else if (desiredMove.x < 0) {
                    finalPos.x = bPos.x + bSize.x - colOff.x;
                }
                m_velocity.x = 0.0f;
            }
        }
    }

    // ギミックとの衝突（X軸）
    for (auto* gimmick : nearbyGimmicks) {
        if (!gimmick || !gimmick->IsActive()) continue;

        auto type = gimmick->GetGimmickType();

        // ギロチンはX軸判定をスキップ（すり抜け）
        if (type == GameGimmick::Type::GUILLOTINE) {
            continue;
        }

        if (type != GameGimmick::Type::JUMP_PAD ){
            continue;
        }

        if (type == GameGimmick::Type::STAKE)  continue;

        const auto& colliders = gimmick->GetColliders();
        for (const auto& col : colliders) {
            if (!col.IsActive()) continue;

            ColliderType colType = col.GetType();
            if (colType != ColliderType::BODY) continue;

            DirectX::XMFLOAT2 cPos = col.GetColliderPos();
            DirectX::XMFLOAT2 cSize = col.GetColliderSize();

            if (finalPos.y + colOff.y + size.y > cPos.y && finalPos.y + colOff.y < cPos.y + cSize.y) {
                if (finalPos.x + colOff.x + size.x > cPos.x && finalPos.x + colOff.x < cPos.x + cSize.x) {
                    if (desiredMove.x > 0) {
                        finalPos.x = cPos.x - size.x - colOff.x;
                    } else if (desiredMove.x < 0) {
                        finalPos.x = cPos.x + cSize.x - colOff.x;
                    }
                    m_velocity.x = 0.0f;
                }
            }
        }
    }


    // ボス本体との衝突（X軸）
    //for (const auto* col : bossColliders) {
    //    if (!col) continue;

    //    DirectX::XMFLOAT2 cPos = col->GetColliderPos();
    //    DirectX::XMFLOAT2 cSize = col->GetColliderSize();

    //    if (finalPos.y + colOff.y + size.y > cPos.y && finalPos.y + colOff.y < cPos.y + cSize.y) {
    //        if (finalPos.x + colOff.x + size.x > cPos.x && finalPos.x + colOff.x < cPos.x + cSize.x) {
    //            if (desiredMove.x > 0) {
    //                finalPos.x = cPos.x - size.x - colOff.x;
    //            } else if (desiredMove.x < 0) {
    //                finalPos.x = cPos.x + cSize.x - colOff.x;
    //            }
    //            m_velocity.x = 0.0f;
    //        }
    //    }
    //}

    // ボスのProjectileとの衝突（X軸）
    //for (const auto* col : bossProjectileColliders) {
    //    if (!col) continue;

    //    DirectX::XMFLOAT2 cPos = col->GetColliderPos();
    //    DirectX::XMFLOAT2 cSize = col->GetColliderSize();

    //    if (finalPos.y + colOff.y + size.y > cPos.y && finalPos.y + colOff.y < cPos.y + cSize.y) {
    //        if (finalPos.x + colOff.x + size.x > cPos.x && finalPos.x + colOff.x < cPos.x + cSize.x) {
    //            if (desiredMove.x > 0) {
    //                finalPos.x = cPos.x - size.x - colOff.x;
    //            } else if (desiredMove.x < 0) {
    //                finalPos.x = cPos.x + cSize.x - colOff.x;
    //            }
    //            m_velocity.x = 0.0f;
    //        }
    //    }
    //}

    // ===== Y軸の移動と衝突判定 =====
    finalPos.y = prevPos.y + desiredMove.y;

    // ブロックとの衝突
    for (auto* block : nearbyBlocks) {
        if (!block || !block->IsActive()) continue;

        DirectX::XMFLOAT2 bPos = block->GetPosition();
        DirectX::XMFLOAT2 bSize = block->GetSize();

        if (finalPos.y + colOff.y + size.y > bPos.y && finalPos.y + colOff.y < bPos.y + bSize.y) {
            if (finalPos.x + colOff.x + size.x > bPos.x && finalPos.x + colOff.x < bPos.x + bSize.x) {
                if (desiredMove.y > 0) {
                    finalPos.y = bPos.y - size.y - colOff.y;
                } else if (desiredMove.y < 0) {
                    finalPos.y = bPos.y + bSize.y - colOff.y;
                }
                m_velocity.y = 0.0f;
            }
        }
    }


    // すり抜けブロックとの衝突（敵のみ：上から乗る場合）
    bool isEnemy = (m_tag == ObjectTag::ENEMY_KATANA ||
        m_tag == ObjectTag::ENEMY_BOW ||
        m_tag == ObjectTag::ENEMY_KANABO ||
        m_tag == ObjectTag::ENEMY_YARI);

    if (isEnemy) {
        std::vector<Block*> slipThroughBlocks;
        stage->GetSlipThroughBlocksInRange(minX, minY, maxX, maxY, slipThroughBlocks);

        for (auto* block : slipThroughBlocks) {
            if (!block || !block->IsActive()) continue;

            DirectX::XMFLOAT2 bPos = block->GetPosition();
            DirectX::XMFLOAT2 bSize = block->GetSize();

            // 下方向に移動中（落下中）のみ判定
            if (desiredMove.y > 0) {
                // 前フレームで上にいたかチェック
                float prevBottom = prevPos.y + colOff.y + size.y;

                if (prevBottom <= bPos.y + 4.0f) {  // 敵用に許容値を広めに
                    if (finalPos.y + colOff.y + size.y > bPos.y && finalPos.y + colOff.y < bPos.y + bSize.y) {
                        if (finalPos.x + colOff.x + size.x > bPos.x && finalPos.x + colOff.x < bPos.x + bSize.x) {
                            finalPos.y = bPos.y - size.y - colOff.y;
                            m_velocity.y = 0.0f;
                        }
                    }
                }
            }
        }
    }

    // ギミックとの衝突（Y軸）
    for (auto* gimmick : nearbyGimmicks) {
        if (!gimmick || !gimmick->IsActive()) continue;

        auto type = gimmick->GetGimmickType();
        if (type != GameGimmick::Type::JUMP_PAD &&
            type != GameGimmick::Type::GUILLOTINE) {
            continue;
        }

        if(type == GameGimmick::Type::STAKE)  continue;

        const auto& colliders = gimmick->GetColliders();
        for (const auto& col : colliders) {
            if (!col.IsActive()) continue;

            ColliderType colType = col.GetType();
            bool shouldCheck = false;

            if (type == GameGimmick::Type::GUILLOTINE) {
                // ギロチンはBLADE_TOPのみ
                shouldCheck = (colType == ColliderType::GUILLOTINE_BLADE_TOP);
            } else {
                shouldCheck = (colType == ColliderType::BODY);
            }

            if (!shouldCheck) continue;

            DirectX::XMFLOAT2 cPos = col.GetColliderPos();
            DirectX::XMFLOAT2 cSize = col.GetColliderSize();

            if (finalPos.y + colOff.y + size.y > cPos.y && finalPos.y + colOff.y < cPos.y + cSize.y) {
                if (finalPos.x + colOff.x + size.x > cPos.x && finalPos.x + colOff.x < cPos.x + cSize.x) {

                    // ギロチンはすり抜け床として処理
                    if (type == GameGimmick::Type::GUILLOTINE) {
                        float prevBottom = prevPos.y + colOff.y + size.y;
                        if (desiredMove.y > 0 && prevBottom <= cPos.y + 1.0f) {
                            finalPos.y = cPos.y - size.y - colOff.y;;
                            m_velocity.y = 0.0f;
                        }
                    } else {
                        // 通常のギミック（ジャンプパッド、杭）
                        if (desiredMove.y > 0) {
                            finalPos.y = cPos.y - size.y - colOff.y;
                        } else if (desiredMove.y < 0) {
                            finalPos.y = cPos.y + cSize.y - colOff.y;
                        }
                        m_velocity.y = 0.0f;
                    }
                }
            }
        }
    }


    // ボス本体との衝突（Y軸）
    //for (const auto* col : bossColliders) {
    //    if (!col) continue;

    //    DirectX::XMFLOAT2 cPos = col->GetColliderPos();
    //    DirectX::XMFLOAT2 cSize = col->GetColliderSize();

    //    if (finalPos.y + colOff.y + size.y > cPos.y && finalPos.y + colOff.y < cPos.y + cSize.y) {
    //        if (finalPos.x + colOff.x + size.x > cPos.x && finalPos.x + colOff.x < cPos.x + cSize.x) {
    //            if (desiredMove.y > 0) {
    //                finalPos.y = cPos.y - size.y - colOff.y;
    //            } else if (desiredMove.y < 0) {
    //                finalPos.y = cPos.y + cSize.y - colOff.y;
    //            }
    //            m_velocity.y = 0.0f;
    //        }
    //    }
    //}

    // ボスのProjectileとの衝突（Y軸）- 進入方向の逆に押し出す
    //for (const auto* col : bossProjectileColliders) {
    //    if (!col) continue;

    //    DirectX::XMFLOAT2 cPos = col->GetColliderPos();
    //    DirectX::XMFLOAT2 cSize = col->GetColliderSize();

    //    if (finalPos.y + colOff.y + size.y > cPos.y && finalPos.y + colOff.y < cPos.y + cSize.y) {
    //        if (finalPos.x + colOff.x + size.x > cPos.x && finalPos.x + colOff.x < cPos.x + cSize.x) {
    //            // プレイヤーの移動方向で判定
    //            if (desiredMove.y < 0) {
    //                // 上に移動中（下から突っ込んだ）→ 下に押し出す
    //                finalPos.y = cPos.y + cSize.y - colOff.y;
    //            } else {
    //                // 下に移動中（上から落ちた）→ 上に押し出す
    //                finalPos.y = cPos.y - size.y - colOff.y;
    //            }
    //            m_velocity.y = 0.0f;
    //        }
    //    }
    //}

    // ===== 挟まれ対策：押し出し後も衝突している場合の補正 =====
    if (m_tag == ObjectTag::PLAYER) {
        // ギロチンと重なっているかチェック
        bool isInsideGuillotine = false;
        for (auto* gimmick : nearbyGimmicks) {
            if (!gimmick || !gimmick->IsActive()) continue;
            if (gimmick->GetGimmickType() != GameGimmick::Type::GUILLOTINE) continue;

            // ギミック本体の位置とサイズでチェック
            DirectX::XMFLOAT2 gPos = gimmick->GetPosition();
            DirectX::XMFLOAT2 gSize = gimmick->GetSize();

            if (finalPos.x + colOff.x + size.x > gPos.x && finalPos.x + colOff.x < gPos.x + gSize.x &&
                finalPos.y + colOff.y + size.y > gPos.y && finalPos.y + colOff.y < gPos.y + gSize.y) {
                isInsideGuillotine = true;
                break;
            }
        }

        // ギロチン内にいる場合は埋まり対策スキップ
        if (!isInsideGuillotine) {

            DirectX::XMFLOAT2 colPos = { finalPos.x + colOff.x, finalPos.y + colOff.y };

            // 現在位置がまだ衝突しているかチェック
            bool stillBlocked = IsPositionBlocked(
                colPos, size, nearbyBlocks, bossColliders, bossProjectileColliders,
                stageLeft, stageRight, stageTop, stageBottom);

            if (stillBlocked) {
                // 脱出位置を探す
                DirectX::XMFLOAT2 escapePos;
                DirectX::XMFLOAT2 escapeVelocity;

                if (FindEscapePosition(
                    colPos, size, nearbyBlocks, bossColliders, bossProjectileColliders,
                    stageLeft, stageRight, stageTop, stageBottom,
                    escapePos, escapeVelocity)) {
                    finalPos.x = escapePos.x - colOff.x;
                    finalPos.y = escapePos.y - colOff.y;
                    m_velocity = escapeVelocity;
                } else {
                    // 脱出不可能な場合は前の位置に戻す
                    finalPos = prevPos;
                    m_velocity.x = 0.0f;
                    m_velocity.y = 0.0f;
                }
            }
        }

        // 最終的なステージ境界クランプ
        if (finalPos.x + colOff.x < stageLeft) {
            finalPos.x = stageLeft - colOff.x;
        }
        if (finalPos.x + colOff.x + size.x > stageRight) {
            finalPos.x = stageRight - size.x - colOff.x;
        }
        if (finalPos.y + colOff.y < stageTop) {
            finalPos.y = stageTop - colOff.y;
        }
        // 下方向は落下死判定があるのでクランプしない
    }

    m_position = finalPos;
}



void GameObject::MoveX() {
    float deltaTime = (float)TimeManager::GetInstance().GetScaledElapsedTime();
    m_position.x += m_velocity.x * deltaTime;
}

void GameObject::MoveY() {
    float deltaTime = (float)TimeManager::GetInstance().GetScaledElapsedTime();
    m_position.y += m_velocity.y * deltaTime;
}
