#pragma once

#include <array>
#include <string_view>

// Every key of the localization tables that the menu uses (dist/SKSE/Plugins/K7RSCarryWeight/lang/*.toml). A test
// checks that english.toml and russian.toml have exactly these keys (AGENTS.md, "Локализация").
namespace k7::rscw::ui::keys
{
    inline constexpr std::string_view MENU_SECTION = "K7RS_MENU_SECTION";
    inline constexpr std::string_view MENU_SETTINGS = "K7RS_MENU_SETTINGS";
    inline constexpr std::string_view PLAYER_HEADER = "K7RS_PLAYER_HEADER";
    inline constexpr std::string_view PLAYER_NONE = "K7RS_PLAYER_NONE";
    inline constexpr std::string_view PLAYER_RACE = "K7RS_PLAYER_RACE";
    inline constexpr std::string_view PLAYER_KIND_RACE = "K7RS_PLAYER_KIND_RACE";
    inline constexpr std::string_view PLAYER_KIND_BEAST = "K7RS_PLAYER_KIND_BEAST";
    inline constexpr std::string_view PLAYER_KIND_NONE = "K7RS_PLAYER_KIND_NONE";
    inline constexpr std::string_view PLAYER_SEX = "K7RS_PLAYER_SEX";
    inline constexpr std::string_view SEX_MALE = "K7RS_SEX_MALE";
    inline constexpr std::string_view SEX_FEMALE = "K7RS_SEX_FEMALE";
    inline constexpr std::string_view SEX_UNKNOWN = "K7RS_SEX_UNKNOWN";
    inline constexpr std::string_view PLAYER_BASE = "K7RS_PLAYER_BASE";
    inline constexpr std::string_view PLAYER_VALUE = "K7RS_PLAYER_VALUE";
    inline constexpr std::string_view PLAYER_VALUE_HINT = "K7RS_PLAYER_VALUE_HINT";
    inline constexpr std::string_view PLAYER_VALUE_NONE = "K7RS_PLAYER_VALUE_NONE";
    inline constexpr std::string_view PLAYER_BREAKDOWN = "K7RS_PLAYER_BREAKDOWN";
    inline constexpr std::string_view PLAYER_CONTRIBUTION = "K7RS_PLAYER_CONTRIBUTION";
    inline constexpr std::string_view PLAYER_CONTRIBUTION_HINT = "K7RS_PLAYER_CONTRIBUTION_HINT";
    inline constexpr std::string_view PLAYER_TOTAL = "K7RS_PLAYER_TOTAL";
    inline constexpr std::string_view PLAYER_SURVIVAL = "K7RS_PLAYER_SURVIVAL";
    inline constexpr std::string_view PLAYER_SURVIVAL_LEFT = "K7RS_PLAYER_SURVIVAL_LEFT";
    inline constexpr std::string_view PLAYER_OTHER = "K7RS_PLAYER_OTHER";
    inline constexpr std::string_view FILES = "K7RS_FILES";
    inline constexpr std::string_view ERRORS_HEADER = "K7RS_ERRORS_HEADER";
    inline constexpr std::string_view ERRORS_NONE = "K7RS_ERRORS_NONE";
    inline constexpr std::string_view GENERAL_HEADER = "K7RS_GENERAL_HEADER";
    inline constexpr std::string_view GENERAL_ENABLED = "K7RS_GENERAL_ENABLED";
    inline constexpr std::string_view GENERAL_ENABLED_HINT = "K7RS_GENERAL_ENABLED_HINT";
    inline constexpr std::string_view GENERAL_MIN_TOTAL = "K7RS_GENERAL_MIN_TOTAL";
    inline constexpr std::string_view GENERAL_MIN_TOTAL_HINT = "K7RS_GENERAL_MIN_TOTAL_HINT";
    inline constexpr std::string_view GENERAL_SURVIVAL_PENALTY = "K7RS_GENERAL_SURVIVAL_PENALTY";
    inline constexpr std::string_view GENERAL_SURVIVAL_PENALTY_HINT = "K7RS_GENERAL_SURVIVAL_PENALTY_HINT";
    inline constexpr std::string_view GENERAL_LOG_LEVEL = "K7RS_GENERAL_LOG_LEVEL";
    inline constexpr std::string_view LOG_TRACE = "K7RS_LOG_TRACE";
    inline constexpr std::string_view LOG_DEBUG = "K7RS_LOG_DEBUG";
    inline constexpr std::string_view LOG_INFO = "K7RS_LOG_INFO";
    inline constexpr std::string_view LOG_WARN = "K7RS_LOG_WARN";
    inline constexpr std::string_view LOG_ERROR = "K7RS_LOG_ERROR";
    inline constexpr std::string_view RACES_HEADER = "K7RS_RACES_HEADER";
    inline constexpr std::string_view RACES_COLUMN_RACE = "K7RS_RACES_COLUMN_RACE";
    inline constexpr std::string_view RACES_COLUMN_MALE = "K7RS_RACES_COLUMN_MALE";
    inline constexpr std::string_view RACES_COLUMN_FEMALE = "K7RS_RACES_COLUMN_FEMALE";
    inline constexpr std::string_view RACES_VALUE_HINT = "K7RS_RACES_VALUE_HINT";
    inline constexpr std::string_view RACES_EDITED = "K7RS_RACES_EDITED";
    inline constexpr std::string_view RACES_REVERT = "K7RS_RACES_REVERT";
    inline constexpr std::string_view RACES_REVERT_HINT = "K7RS_RACES_REVERT_HINT";
    inline constexpr std::string_view RACES_REMOVE = "K7RS_RACES_REMOVE";
    inline constexpr std::string_view RACES_ADD_CURRENT = "K7RS_RACES_ADD_CURRENT";
    inline constexpr std::string_view RACES_FORMS = "K7RS_RACES_FORMS";
    inline constexpr std::string_view RACE_ORC = "K7RS_RACE_ORC";
    inline constexpr std::string_view RACE_NORD = "K7RS_RACE_NORD";
    inline constexpr std::string_view RACE_REDGUARD = "K7RS_RACE_REDGUARD";
    inline constexpr std::string_view RACE_ARGONIAN = "K7RS_RACE_ARGONIAN";
    inline constexpr std::string_view RACE_IMPERIAL = "K7RS_RACE_IMPERIAL";
    inline constexpr std::string_view RACE_DUNMER = "K7RS_RACE_DUNMER";
    inline constexpr std::string_view RACE_BRETON = "K7RS_RACE_BRETON";
    inline constexpr std::string_view RACE_ALTMER = "K7RS_RACE_ALTMER";
    inline constexpr std::string_view RACE_BOSMER = "K7RS_RACE_BOSMER";
    inline constexpr std::string_view RACE_KHAJIIT = "K7RS_RACE_KHAJIIT";
    inline constexpr std::string_view FORM_ADD = "K7RS_FORM_ADD";
    inline constexpr std::string_view FORM_REMOVE = "K7RS_FORM_REMOVE";
    inline constexpr std::string_view FORM_INPUT_HINT = "K7RS_FORM_INPUT_HINT";
    inline constexpr std::string_view FORM_BAD_FORMAT = "K7RS_FORM_BAD_FORMAT";
    inline constexpr std::string_view FORM_NOT_FOUND = "K7RS_FORM_NOT_FOUND";
    inline constexpr std::string_view FORM_NOT_RACE = "K7RS_FORM_NOT_RACE";
    inline constexpr std::string_view FORM_NOT_EFFECT = "K7RS_FORM_NOT_EFFECT";
    inline constexpr std::string_view FORM_DUPLICATE = "K7RS_FORM_DUPLICATE";
    inline constexpr std::string_view FORM_BUILT_IN = "K7RS_FORM_BUILT_IN";
    inline constexpr std::string_view BEAST_HEADER = "K7RS_BEAST_HEADER";
    inline constexpr std::string_view BEAST_HINT = "K7RS_BEAST_HINT";
    inline constexpr std::string_view BEAST_ADD_CURRENT = "K7RS_BEAST_ADD_CURRENT";
    inline constexpr std::string_view SURVIVAL_HEADER = "K7RS_SURVIVAL_HEADER";
    inline constexpr std::string_view SURVIVAL_HINT = "K7RS_SURVIVAL_HINT";
    inline constexpr std::string_view SAVE = "K7RS_SAVE";
    inline constexpr std::string_view DISCARD = "K7RS_DISCARD";
    inline constexpr std::string_view RELOAD = "K7RS_RELOAD";
    inline constexpr std::string_view UNSAVED = "K7RS_UNSAVED";
    inline constexpr std::string_view SAVED = "K7RS_SAVED";
    inline constexpr std::string_view SAVE_FAILED = "K7RS_SAVE_FAILED";
    inline constexpr std::string_view CANCEL = "K7RS_CANCEL";
    inline constexpr std::string_view CHANGED_TITLE = "K7RS_CHANGED_TITLE";
    inline constexpr std::string_view CHANGED_TEXT = "K7RS_CHANGED_TEXT";
    inline constexpr std::string_view CHANGED_OVERWRITE = "K7RS_CHANGED_OVERWRITE";
    inline constexpr std::string_view CHANGED_RELOAD = "K7RS_CHANGED_RELOAD";
    inline constexpr std::string_view BROKEN_TITLE = "K7RS_BROKEN_TITLE";
    inline constexpr std::string_view BROKEN_TEXT = "K7RS_BROKEN_TEXT";
    inline constexpr std::string_view BROKEN_CLEAN = "K7RS_BROKEN_CLEAN";

    inline constexpr std::array kAll{
        MENU_SECTION,
        MENU_SETTINGS,
        PLAYER_HEADER,
        PLAYER_NONE,
        PLAYER_RACE,
        PLAYER_KIND_RACE,
        PLAYER_KIND_BEAST,
        PLAYER_KIND_NONE,
        PLAYER_SEX,
        SEX_MALE,
        SEX_FEMALE,
        SEX_UNKNOWN,
        PLAYER_BASE,
        PLAYER_VALUE,
        PLAYER_VALUE_HINT,
        PLAYER_VALUE_NONE,
        PLAYER_BREAKDOWN,
        PLAYER_CONTRIBUTION,
        PLAYER_CONTRIBUTION_HINT,
        PLAYER_TOTAL,
        PLAYER_SURVIVAL,
        PLAYER_SURVIVAL_LEFT,
        PLAYER_OTHER,
        FILES,
        ERRORS_HEADER,
        ERRORS_NONE,
        GENERAL_HEADER,
        GENERAL_ENABLED,
        GENERAL_ENABLED_HINT,
        GENERAL_MIN_TOTAL,
        GENERAL_MIN_TOTAL_HINT,
        GENERAL_SURVIVAL_PENALTY,
        GENERAL_SURVIVAL_PENALTY_HINT,
        GENERAL_LOG_LEVEL,
        LOG_TRACE,
        LOG_DEBUG,
        LOG_INFO,
        LOG_WARN,
        LOG_ERROR,
        RACES_HEADER,
        RACES_COLUMN_RACE,
        RACES_COLUMN_MALE,
        RACES_COLUMN_FEMALE,
        RACES_VALUE_HINT,
        RACES_EDITED,
        RACES_REVERT,
        RACES_REVERT_HINT,
        RACES_REMOVE,
        RACES_ADD_CURRENT,
        RACES_FORMS,
        RACE_ORC,
        RACE_NORD,
        RACE_REDGUARD,
        RACE_ARGONIAN,
        RACE_IMPERIAL,
        RACE_DUNMER,
        RACE_BRETON,
        RACE_ALTMER,
        RACE_BOSMER,
        RACE_KHAJIIT,
        FORM_ADD,
        FORM_REMOVE,
        FORM_INPUT_HINT,
        FORM_BAD_FORMAT,
        FORM_NOT_FOUND,
        FORM_NOT_RACE,
        FORM_NOT_EFFECT,
        FORM_DUPLICATE,
        FORM_BUILT_IN,
        BEAST_HEADER,
        BEAST_HINT,
        BEAST_ADD_CURRENT,
        SURVIVAL_HEADER,
        SURVIVAL_HINT,
        SAVE,
        DISCARD,
        RELOAD,
        UNSAVED,
        SAVED,
        SAVE_FAILED,
        CANCEL,
        CHANGED_TITLE,
        CHANGED_TEXT,
        CHANGED_OVERWRITE,
        CHANGED_RELOAD,
        BROKEN_TITLE,
        BROKEN_TEXT,
        BROKEN_CLEAN,
    };
}
