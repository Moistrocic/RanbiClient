// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Rcme, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Rcme, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Rcme, ScriptName, Len, Def, Save, Desc) ;
#endif

MACRO_CONFIG_INT(RcHammerFix, rc_hammer_fix, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(RcCursorCopy, rc_cursor_copy, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(RcEmotionDeferredRender, rc_emotion_deferred_render, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcHookCollDeferredRender, rc_hook_coll_deferred_render, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(RcTalkingHidden, rc_talking_hidden, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(RcScoreboardShowLove, rc_scoreboard_show_love, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcScoreboardShowPoints, rc_scoreboard_show_points, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcScoreboardCopyName, rc_scoreboard_copy_name, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(RcNameplatesShowPoints, rc_nameplates_show_points, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesShowPositionX, rc_nameplates_show_position_x, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesShowPositionXMatched, rc_nameplates_show_position_x_matched, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_COL(RcNameplatesShowPositionXMatchedColor, rc_nameplates_show_position_x_matched_color, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesShowFinished, rc_nameplates_show_finished, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesShowFinishedSize, rc_nameplates_show_finished_size, 18, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesShowDummyCopyStatus, rc_nameplates_show_dummy_copy_status, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesShowDummyCopyStatusSize, rc_nameplates_show_dummy_copy_status_size, 18, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesShowHammerFlyStatus, rc_nameplates_show_hammer_fly_status, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesShowHammerFlyStatusSize, rc_nameplates_show_hammer_fly_status_size, 18, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesShowDummyResetStatus, rc_nameplates_show_dummy_reset_status, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesShowDummyResetStatusSize, rc_nameplates_show_dummy_reset_status_size, 18, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHidden, rc_nameplates_range_hidden, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenName, rc_nameplates_range_hidden_name, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenPoints, rc_nameplates_range_hidden_points, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenPositionX, rc_nameplates_range_hidden_position_x, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenFinished, rc_nameplates_range_hidden_finished, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenStatus, rc_nameplates_range_hidden_status, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenHookStatus, rc_nameplates_range_hidden_hook_status, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenLove, rc_nameplates_range_hidden_love, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenOthers, rc_nameplates_range_hidden_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenRadius, rc_nameplates_range_hidden_radius, 6, -10, 10, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenAngleStart, rc_nameplates_range_hidden_angle_start, 0, 0, 360, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcNameplatesRangeHiddenAngleEnd, rc_nameplates_range_hidden_angle_end, 360, 0, 360, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(RcChatSkipRepeatHistory, rc_chat_skip_repeat_history, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcChatShowPoints, rc_chat_show_points, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(RcShowWeaponsAngle, rc_show_weapons_angle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcShowEnableSkipThreeTiles, rc_show_enable_skip_three_tiles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcShowEnableSkipThreeTilesInfoSize, rc_show_enable_skip_three_tiles_info_size, 10, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_COL(RcShowEnableSkipThreeTilesInfoColor, rc_show_enable_skip_three_tiles_info_color, 8191872, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcShowEnableSkipThreeTilesInfoPositionX, rc_show_enable_skip_three_tiles_info_position_x, 20, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcShowEnableSkipThreeTilesInfoPositionY, rc_show_enable_skip_three_tiles_info_position_y, 94, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(RcShowEnableSkipThreeTilesInfoPositionLeftText, rc_show_enable_skip_three_tiles_info_position_left_text, 128, "←", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(RcShowEnableSkipThreeTilesInfoPositionRightText, rc_show_enable_skip_three_tiles_info_position_right_text, 128, "→", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(RcShowEnableSkipThreeTilesInfoPositionDJLeftText, rc_show_enable_skip_three_tiles_info_position_dj_left_text, 128, "↖", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(RcShowEnableSkipThreeTilesInfoPositionDJRightText, rc_show_enable_skip_three_tiles_info_position_dj_right_text, 128, "↗", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcAutoChangeSkin, rc_auto_change_skin, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcShowSpectatorSkin, rc_show_spectator_skin, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcShowAllPlayers, rc_show_all_players, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
