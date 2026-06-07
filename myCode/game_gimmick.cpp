/*****************************************************************//**
 * \file   game_gimmick.cpp
 * \brief  ゲームギミック
 *
 * \author 菊池凌斗
 * \date   2026/1/18
 * 基本的な機能のギミック基底クラス->派生クラスで各種ギミック実装
 *********************************************************************/
#include "game_gimmick.h"
#include "texture.h"
#include "sprite.h"
#include "CollisionManager.h"
#include "input_manager.h"
#include "game_player.h"
#include "game_enemy.h"
#include "weapon.h"
#include "game_enemy_katana.h"
#include "game_enemy_kanabo.h"
#include "game_enemy_bow.h"
#include "weapon_bow_bullet.h"
#include "direct3d.h"
#include <cmath>
#include <random>
#include "Audio.h"
#include "game_enemy_spear.h"
#include "sprite_anim.h"
using namespace DirectX;

void GameGimmick::Initialize() {
	CollisionManager::GetInstance().AddObject(this);
}

void GameGimmick::Finalize() {
	CollisionManager::GetInstance().RemoveObject(this);
}

void GameGimmick::Update() {
	UpdateColliderPosition();
}

void GameGimmick::Draw() {
}

void GameGimmick::OnCollision(const GameObject* otherObject, const Collider* myCollider, const Collider* otherCollider, COLLISION_VEC hitVec) {
}

//==============================================================================
// SpeedUpGimmick - スピードアップギミック
//==============================================================================

void SpeedUpGimmick::Initialize() {
	// 横
	m_texFrontH = TextureManager::Instance().Get(TexID::GIMMICK_SPEEDUP_HORIZONTAL_FRONT);
	m_texBackH = TextureManager::Instance().Get(TexID::GIMMICK_SPEEDUP_HORIZONTAL_BACK);
	// 縦
	m_texFrontV = TextureManager::Instance().Get(TexID::GIMMICK_SPEEDUP_VERTICAL_FRONT);
	m_texBackV = TextureManager::Instance().Get(TexID::GIMMICK_SPEEDUP_VERTICAL_BACK);

	// コライダー再設定
	GetColliders().clear();
	m_bodyCollider = nullptr;

	AddCollider(
		{ GetPosition().x + m_effectRegionOffset.x, GetPosition().y + m_effectRegionOffset.y },
		m_effectRegionSize,
		ColliderType::BODY
	);
	CollisionManager::GetInstance().AddObject(this);
}

void SpeedUpGimmick::Finalize() {
	CollisionManager::GetInstance().RemoveObject(this);
}

void SpeedUpGimmick::Update() {
	if (m_cooldownTimer > 0.0f) {
		m_cooldownTimer -= (float)TimeManager::GetInstance().GetElapsedTime();
		if (m_cooldownTimer < 0.0f) {
			m_cooldownTimer = 0.0f;
		}
	}

	UpdateColliderPosition();
}

void SpeedUpGimmick::Draw() {
	if (!IsActive()) return;

	Direct3D_SetAlphaBlend(BLEND_ADD);

	int texBack = (m_direction == Direction::Horizontal) ? m_texBackH : m_texBackV;
	int texFront = (m_direction == Direction::Horizontal) ? m_texFrontH : m_texFrontV;

	// 背面
	if (texBack >= 0)
		Sprite_ScrollDraw(texBack, GetPosition().x, GetPosition().y, GetSize().x, GetSize().y);

	// 前面
	if (texFront >= 0)
		Sprite_ScrollDraw(texFront, GetPosition().x, GetPosition().y, GetSize().x, GetSize().y);

	Direct3D_SetAlphaBlend(BLEND_TRANSPARENT);
}

void SpeedUpGimmick::DrawFront() {
	if (!IsActive()) return;

	Direct3D_SetAlphaBlend(BLEND_ADD);

	int texId = (m_direction == Direction::Horizontal) ? m_texFrontH : m_texFrontV;
	if (texId >= 0)
		Sprite_ScrollDraw(texId, GetPosition().x, GetPosition().y, GetSize().x, GetSize().y);

	Direct3D_SetAlphaBlend(BLEND_TRANSPARENT);
}

void SpeedUpGimmick::OnCollision(const GameObject* otherObject, const Collider* myCollider, const Collider* otherCollider, COLLISION_VEC hitVec) {
	if (!otherObject || !IsActive()) return;
	if (otherObject->GetTag() != ObjectTag::PLAYER) return;
	if (m_cooldownTimer > 0.0f) return;

	Player* player = dynamic_cast<Player*>(const_cast<GameObject*>(otherObject));
	if (!player) return;

	if (!Audio_IsPlayingSE(AudioID::SE_SPEED_UP)) {
		Audio_PlaySE(AudioID::SE_SPEED_UP);
	}

	if (m_direction == Direction::Horizontal) {
		float vx = player->GetVelocityX();
		int dir = (vx > 0.1f) ? 1 : (vx < -0.1f) ? -1 : (player->GetPosition().x < GetPosition().x + GetSize().x / 2.f ? 1 : -1);
		player->ApplyImpulseX(vx + dir * BOOST_VELOCITY_X);
	} else {
		float vy = player->GetVelocityY();
		int dir = (vy > 0.1f) ? 1 : (vy < -0.1f) ? -1 : (player->GetPosition().y < GetPosition().y + GetSize().y / 2.f ? 1 : -1);
		player->ApplyImpulseY(vy + dir * BOOST_VELOCITY_Y);
	}
	m_cooldownTimer = APPLY_COOLDOWN;
}

//==============================================================================
// JumpPadGimmick - ジャンプ台ギミック
//==============================================================================

void JumpPadGimmick::Initialize() {
	m_textureId = TextureManager::Instance().Get(TexID::GIMMICK_JUMPPAD);
	// コライダーをオフセット付きで設定
	GetColliders().clear();
	AddCollider(
		{ GetPosition().x, GetPosition().y + OFFSET_Y },
		{ DRAW_WIDTH, DRAW_HEIGHT },
		ColliderType::BODY
	);

	CollisionManager::GetInstance().AddObject(this);
}

void JumpPadGimmick::Finalize() {
	CollisionManager::GetInstance().RemoveObject(this);
}

void JumpPadGimmick::Update() {
	if (!GetColliders().empty()) {
		GetColliders()[0].SetColliderPos({ GetPosition().x, GetPosition().y + OFFSET_Y });
	}
}

void JumpPadGimmick::Draw() {
	if (!IsActive()) return;
	if (m_textureId < 0) return;

	Sprite_ScrollDraw(
		m_textureId,
		GetPosition().x,
		GetPosition().y + OFFSET_Y,
		DRAW_WIDTH,
		DRAW_HEIGHT,
		0.0f, 0.0f,
		TEXTURE_WIDTH, TEXTURE_HEIGHT,
		{ 1.0f, 1.0f, 1.0f, 1.0f }
	);
}

void JumpPadGimmick::OnCollision(
	const GameObject* otherObject,
	const Collider* myCollider,
	const Collider* otherCollider,
	COLLISION_VEC hitVec
) {
	if (!otherObject || !IsActive()) return;
	if (otherObject->GetTag() != ObjectTag::PLAYER) return;

	GameObject* other = const_cast<GameObject*>(otherObject);
	Player* p = dynamic_cast<Player*>(other);
	if (!p) return;
    
    // プレイヤーの足元がジャンプパッドの上面付近にあるかチェック
    float playerBottom = p->GetPosition().y + p->GetSize().y;
    float padTop = GetPosition().y + OFFSET_Y;
    
    // 落下中（VelocityY >= 0）で、足元がパッド上面付近
    if (playerBottom <= padTop + 10.0f && p->IsJumpPad() == true) {
        p->ApplyImpulseY(JUMP_POWER_BOOST);
        Audio_PlaySE(AudioID::SE_JUMP_PAD);
    }
}

//==============================================================================
// DoorGimmick - ドアギミック
//==============================================================================
void DoorGimmick::Initialize() {
	m_textureId = TextureManager::Instance().Get(TexID::GIMMICK_DOOR_CLOSED);
	m_openTextureId = TextureManager::Instance().Get(TexID::GIMMICK_DOOR_OPEN);
	m_barrierTextureId = TextureManager::Instance().Get(TexID::BOSS_BARRIER);
	m_playerEntered = false;

	m_isOpen = false;
	m_playerEntered = false;

	// バリア破壊アニメーション作成
	if (!m_barrierAnim) {
		m_barrierAnim = new AnimPattern(
			m_barrierTextureId,
			BARRIER_ANIM_TOTAL,
			BARRIER_ANIM_COLS,
			0.05f,
			{ 0, 0 },
			{ static_cast<uint32_t>(BARRIER_FRAME_WIDTH), static_cast<uint32_t>(BARRIER_FRAME_HEIGHT) },
			false,
			false
		);
	}
	if (!m_barrierAnimPlayer) {
		m_barrierAnimPlayer = new AnimPatternPlayer(m_barrierAnim);
	}
	m_isBarrierAnimPlaying = false;

	float colliderSize = DRAW_SIZE * COLLIDER_RATIO;
	float colliderOffsetX = (DRAW_SIZE - colliderSize) * 0.5f;

	float doorBottom = GetPosition().y + 64.0f;

	float colliderX = GetPosition().x + colliderOffsetX;
	float colliderY = doorBottom - colliderSize;

	GetColliders().clear();
	AddCollider({ colliderX, colliderY }, { colliderSize, colliderSize }, ColliderType::BODY);
	CollisionManager::GetInstance().AddObject(this);
}

void DoorGimmick::Finalize() {
	CollisionManager::GetInstance().RemoveObject(this);

	// バリアアニメーション解放
	if (m_barrierAnimPlayer) {
		delete m_barrierAnimPlayer;
		m_barrierAnimPlayer = nullptr;
	}
	if (m_barrierAnim) {
		delete m_barrierAnim;
		m_barrierAnim = nullptr;
	}
}

void DoorGimmick::Update() {
	// バリア破壊アニメーション更新
	if (m_isBarrierAnimPlaying && m_barrierAnimPlayer) {
		m_barrierAnimPlayer->Update();
		if (m_barrierAnimPlayer->IsEnd()) {
			m_isBarrierAnimPlaying = false;
		}
	}
}

void DoorGimmick::Draw() {
	if (!IsActive()) return;

	int texId = m_isOpen ? m_openTextureId : m_textureId;

	if (texId >= 0) {
		float doorBottom = GetPosition().y + 64.0f;
		float drawY = doorBottom - DRAW_SIZE + 26.0f;

		Sprite_ScrollDraw(texId, GetPosition().x, drawY, DRAW_SIZE, DRAW_SIZE);
	}

	// バリア描画（アニメーション対応）
	if (m_barrierTextureId >= 0) {
		float doorBottom = GetPosition().y + 64.0f;
		float drawY = doorBottom - DRAW_SIZE + 26.0f;

		float barrierW = DRAW_SIZE + 32.0f;
		float barrierH = DRAW_SIZE + 32.0f;
		float barrierX = GetPosition().x - 16.0f;
		float barrierY = drawY - 16.0f;

		if (m_isBarrierAnimPlaying && m_barrierAnimPlayer) {
			// 破壊アニメーション再生中
			int pattern = m_barrierAnimPlayer->GetPattern();
			int col = pattern % BARRIER_ANIM_COLS;
			int row = pattern / BARRIER_ANIM_COLS;

			float uvX = col * BARRIER_FRAME_WIDTH;
			float uvY = row * BARRIER_FRAME_HEIGHT;

			Sprite_ScrollDraw(m_barrierTextureId,
				barrierX, barrierY,
				barrierW, barrierH,
				uvX, uvY,
				BARRIER_FRAME_WIDTH, BARRIER_FRAME_HEIGHT,
				0.0f,
				{ 1.0f, 1.0f, 1.0f, 1.0f });
		} else if (m_isLocked) {
			// ロック中：静止バリア表示
			Sprite_ScrollDraw(m_barrierTextureId,
				barrierX, barrierY,
				barrierW, barrierH,
				0.0f, 0.0f,
				BARRIER_FRAME_WIDTH, BARRIER_FRAME_HEIGHT,
				0.0f,
				{ 1.0f, 1.0f, 1.0f, 0.7f });
		}
	}
}

void DoorGimmick::OnCollision(const GameObject* otherObject, const Collider* myCollider, const Collider* otherCollider, COLLISION_VEC hitVec) {
	if (!otherObject) return;

	if (otherObject->GetTag() == ObjectTag::PLAYER) {
		// ロック中は入れない
		if (m_isLocked) return;

		Player* player = dynamic_cast<Player*>(const_cast<GameObject*>(otherObject));

		if (m_isOpen) {
			if ((InputManager::Cmd().up || InputManager::Cmd().enter) && (player->GetState() == Player::PLAYER_STATE_IDLE || player->GetState() == Player::PLAYER_STATE_WALK)) {
				Audio_PlaySE(AudioID::SE_DOOR_CLOSE);
				m_playerEntered = true;
			}
		}
	}
}

// SetLocked実装
void DoorGimmick::SetLocked(bool locked) {
	// ロック解除時にバリア破壊アニメーション再生
	if (m_isLocked && !locked) {
		if (m_barrierAnimPlayer) {
			m_barrierAnimPlayer->Reset();
			m_isBarrierAnimPlaying = true;
		}
		Audio_PlaySE(AudioID::SE_BARRIER_BREAK);
	}
	m_isLocked = locked;
}


//==============================================================================
// StakeGimmick - 杭ギミック
//==============================================================================
void StakeGimmick::Initialize() {
	// 仮面モードならマスクテクスチャ、通常なら杭テクスチャ
	if (m_isMaskMode) {
		std::random_device rd;
		std::mt19937 gen(rd());

		std::uniform_int_distribution<int> dis(0, 3);

		BossMask::MaskType random = static_cast<BossMask::MaskType>(dis(gen));

		switch (random)
		{
		case BossMask::MaskType::KATANA:
			m_textureId = TextureManager::Instance().Get(TexID::BOSS_MASK_KATANA);
			break;
		case BossMask::MaskType::KANABO:
			m_textureId = TextureManager::Instance().Get(TexID::BOSS_MASK_KANABO);
			break;
		case BossMask::MaskType::BOW:
			m_textureId = TextureManager::Instance().Get(TexID::BOSS_MASK_BOW);
			break;
		case BossMask::MaskType::SPEAR:
			m_textureId = TextureManager::Instance().Get(TexID::BOSS_MASK_SPEAR);
			break;
		default:
			m_textureId = TextureManager::Instance().Get(TexID::BOSS_MASK_KATANA);
			break;
		}

		m_type = random;

		m_barrierTextureId = TextureManager::Instance().Get(TexID::BOSS_BARRIER);
	} else {
		m_textureId = TextureManager::Instance().Get(TexID::GIMMICK_STAKE);
	}
	m_isActivated = false;

	GetColliders().clear();
	AddCollider(GetPosition(), GetSize(), ColliderType::BODY);

	CollisionManager::GetInstance().AddObject(this);
}

void StakeGimmick::Draw() {
	if (!IsActive()) return;
	if (m_textureId >= 0) {
		Sprite_ScrollDraw(m_textureId, GetPosition().x, GetPosition().y, GetSize().x, GetSize().y);
	}
}

void StakeGimmick::OnHitByWeapon() {
	if (m_isActivated) return;

	m_isActivated = true;

	// 仮面モードならマスク破壊音、通常なら杭音
	if (m_isMaskMode) {
		Audio_PlaySE(AudioID::SE_MASK_BREAK, 0.7f);
	} else {
		Audio_PlaySE(AudioID::SE_STAKE, 0.7f);
	}

	// ドアを開く
	if (m_linkedDoor) {
		m_linkedDoor->Open();
		m_linkedDoor->SetLocked(false);  // バリア解除
	}

	// 連動する壁をすべて消す
	for (auto* wall : m_linkedWalls) {
		if (wall) {
			wall->Destroy();
		}
	}

	// 連動するNextStageのバリアを解除
	for (auto* ns : m_linkedNextStages) {
		if (ns) {
			ns->SetLocked(false);
		}
	}

	// 仮面モードなら非表示にする
	if (m_isMaskMode) {
		SetActive(false);
		for (auto& col : GetColliders()) {
			col.SetActive(false);
		}
	}
}

void StakeGimmick::Finalize() {
	CollisionManager::GetInstance().RemoveObject(this);
}

void StakeGimmick::Update() {
	// 杭/仮面は特に毎フレーム処理なし
}

void StakeGimmick::OnCollision(const GameObject* otherObject, const Collider* myCollider, const Collider* otherCollider, COLLISION_VEC hitVec) {
	if (!otherObject || !otherCollider) return;

	// コライダー種別判定（ATTACK かつ相手Weapon種のときのみ通す）
	if (otherCollider->GetType() == ColliderType::ATTACK) {
		auto weapon = dynamic_cast<const Weapon*>(otherObject);
		if (weapon && (weapon->GetWeaponType() == Weapon::Type::KATANA || weapon->GetWeaponType() == Weapon::Type::KANABO || weapon->GetWeaponType() == Weapon::Type::BOW || weapon->GetWeaponType() == Weapon::Type::SPEAR)) {
			Player* player = dynamic_cast<Player*>(weapon->GetOwner());

			if (player) 
			{
				Enemy* enemy{};

				switch (m_type)
				{
				case BossMask::MaskType::KATANA:
					enemy = new EnemyKatana(GetPosition(), {}, {});
					break;
				case BossMask::MaskType::KANABO:
					enemy = new EnemyKanabo(GetPosition(), {}, {});
					break;
				case BossMask::MaskType::BOW:
					enemy = new EnemyBow(GetPosition(), {}, {});
					break;
				case BossMask::MaskType::SPEAR:
					enemy = new EnemySpear(GetPosition(), {}, {});
					break;
				default:
					break;
				}

				player->OnEnemyKilled(enemy);

				delete enemy;
			}
			OnHitByWeapon();
		}
	}
	// 弓の弾チェック
	if (otherObject->GetTag() == ObjectTag::BULLET) {
		auto bullet = dynamic_cast<const BulletBow*>(otherObject);
		if (bullet) {
			auto weapon = dynamic_cast<const Weapon*>(bullet->GetOwner());
			if (weapon)
			{
				Player* player = dynamic_cast<Player*>(weapon->GetOwner());

				if (player)
				{
					Enemy* enemy{};

					switch (m_type)
					{
					case BossMask::MaskType::KATANA:
						enemy = new EnemyKatana(GetPosition(), {}, {});
						break;
					case BossMask::MaskType::KANABO:
						enemy = new EnemyKanabo(GetPosition(), {}, {});
						break;
					case BossMask::MaskType::BOW:
						enemy = new EnemyBow(GetPosition(), {}, {});
						break;
					case BossMask::MaskType::SPEAR:
						enemy = new EnemySpear(GetPosition(), {}, {});
						break;
					default:
						break;
					}

					player->OnEnemyKilled(enemy);

					delete enemy;
				}
			}

			OnHitByWeapon();
		}
	}
}


//==============================================================================
// GuillotineGimmick - ギロチンギミック
//==============================================================================
void GuillotineGimmick::CreateParts(const XMFLOAT2& pos, const XMFLOAT2& size) {
	m_top = { PartType::Top, pos, size };
	m_middle = { PartType::Middle, pos, size };
	m_blade = { PartType::Blade, pos, size };
}

void GuillotineGimmick::Initialize() {
	m_top.texId = TextureManager::Instance().Get(TexID::GIMMICK_GUILLOTINE_TOP);
	m_middle.texId = TextureManager::Instance().Get(TexID::GIMMICK_GUILLOTINE_MIDDLE);
	m_blade.texId = TextureManager::Instance().Get(TexID::GIMMICK_GUILLOTINE_BLADE);

	// コライダー再設定
	GetColliders().clear();
	m_bodyCollider = nullptr;

	GetColliders().reserve(3);
	// パーツごとに独立コライダー
	AddCollider(m_top.pos, { TOPCOLSIZE_X, TOPCOLSIZE_Y }, ColliderType::BODY);    // 0番...TOP
	midColPos = {
	m_middle.pos.x + MIDCOLPOS_X,
	m_middle.pos.y + MIDCOLPOS_Y
	};
	midColSize = { MIDCOLSIZE_X, MIDCOLPOS_Y };
	AddCollider(midColPos, midColSize, ColliderType::BODY);
	bottomColPos = {
	m_blade.pos.x + BOTTOMCOLPOS_X,
	m_blade.pos.y + BOTTOMCOLPOS_Y
	};
	bottomColSize = { BOTTOMCOLSIZE_X, BOTTOMCOLSIZE_Y };
	AddCollider(bottomColPos, bottomColSize, ColliderType::GUILLOTINE_BLADE_TOP);   // 2番...BLADE

	// 刃下部（刃）用コライダー
	bladeBottomColPos = {
		m_blade.pos.x + BLADECOLPOS_X,
		m_blade.pos.y + BLADECOLPOS_Y
	};
	bladeBottomColSize = { BLADECOLSIZE_X, BLADECOLSIZE_Y };
	AddCollider(bladeBottomColPos, bladeBottomColSize, ColliderType::GUILLOTINE_BLADE_BOTTOM);


	CollisionManager::GetInstance().AddObject(this);

	m_top.isActive = true;
	m_middle.isActive = true;
	m_middle.isCut = false;
	m_blade.isActive = true;
	m_blade.isFalling = false;
	m_blade.fallVelY = 0.0f;
}

void GuillotineGimmick::Finalize() {
	CollisionManager::GetInstance().RemoveObject(this);
}

void GuillotineGimmick::Update() {
	// 落下中の刃
	if (m_blade.isActive && m_blade.isFalling) {
		constexpr float GRAVITY = 300.0f;
		float dt = (float)TimeManager::GetInstance().GetScaledElapsedTime();
		m_blade.fallVelY += GRAVITY * dt;
		m_blade.pos.y += m_blade.fallVelY * dt;

		// ロープ切断後はインデックスが1つずれる
		if (m_middle.isCut) {
			if (GetColliders().size() >= 3) {
				bottomColPos = {
					m_blade.pos.x + BOTTOMCOLPOS_X,
					m_blade.pos.y + BOTTOMCOLPOS_Y
				};
				bladeBottomColPos = {
					m_blade.pos.x + BLADECOLPOS_X,
					m_blade.pos.y + BLADECOLPOS_Y
				};
				GetColliders()[1].SetColliderPos(bottomColPos);
				GetColliders()[2].SetColliderPos(bladeBottomColPos);
			}
		} else {
			if (GetColliders().size() >= 4) {
				bottomColPos = {
					m_blade.pos.x + BOTTOMCOLPOS_X,
					m_blade.pos.y + BOTTOMCOLPOS_Y
				};
				bladeBottomColPos = {
					m_blade.pos.x + BLADECOLPOS_X,
					m_blade.pos.y + BLADECOLPOS_Y
				};
				GetColliders()[2].SetColliderPos(bottomColPos);
				GetColliders()[3].SetColliderPos(bladeBottomColPos);
			}
		}
	}
}


void GuillotineGimmick::Draw() {
	if (m_middle.isActive && !m_middle.isCut && m_middle.texId >= 0) {
		for (int i = 0; i < 10; ++i) {
			Sprite_ScrollDraw(m_middle.texId, m_middle.pos.x - (m_middle.size.x / 1.925), m_middle.pos.y - (m_middle.size.y / 4), m_middle.size.x * 2, m_middle.size.y * 2);
			Direct3D_SetAlphaBlend(BLEND_TRANSPARENT);
		}
	}

	if (m_top.isActive && m_top.texId >= 0) {
		for (int i = 0; i < 1; ++i) {
			Sprite_ScrollDraw(m_top.texId, m_top.pos.x, m_top.pos.y, m_top.size.x, m_top.size.y);
			Direct3D_SetAlphaBlend(BLEND_TRANSPARENT);
		}
	}

	if (m_blade.isActive && m_blade.texId >= 0) {
		for (int i = 0; i < 1; ++i) {
			Sprite_ScrollDraw(m_blade.texId, m_blade.pos.x, m_blade.pos.y, m_blade.size.x, m_blade.size.y);
			Direct3D_SetAlphaBlend(BLEND_TRANSPARENT);
		}
	}
}

void GuillotineGimmick::StartBladeFall() {
	m_blade.isFalling = true;
	m_blade.fallVelY = 0.0f;
}

void GuillotineGimmick::OnCollision(
	const GameObject* otherObject, const Collider* myCollider, const Collider* otherCollider, COLLISION_VEC hitVec) {
	if (!otherObject || !otherCollider) return;

	// ギロチン刃底でBLOCKとぶつかった場合に止める
	if (myCollider->GetType() == ColliderType::GUILLOTINE_BLADE_BOTTOM) {
		if (otherObject->GetTag() == ObjectTag::BLOCK_NORMAL ||
		otherObject->GetTag() == ObjectTag::BLOCK_GRASS ||
		otherObject->GetTag() == ObjectTag::BLOCK_STONE ||
		otherObject->GetTag() == ObjectTag::BLOCK_WOOD ||
		otherObject->GetTag() == ObjectTag::BLOCK_AIR) {
			m_blade.pos.y = otherObject->GetPosition().y - (BLADECOLPOS_Y + BLADECOLSIZE_Y) + ((BOTTOMCOLSIZE_Y + BLADECOLSIZE_Y) - 64.0f);
			m_blade.isFalling = false;
			m_blade.fallVelY = 0.0f;

			if (m_middle.isCut) {
				if (GetColliders().size() >= 3) {
					bottomColPos = { m_blade.pos.x + BOTTOMCOLPOS_X, m_blade.pos.y + BOTTOMCOLPOS_Y };
					bladeBottomColPos = { m_blade.pos.x + BLADECOLPOS_X, m_blade.pos.y + BLADECOLPOS_Y };
					GetColliders()[1].SetColliderPos(bottomColPos);
					GetColliders()[2].SetColliderPos(bladeBottomColPos);
				}
			} else {
				if (GetColliders().size() >= 4) {
					bottomColPos = { m_blade.pos.x + BOTTOMCOLPOS_X, m_blade.pos.y + BOTTOMCOLPOS_Y };
					bladeBottomColPos = { m_blade.pos.x + BLADECOLPOS_X, m_blade.pos.y + BLADECOLPOS_Y };
					GetColliders()[2].SetColliderPos(bottomColPos);
					GetColliders()[3].SetColliderPos(bladeBottomColPos);
				}
			}
			return;
		}
	}

	int idx = -1;
	for (size_t i = 0; i < GetColliders().size(); ++i) {
		if (&GetColliders()[i] == myCollider)
			idx = (int)i;
	}
	if (idx == 1) {
		if (otherCollider->GetType() == ColliderType::ATTACK) {
			auto weapon = dynamic_cast<const Weapon*>(otherObject);
			if (weapon && (weapon->GetWeaponType() == Weapon::Type::KATANA || weapon->GetWeaponType() == Weapon::Type::BOW || weapon->GetWeaponType() == Weapon::Type::SPEAR)) {
				CutRope();
				return;
			}
		}
	}

	// 弓の弾チェック
	if (otherObject->GetTag() == ObjectTag::BULLET) {
		auto bullet = dynamic_cast<const BulletBow*>(otherObject);
		if (bullet) {
			CutRope();
			return;
		}
	}

	if (myCollider->GetType() == ColliderType::GUILLOTINE_BLADE_BOTTOM) {
		// 敵にヒット
		if (m_blade.isFalling) {
			if (otherObject->GetTag() == ObjectTag::ENEMY_KATANA) {
				EnemyKatana* enemy = dynamic_cast<EnemyKatana*>(const_cast<GameObject*>(otherObject));
				if (enemy) {
					enemy->kill(); // 即死
				}
			}
			else if (otherObject->GetTag() == ObjectTag::ENEMY_KANABO) {
				EnemyKanabo* enemy = dynamic_cast<EnemyKanabo*>(const_cast<GameObject*>(otherObject));
				if (enemy) {
					enemy->kill(); // 即死
				}
			}
			else if (otherObject->GetTag() == ObjectTag::ENEMY_BOW) {
				EnemyBow* enemy = dynamic_cast<EnemyBow*>(const_cast<GameObject*>(otherObject));
				if (enemy) {
					enemy->kill(); // 即死
				}
				else if (otherObject->GetTag() == ObjectTag::ENEMY_YARI) {
					EnemySpear* enemy = dynamic_cast<EnemySpear*>(const_cast<GameObject*>(otherObject));
					if (enemy) {
						enemy->kill(); // 即死
					}
				}
			}

			// プレイヤーにヒット
			if (m_blade.isFalling && otherObject->GetTag() == ObjectTag::PLAYER) {
				Player* player = dynamic_cast<Player*>(const_cast<GameObject*>(otherObject));
				if (player) {
					player->GuillotineHit();
				}
			}
			return;
		}
	}
}

void GuillotineGimmick::CutRope() {
	if (m_middle.isCut) return;

	m_middle.isCut = true;
	m_middle.isActive = false;

	// CollisionManagerから完全に除外
	CollisionManager::GetInstance().RemoveObject(this);

	// ロープのコライダーを削除
	if (GetColliders().size() > 1) {
		GetColliders().erase(GetColliders().begin() + 1);
	}

	// 再登録
	CollisionManager::GetInstance().AddObject(this);

	// オブジェクトを削除状態にする
	SetDelete(true);

	Audio_PlaySE(AudioID::SE_GUILLOTINE);
	StartBladeFall();
}

//==============================================================================
// NextStageGimmick - 次ステージトリガー
//==============================================================================
void NextStageGimmick::Initialize() {
	m_playerEntered = false;
	m_fadeTexId = TextureManager::Instance().Get(TexID::FADE);
	m_barrierTextureId = TextureManager::Instance().Get(TexID::BOSS_BARRIER);

	// バリア破壊アニメーション作成
	if (!m_barrierAnim) {
		m_barrierAnim = new AnimPattern(
			m_barrierTextureId,
			BARRIER_ANIM_TOTAL,         // 全パターン数 (30)
			BARRIER_ANIM_COLS,          // 横のパターン数 (5)
			0.05f,                      // アニメーション速度
			{ 0, 0 },                   // 開始位置
			{ static_cast<uint32_t>(BARRIER_FRAME_WIDTH), static_cast<uint32_t>(BARRIER_FRAME_HEIGHT) },
			false,
			false                       // ループしない
		);
	}
	if (!m_barrierAnimPlayer) {
		m_barrierAnimPlayer = new AnimPatternPlayer(m_barrierAnim);
	}
	m_isBarrierAnimPlaying = false;

	GetColliders().clear();
	AddCollider({ GetPosition().x + 64.0f, GetPosition().y }, { BLOCK_WIDTH - 64.0f, BLOCK_HEIGHT }, ColliderType::BODY);
	CollisionManager::GetInstance().AddObject(this);
}

void NextStageGimmick::Draw() {
	if (!IsActive()) return;
	if (m_fadeTexId < 0) return;

	Sprite_ScrollDraw(
		m_fadeTexId,
		GetPosition().x, GetPosition().y,
		BLOCK_WIDTH, BLOCK_HEIGHT
	);

	// バリア描画（アニメーション対応）
	if (m_barrierTextureId >= 0) {
		float barrierW = BLOCK_WIDTH + 32.0f;
		float barrierH = BLOCK_HEIGHT + 32.0f;
		float barrierX = GetPosition().x - 16.0f;
		float barrierY = GetPosition().y - 16.0f;

		if (m_isBarrierAnimPlaying && m_barrierAnimPlayer) {
			// 破壊アニメーション再生中
			int pattern = m_barrierAnimPlayer->GetPattern();
			int col = pattern % BARRIER_ANIM_COLS;
			int row = pattern / BARRIER_ANIM_COLS;

			float uvX = col * BARRIER_FRAME_WIDTH;
			float uvY = row * BARRIER_FRAME_HEIGHT;

			Sprite_ScrollDraw(m_barrierTextureId,
				barrierX, barrierY,
				barrierW, barrierH,
				uvX, uvY,
				BARRIER_FRAME_WIDTH, BARRIER_FRAME_HEIGHT,
				0.0f,
				{ 1.0f, 1.0f, 1.0f, 1.0f });
		} else if (m_isLocked) {
			// ロック中：静止バリア表示（最初のフレーム）
			Sprite_ScrollDraw(m_barrierTextureId,
				barrierX, barrierY,
				barrierW, barrierH,
				0.0f, 0.0f,
				BARRIER_FRAME_WIDTH, BARRIER_FRAME_HEIGHT,
				0.0f,
				{ 1.0f, 1.0f, 1.0f, 0.7f });
		}
	}
}

void NextStageGimmick::OnCollision(const GameObject* otherObject, const Collider* myCollider,
	const Collider* otherCollider, COLLISION_VEC hitVec) {
	if (!otherObject) return;

	// ロック中は通過不可
	if (m_isLocked) return;

	if (otherObject->GetTag() == ObjectTag::PLAYER) {
		m_playerEntered = true;
	}
}

void NextStageGimmick::Finalize() {
	CollisionManager::GetInstance().RemoveObject(this);
	GetColliders().clear();

	// バリアアニメーション解放
	if (m_barrierAnimPlayer) {
		delete m_barrierAnimPlayer;
		m_barrierAnimPlayer = nullptr;
	}
	if (m_barrierAnim) {
		delete m_barrierAnim;
		m_barrierAnim = nullptr;
	}
}

void NextStageGimmick::Update() {
	// バリア破壊アニメーション更新
	if (m_isBarrierAnimPlaying && m_barrierAnimPlayer) {
		m_barrierAnimPlayer->Update();
		if (m_barrierAnimPlayer->IsEnd()) {
			m_isBarrierAnimPlaying = false;
		}
	}
}

void NextStageGimmick::SetLocked(bool locked) {
	// ロック解除時にバリア破壊アニメーション再生
	if (m_isLocked && !locked) {
		// ロック中 → 解除 への遷移
		if (m_barrierAnimPlayer) {
			m_barrierAnimPlayer->Reset();
			m_isBarrierAnimPlaying = true;
		}
		Audio_PlaySE(AudioID::SE_BARRIER_BREAK);
	}
	m_isLocked = locked;
}


//==============================================================================
// NextStageEntrance - 次ステージ入口（描画のみ）
//==============================================================================
void NextStageEntrance::Initialize() {
	m_texId = TextureManager::Instance().Get(TexID::FADE);
}

void NextStageEntrance::Finalize() {
}

void NextStageEntrance::Update() {
}

void NextStageEntrance::Draw() {
	if (!IsActive()) return;
	if (m_texId < 0) return;

	// 左右反転して描画
	Sprite_ScrollDraw(
		m_texId,
		GetPosition().x - 64.0f * 3.0f, GetPosition().y,
		BLOCK_WIDTH, BLOCK_HEIGHT,
		0.0f, 0.0f,
		1032.0f, 1032.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		SPRITE_DIRECTION::FLIP_X
	);
}

//==============================================================================
// MidBossWall - ボス部屋前の壁ギミック
//==============================================================================
void MidBossWall::Initialize() {
	m_texId = TextureManager::Instance().Get(TexID::BLOCK_BARRIER);  // 通常ブロックと同じテクスチャ

	AddCollider(GetPosition(), GetSize(), ColliderType::BODY);
	CollisionManager::GetInstance().AddObject(this);
}

void MidBossWall::Finalize() {
	if (m_gridBlock) m_gridBlock->SetActive(false);
	CollisionManager::GetInstance().RemoveObject(this);
	GetColliders().clear();
}

void MidBossWall::Update() {
	if (!IsActive()) return;

	if (m_isFadeOut)
	{
		m_alpha += TimeManager::GetInstance().GetElapsedTime();
		if (m_alpha >= 1.0f)
		{
			m_alpha = 1.0f;
		}
	}
	else
	{
		m_alpha -= TimeManager::GetInstance().GetElapsedTime();
		if (m_alpha <= 0.0f)
		{
			m_alpha = 0.0f;
			SetActive(false);
		}
	}

	// 1. 経過時間を加算
	m_totalTime += TimeManager::GetInstance().GetElapsedTime();

	// 2. 変化させたい2つの色を定義 (R, G, B, A)
	// 通常時の薄い赤
	XMVECTOR colorNormal = XMVectorSet(1.0f, 0.0f, 0.0f, m_alpha);
	// 濃く光る赤
	XMVECTOR colorBright = XMVectorSet(1.0f, 0.2f, 0.2f, m_alpha);

	// 3. 点滅のスピード
	float speed = 3.0f;

	// 4. サイン波を使って 0.0 ～ 1.0 の割合(t)を計算
	// std::sin は <cmath> に含まれます
	float t = (std::sin(m_totalTime * speed) + 1.0f) / 2.0f;

	// 5. 2つの色を t の割合で線形補間 (Lerp)
	XMVECTOR currentColor = XMVectorLerp(colorNormal, colorBright, t);

	// 6. 描画用に XMFLOAT4 (または XMFLOAT3) に変換
	XMFLOAT4 renderColor;
	XMStoreFloat4(&renderColor, currentColor);

	SetColor(renderColor);
}

void MidBossWall::Draw() {
	if (!IsActive()) return;
	if (m_texId < 0) return;


	Sprite_ScrollDraw(m_texId, GetPosition().x, GetPosition().y, GetSize().x, GetSize().y, GetColor());
}

void MidBossWall::Appear() {
	SetActive(true);
	// コライダーも有効化
	for (auto& col : GetColliders()) {
		col.SetActive(true);
	}

	m_isFadeOut = true;

	if (m_gridBlock) m_gridBlock->SetActive(true);
}

void MidBossWall::Destroy() {
	// コライダーも無効化
	for (auto& col : GetColliders()) {
		col.SetActive(false);
	}

	m_isFadeOut = false;

	if (m_gridBlock) m_gridBlock->SetActive(false);
}

//==============================================================================
// CorridorMaskBarrage - 廊下弾幕ギミック
//==============================================================================
void CorridorMaskBarrage::Initialize() {
	// コリジョン不要
}

void CorridorMaskBarrage::Finalize() {
	for (auto* proj : m_projectiles) {
		if (proj) {
			CollisionManager::GetInstance().RemoveObject(proj);
			proj->Finalize();
			delete proj;
		}
	}
	m_projectiles.clear();
}

void CorridorMaskBarrage::Update() {
	float dt = static_cast<float>(TimeManager::GetInstance().GetScaledElapsedTime());

	// プレイヤーがcmbより左にいれば弾幕有効
	if (m_player) {
		float playerX = m_player->GetPosition().x;
		m_isBarrageActive = (playerX < m_triggerRightX);
	} else {
		m_isBarrageActive = false;
	}

	// 弾幕中なら定期的に1発ずつスポーン
	if (m_isBarrageActive) {
		m_spawnTimer += dt;
		if (m_spawnTimer >= m_spawnInterval) {
			m_spawnTimer -= m_spawnInterval;
			SpawnMaskBullet();
		}
	} else {
		m_spawnTimer = 0.0f;
	}

	// 全弾更新
	for (auto* proj : m_projectiles) {
		if (proj && proj->IsActive()) {
			proj->Update();
		}
	}

	CleanupProjectiles();
}

void CorridorMaskBarrage::Draw() {
	for (auto* proj : m_projectiles) {
		if (proj && proj->IsActive()) {
			proj->Draw();
		}
	}
}

void CorridorMaskBarrage::SpawnMaskBullet() {
	if (!m_player) return;

	float playerY = m_player->GetPosition().y + m_player->GetSize().y * 0.5f;
	float yOffset = static_cast<float>((rand() % static_cast<int>(Y_RANDOM_RANGE * 2)) - static_cast<int>(Y_RANDOM_RANGE));
	float spawnY = playerY + yOffset - m_bulletSize.y * 0.5f;

	if (spawnY < 64.0f) spawnY = 64.0f;
	if (spawnY > m_stageHeight - 128.0f) spawnY = m_stageHeight - 128.0f;

	DirectX::XMFLOAT2 spawnPos = { m_spawnX, spawnY };

	auto* bullet = new MaskBulletProjectile(
		spawnPos, m_bulletSize, nullptr, m_player, false, m_stageWidth);

	// テクスチャを明示的に設定
	int texId = TextureManager::Instance().Get(TexID::BOSS_MASK_BULLET);
	bullet->SetTextureId(texId);

	bullet->SetChargeDuration(0.0f);
	bullet->SetBulletSpeed(m_bulletSpeed);
	bullet->SetStageBounds(0.0f, m_stageWidth, m_stageHeight);

	float angleOffset = static_cast<float>((rand() % 200) - 100) / 100.0f * MAX_ANGLE_OFFSET;
	float angle = 3.14159f + angleOffset;
	bullet->SetMoveAngle(angle);

	// Fire()だけ呼ぶ（中でコライダー追加＋CollisionManager登録される）
	bullet->Fire();

	m_projectiles.push_back(bullet);
}

void CorridorMaskBarrage::CleanupProjectiles() {
	for (auto it = m_projectiles.begin(); it != m_projectiles.end(); ) {
		if (!(*it) || !(*it)->IsActive()) {
			if (*it) {
				(*it)->Finalize();
				delete* it;
			}
			it = m_projectiles.erase(it);
		} else {
			++it;
		}
	}
}
