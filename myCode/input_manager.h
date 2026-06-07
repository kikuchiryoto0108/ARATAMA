//==============================================================================
//
//  インプットマネージャー [input_manager.h]
//  Author : Ryoto Kikuchi
//  Date   : 2026/1/5
//------------------------------------------------------------------------------
//
//==============================================================================
#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <DirectXMath.h>
#include "weapon.h"
#include "mouse.h"

class InputManager {
public:
	// 入力コマンド構造体
	struct Command {
		bool moveLeft;
		bool moveRight;
		bool up;
		bool down;
		bool jump;
		bool dash;
		bool attack;
		bool attack_gauge;
		bool weaponSwitch1;		// 刀
		bool weaponSwitch2;		// 弓
		bool weaponSwitch3;		// 金棒
		bool enter;				// ENTERキー（シーン遷移用）
		bool rightButton;       // 右ボタン（コントローラー用）

		// アナログ移動量（-1.0 ~ 1.0）
		float moveAnalogX;  // 左右の移動量
		float moveAnalogY;  // 上下の移動量

		DirectX::XMFLOAT2 attackDirection;	// 攻撃方向（マウス位置から計算）
		bool hasAttackDirection;

		DirectX::XMFLOAT2 mousePosition;	// マウスの画面座標
		DirectX::XMFLOAT2 mouseWorldPosition;	// マウスのワールド座標（カメラ考慮）

		// コントローラー
		DirectX::XMFLOAT2 leftStick;
		DirectX::XMFLOAT2 rightStick;
	};

public:
	static InputManager& Instance();

	// 初期化・終了処理
	void Initialize();
	void Finalize();

	// 毎フレーム更新
	void Update();

	// 入力コマンドの取得
	const Command& GetCommand() const { return command_; }

	// ショートカット：毎回書くのが面倒なので
	static const Command& Cmd() { return Instance().GetCommand(); }

	// デバッグ用：カメラ制御モードかどうか
	bool IsCameraDebugMode() const { return cameraDebugMode_; }

	// デバッグ用：カメラ移動入力
	DirectX::XMFLOAT2 GetCameraDebugMove() const;

	// マウス関連のヘルパー関数
	DirectX::XMFLOAT2 GetMouseScreenPosition() const { return command_.mousePosition; }
	DirectX::XMFLOAT2 GetMouseWorldPosition() const { return command_.mouseWorldPosition; }

	// カメラ設定（ワールド座標変換用）
	void SetCamera(class Camera* camera) { camera_ = camera; }

	// 振動開始（強度：0.0-1.0、時間：秒）
	static void StartVibration(float intensity, float duration);

	// 振動停止
	static void StopVibration();

	// 振動中かどうか
	static bool IsVibrating();

private:
	InputManager();
	~InputManager();

	// コピー禁止
	InputManager(const InputManager&) = delete;
	InputManager& operator=(const InputManager&) = delete;

	void UpdateCommand();
	void UpdateAttackDirection();
	void UpdateMousePosition();

private:
	Command command_;
	Command prevCommand_;	// 前フレームのコマンド（入力の変化検出用）

	bool cameraDebugMode_;
	DirectX::XMFLOAT2 mousePosition_;

	class Camera* camera_;	// ワールド座標変換用

	// 入力キャッシュ
	struct InputCache {
		// キーボード
		bool keyMoveLeft;
		bool keyMoveRight;
		bool keyMoveUp;
		bool keyMoveDown;
		bool keyJump;
		bool keyDash;
		bool keyEnter;
		bool keyWeapon1;
		bool keyWeapon2;
		bool keyWeapon3;
		bool keyUp;
		bool keyDown;
		bool keyLeft;
		bool keyRight;

		// マウス
		Mouse_State mouseState;
		bool mouseAttack;
		bool mouseAttackGauge;

		// ゲームパッド
		float stickX;
		float stickY;
		bool padMoveLeft;
		bool padMoveRight;
		bool padMoveUp;
		bool padMoveDown;
		bool padJump;
		bool padAttack;
		bool padAttackGauge;
		bool padDash;
		bool padEnter;
		bool padWeapon1;
		bool padWeapon2;
		bool padWeapon3;
		bool padRight;
		bool dpadUpKey;
		bool dpadDownKey;
		bool dpadLeftKey;
		bool dpadRightKey;
	};
	InputCache cache_{};

public:
	enum class InputType {
		MouseKeyboard,
		Gamepad
	};

	InputType GetInputType() const { return inputType_; }
private:
	InputType inputType_{ InputType::MouseKeyboard };
};

#endif // INPUT_MANAGER_H