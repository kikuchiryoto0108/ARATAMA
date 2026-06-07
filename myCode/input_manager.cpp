//==============================================================================
//
//  インプットマネージャー [input_manager.cpp]
//  Author : Ryoto Kikuchi
//  Date   : 2026/1/5
//------------------------------------------------------------------------------
//
//==============================================================================
#include "input_manager.h"
#include "key_logger.h"
#include "game_controller.h"
#include "game.h"
#include "camera.h"
#include <cstring>

InputManager& InputManager::Instance() {
	static InputManager instance;
	return instance;
}

InputManager::InputManager()
	: command_{}
	, prevCommand_{}
	, cameraDebugMode_(false)
	, mousePosition_{ 0.0f, 0.0f }
	, camera_(nullptr) {
}

InputManager::~InputManager() {
	Finalize();
}

void InputManager::Initialize() {
	std::memset(&command_, 0, sizeof(Command));
	std::memset(&prevCommand_, 0, sizeof(Command));
	cameraDebugMode_ = false;
	mousePosition_ = { 0.0f, 0.0f };
	camera_ = nullptr;
}

void InputManager::Finalize() {
	camera_ = nullptr;
}

void InputManager::Update() {
	// 前フレームのコマンドを保存
	prevCommand_ = command_;

	// コマンドをクリア
	std::memset(&command_, 0, sizeof(Command));

	// デバッグモードの切り替え（D1キーでカメラデバッグモード）
	cameraDebugMode_ = KeyLogger_IsPressed(KK_D1);

	// 入力の更新
	UpdateCommand();
	UpdateMousePosition();
	UpdateAttackDirection();
}

void InputManager::UpdateCommand() {
	// ========================================
	// キーボード入力
	// ========================================
	cache_.keyMoveLeft = KeyLogger_IsPressed(KK_A);
	cache_.keyMoveRight = KeyLogger_IsPressed(KK_D);
	cache_.keyMoveUp = KeyLogger_IsPressed(KK_W);
	cache_.keyMoveDown = KeyLogger_IsPressed(KK_S);
	cache_.keyJump = KeyLogger_IsTrigger(KK_SPACE);
	cache_.keyDash = KeyLogger_IsTrigger(KK_LEFTSHIFT);
	cache_.keyEnter = KeyLogger_IsTrigger(KK_ENTER);
	cache_.keyWeapon1 = KeyLogger_IsTrigger(KK_I);
	cache_.keyWeapon2 = KeyLogger_IsTrigger(KK_O);
	cache_.keyWeapon3 = KeyLogger_IsTrigger(KK_P);
	cache_.keyUp = KeyLogger_IsPressed(KK_UP);
	cache_.keyDown = KeyLogger_IsPressed(KK_DOWN);
	cache_.keyLeft = KeyLogger_IsPressed(KK_LEFT);
	cache_.keyRight = KeyLogger_IsPressed(KK_RIGHT);

	// ========================================
	// マウス入力
	// ========================================
	Mouse_GetState(&cache_.mouseState);

	// トリガー判定：前フレームで押されてなくて、今フレームで押されている
	cache_.mouseAttack = cache_.mouseState.leftButton && !prevCommand_.attack;
	cache_.mouseAttackGauge = cache_.mouseState.rightButton && !prevCommand_.attack_gauge;

	// マウス位置保存
	mousePosition_.x = static_cast<float>(cache_.mouseState.x);
	mousePosition_.y = static_cast<float>(cache_.mouseState.y);

	// ========================================
	// ゲームパッド入力
	// ========================================
	cache_.stickX = GameController::GetLeftStickX();
	cache_.stickY = GameController::GetLeftStickY();

	// デッドゾーン定数
	const float STICK_DEADZONE = 0.05f;      // スティック検出用
	const float MOVE_THRESHOLD = 0.1f;       // 移動判定用（ON/OFF）

	// 移動：Press判定（連続入力）
	cache_.padMoveLeft = GameController::IsPressed_DpadLeft() || cache_.stickX < -MOVE_THRESHOLD;
	cache_.padMoveRight = GameController::IsPressed_DpadRight() || cache_.stickX > MOVE_THRESHOLD;

	cache_.padMoveUp = GameController::IsPressed_DpadUp() ||
		(cache_.stickY < -MOVE_THRESHOLD && fabsf(cache_.stickY) > fabsf(cache_.stickX));  // 上方向優先
	cache_.padMoveDown = GameController::IsPressed_DpadDown() ||
		(cache_.stickY > MOVE_THRESHOLD && fabsf(cache_.stickY) > fabsf(cache_.stickX));  // 下方向優先

	// アクション：Trigger判定（押した瞬間だけ）
	cache_.padJump = GameController::IsTrigger_ButtonDown();    // 下ボタン（ジャンプ）
	cache_.padAttack = GameController::IsTrigger_ButtonLeft();  // 左ボタン（攻撃）
	cache_.padAttackGauge = GameController::IsTrigger_R2();  // 右ボタン（ゲージ消費攻撃）
	cache_.padDash = GameController::IsTrigger_L2();  // ダッシュ

	// システム：Trigger判定
	cache_.padEnter = GameController::IsTrigger_ButtonRight();

	// 武器切り替え：Trigger判定
	cache_.padWeapon1 = GameController::IsTrigger_L1();
	cache_.padWeapon2 = GameController::IsTrigger_R1();
	cache_.padWeapon3 = GameController::IsTrigger_L2();

	// スティック生値を保存
	command_.leftStick = { cache_.stickX, cache_.stickY };
	command_.rightStick = { GameController::GetRightStickX(), GameController::GetRightStickY() };

	// Aボタン
	cache_.padRight = GameController::IsTrigger_ButtonRight();    // Aジャンプ

	cache_.dpadUpKey = GameController::IsPressed_DpadUp();
	cache_.dpadDownKey = GameController::IsPressed_DpadDown();
	cache_.dpadLeftKey = GameController::IsPressed_DpadLeft();
	cache_.dpadRightKey = GameController::IsPressed_DpadRight();

	//========================
	// 入力デバイス判定
	//========================
	bool hasGamepadInput =
		fabs(cache_.stickX) > STICK_DEADZONE ||
		fabs(cache_.stickY) > STICK_DEADZONE ||
		cache_.dpadUpKey || cache_.dpadDownKey ||
		cache_.dpadLeftKey || cache_.dpadRightKey ||
		cache_.padJump || cache_.padAttack || cache_.padAttackGauge ||
		cache_.padDash || cache_.padEnter;

	bool hasKeyboardInput =
		cache_.keyMoveLeft || cache_.keyMoveRight ||
		cache_.keyMoveUp || cache_.keyMoveDown ||
		cache_.keyJump || cache_.keyDash || cache_.keyEnter;

	bool hasMouseInput =
		cache_.mouseState.leftButton || cache_.mouseState.rightButton ||
		(cache_.mouseState.x != static_cast<int>(prevCommand_.mousePosition.x)) ||
		(cache_.mouseState.y != static_cast<int>(prevCommand_.mousePosition.y));

	if (hasGamepadInput) {
		inputType_ = InputType::Gamepad;
	} else if (hasKeyboardInput || hasMouseInput) {
		inputType_ = InputType::MouseKeyboard;
	}

	// ========================================
	// アナログ移動量の計算
	// ========================================
	command_.moveAnalogX = 0.0f;
	command_.moveAnalogY = 0.0f;

	if (inputType_ == InputType::Gamepad) {
		// ゲームパッド：スティックの傾きをそのまま使用

		// 左スティックX軸
		if (fabs(cache_.stickX) > STICK_DEADZONE) {
			float sign = (cache_.stickX > 0.0f) ? 1.0f : -1.0f;
			float adjusted = (fabs(cache_.stickX) - STICK_DEADZONE) / (1.0f - STICK_DEADZONE);
			command_.moveAnalogX = sign * adjusted;
		}

		// 左スティックY軸
		if (fabs(cache_.stickY) > STICK_DEADZONE) {
			float sign = (cache_.stickY > 0.0f) ? 1.0f : -1.0f;
			float adjusted = (fabs(cache_.stickY) - STICK_DEADZONE) / (1.0f - STICK_DEADZONE);
			command_.moveAnalogY = sign * adjusted;
		}

		// D-Padは最大値で上書き
		if (cache_.dpadLeftKey) command_.moveAnalogX = -1.0f;
		if (cache_.dpadRightKey) command_.moveAnalogX = 1.0f;
		if (cache_.dpadUpKey) command_.moveAnalogY = -1.0f;
		if (cache_.dpadDownKey) command_.moveAnalogY = 1.0f;
	} else {
		// キーボード：常に最大値
		if (cache_.keyMoveLeft) {
			command_.moveAnalogX = -1.0f;
		} else if (cache_.keyMoveRight) {
			command_.moveAnalogX = 1.0f;
		}

		if (cache_.keyMoveUp) {
			command_.moveAnalogY = -1.0f;
		} else if (cache_.keyMoveDown) {
			command_.moveAnalogY = 1.0f;
		}
	}

	// ========================================
	// 統合（OR結合）
	// ========================================
	command_.moveLeft = cache_.keyMoveLeft || cache_.padMoveLeft;
	command_.moveRight = cache_.keyMoveRight || cache_.padMoveRight;
	command_.jump = cache_.keyJump || cache_.padJump;
	command_.dash = cache_.keyDash || cache_.padDash;
	command_.attack = cache_.mouseAttack || cache_.padAttack;
	command_.attack_gauge = cache_.mouseAttackGauge || cache_.padAttackGauge;
	command_.enter = cache_.keyEnter || cache_.padEnter;

	command_.up = cache_.keyMoveUp || cache_.dpadUpKey || cache_.padMoveUp;
	command_.down = cache_.keyMoveDown || cache_.dpadDownKey || cache_.padMoveDown;

	command_.rightButton = cache_.padRight;

	command_.weaponSwitch1 = cache_.keyWeapon1 || cache_.padWeapon1;
	command_.weaponSwitch2 = cache_.keyWeapon2 || cache_.padWeapon2;
	command_.weaponSwitch3 = cache_.keyWeapon3 || cache_.padWeapon3;
}


void InputManager::UpdateMousePosition() {
	// スクリーン座標
	command_.mousePosition = mousePosition_;

	// ワールド座標（カメラがあれば変換）
	if (camera_) {
		// カメラのオフセットを考慮してワールド座標に変換
		command_.mouseWorldPosition.x = mousePosition_.x + camera_->GetPosition().x - SCREEN_WIDTH / 2.0f;
		command_.mouseWorldPosition.y = mousePosition_.y + camera_->GetPosition().y - SCREEN_HEIGHT / 2.0f;
	} else {
		// カメラがない場合はスクリーン座標と同じ
		command_.mouseWorldPosition = mousePosition_;
	}
}

void InputManager::UpdateAttackDirection() {

	command_.hasAttackDirection = true;

	//========================
	// マウス＆キーボード
	//========================
	if (inputType_ == InputType::MouseKeyboard) {
		if (mousePosition_.x < SCREEN_WIDTH * 0.5f) {
			command_.attackDirection = { -1.0f, 0.0f };
		} else {
			command_.attackDirection = { 1.0f, 0.0f };
		}
		return;
	}

	//========================
	// ゲームパッド
	//========================
	const float STICK_DEADZONE = 0.2f;

	// 左スティック優先
	if (fabs(command_.leftStick.x) > STICK_DEADZONE) {
		command_.attackDirection =
			(command_.leftStick.x >= 0.0f)
			? DirectX::XMFLOAT2{ 1.0f, 0.0f }
		: DirectX::XMFLOAT2{ -1.0f, 0.0f };
		return;
	}

	// スティック入力が無ければ最後の向きを維持
	// （Player 側で m_isRight を更新しているので問題なし）
}

DirectX::XMFLOAT2 InputManager::GetCameraDebugMove() const {
	if (!cameraDebugMode_) {
		return { 0.0f, 0.0f };
	}

	DirectX::XMFLOAT2 move{ 0.0f, 0.0f };

	if (KeyLogger_IsPressed(KK_UP)) {
		move.y -= 50.0f;
	}
	if (KeyLogger_IsPressed(KK_DOWN)) {
		move.y += 50.0f;
	}
	if (KeyLogger_IsPressed(KK_LEFT)) {
		move.x -= 50.0f;
	}
	if (KeyLogger_IsPressed(KK_RIGHT)) {
		move.x += 50.0f;
	}

	return move;
}

// 振動関連
void InputManager::StartVibration(float intensity, float duration) {
	GameController::StartVibration(intensity, duration);
}

void InputManager::StopVibration() {
	GameController::StopVibration();
}

bool InputManager::IsVibrating() {
	return GameController::IsVibrating();
}