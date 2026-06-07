//==============================================================================
//
//  音声定義ファイル [audio_def.h]
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/10
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

//==============================================================================
// 音声定義リスト
//==============================================================================
// 
// 【使い方】
// 新しい音声を追加するときは、このファイルに1行追加するだけでOK
// enum, ファイルパス配列, BGMフラグ配列が自動生成される
// 
// 【書式】
// AUDIO(ID名, "ファイルパス", BGMフラグ)
//   - ID名      : AudioID::XXX として使う名前
//   - ファイルパス : resource/sound/ からの相対パス
//   - BGMフラグ   : true = BGM（ループ再生）, false = SE（単発再生）
// 
// 【注意】
// - 最後の行以外は末尾に \ が必要
// - 最後の行は \ を付けない
//
//==============================================================================

#define AUDIO_LIST \
    /*=== BGM ===*/ \
    AUDIO(BGM_TITLE,   "resource/sound/BGM/Title.mp3",    true) \
    AUDIO(BGM_GAME1,   "resource/sound/BGM/Stage_01.mp3", true) \
    AUDIO(BGM_GAME2,   "resource/sound/BGM/Stage_02.mp3", true) \
    AUDIO(BGM_GAME3,   "resource/sound/BGM/Stage_03.mp3", true) \
    AUDIO(BGM_GAME4,   "resource/sound/BGM/Stage_04.mp3", true) \
    AUDIO(BGM_GAME5,   "resource/sound/BGM/Stage_05.mp3", true) \
    AUDIO(BGM_BOSS1,   "resource/sound/BGM/Boss_01.mp3",  true) \
    AUDIO(BGM_BOSS2,   "resource/sound/BGM/Boss_02.mp3",  true) \
    AUDIO(BGM_RESULT,  "resource/sound/BGM/Result.mp3",   true) \
    \
    /*=== SE - プレイヤー ===*/ \
    AUDIO(SE_JUMP,     "resource/sound/SE/player/player_jump.mp3",    false) \
    AUDIO(SE_DAMAGE,   "resource/sound/SE/damage.mp3",                false) \
    AUDIO(SE_DEATH,    "resource/sound/SE/death.mp3",                 false) \
    AUDIO(SE_RESPAWN,  "resource/sound/SE/respawn.mp3",               false) \
    AUDIO(SE_HYOUI,    "resource/sound/SE/hyoui.mp3",                 false) \
    AUDIO(SE_TYAKUTI,  "resource/sound/SE/player/player_tyakuti.mp3", false) \
    AUDIO(SE_DASH,     "resource/sound/SE/player/player_dash.mp3",    false) \
    AUDIO(SE_FOOTSTEP, "resource/sound/SE/player/footstep.mp3",       false) \
    \
    /*=== SE - コンボ ===*/ \
    AUDIO(SE_COMBO_ADD, "resource/sound/SE/combo_add.mp3", false) \
    AUDIO(SE_COMBO_FIN, "resource/sound/SE/combo_fin.mp3", false) \
    \
    /*=== SE - 武器攻撃 ===*/ \
    AUDIO(SE_KATANA_ATTACK, "resource/sound/SE/katana_attack.mp3", false) \
    AUDIO(SE_KATANA_IAI,    "resource/sound/SE/katana_iai.mp3",    false) \
    AUDIO(SE_KANABO_KAMAE,  "resource/sound/SE/kanabo_kamae.mp3",  false) \
    AUDIO(SE_KANABO_ATTACK, "resource/sound/SE/kanabo_attack.mp3", false) \
    AUDIO(SE_SPEAR_KAMAE,   "resource/sound/SE/spear_kamae.mp3",   false) \
    AUDIO(SE_SPEAR_ATTACK,  "resource/sound/SE/spear_attack.mp3",  false) \
    AUDIO(SE_BOW_CHARGE,    "resource/sound/SE/bow_charge.mp3",    false) \
    AUDIO(SE_BOW_SHOOT,     "resource/sound/SE/bow_shoot.mp3",     false) \
    \
    /*=== SE - ギミック ===*/ \
    AUDIO(SE_GUILLOTINE,  "resource/sound/SE/guillotine.mp3",  false) \
    AUDIO(SE_STAKE,       "resource/sound/SE/stake.mp3",       false) \
    AUDIO(SE_ROPE_CUT,    "resource/sound/SE/rope_cut.mp3",    false) \
    AUDIO(SE_SPEED_UP,    "resource/sound/SE/speed_up.mp3",    false) \
    AUDIO(SE_JUMP_PAD,    "resource/sound/SE/jump_pad.mp3",    false) \
    AUDIO(SE_CANNON_IN,   "resource/sound/SE/cannon_in.mp3",   false) \
    AUDIO(SE_CANNON_OUT,  "resource/sound/SE/cannon_out.mp3",  false) \
    AUDIO(SE_CHECK_POINT, "resource/sound/SE/check_point.mp3", false) \
    AUDIO(SE_CONTINUE,    "resource/sound/SE/continue.mp3",    false) \
    \
    /*=== SE - UI/システム ===*/ \
    AUDIO(SE_SYSTEM_PRESS,  "resource/sound/SE/system_press.mp3",  false) \
    AUDIO(SE_SYSTEM_SELECT, "resource/sound/SE/system_select.mp3", false) \
    \
    /*=== SE - BOSS ===*/ \
    AUDIO(SE_BOSS_WIN,              "resource/sound/SE/boss/boss_win.mp3",                  false) \
    AUDIO(SE_BARRIER_HIT,           "resource/sound/SE/boss/barrier_hit.mp3",               false) \
    AUDIO(SE_BARRIER_BREAK,         "resource/sound/SE/boss/barrier_break.mp3",             false) \
    AUDIO(SE_BOSS_BEAM,             "resource/sound/SE/boss/boss_beam.mp3",                 false) \
    AUDIO(SE_BOSS_GUILLOTINE,       "resource/sound/SE/boss/boss_guillotine.mp3",           false) \
    AUDIO(SE_BOSS_GUILLOTINE_RAKKA, "resource/sound/SE/boss/guillotine_rakka.mp3",          false) \
    AUDIO(SE_BOSS_WALL_HIT,         "resource/sound/SE/boss/boss_wall_hit.mp3",             false) \
    AUDIO(SE_BOSS_SCREAM,           "resource/sound/SE/boss/boss_scream.mp3",               false) \
    AUDIO(SE_BOSS_DAMAGE,           "resource/sound/SE/boss/boss_damage.mp3",               false) \
    AUDIO(SE_MASK_BREAK,            "resource/sound/SE/boss/mask_break.mp3",                false) \
    AUDIO(SE_MASK_FIRE,             "resource/sound/SE/boss/mask_fire.mp3",                 false) \
    AUDIO(SE_MASK_BULLET,           "resource/sound/SE/boss/mask_bullet.mp3",               false) \
    AUDIO(SE_DOOR_CLOSE,            "resource/sound/SE/boss/door_close.mp3",                false) \
    AUDIO(SE_SPECIAL_ATTACK_READY,  "resource/sound/SE/boss/boss_special_attack_ready.mp3", false) \
    AUDIO(SE_BOSS_SCRATCH,          "resource/sound/SE/boss/boss_scratch.mp3",              false)