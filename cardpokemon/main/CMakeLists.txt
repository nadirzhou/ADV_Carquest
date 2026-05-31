file(GLOB_RECURSE CARDPOKEMON_HAL_SRCS
    "../../main/hal/*.c"
    "../../main/hal/*.cc"
    "../../main/hal/*.cpp"
    "../../main/apps/utils/audio/*.cpp"
)

idf_component_register(
    SRCS
        "cardpokemon_main.cpp"
        ${CARDPOKEMON_HAL_SRCS}
    INCLUDE_DIRS
        "."
        "../../main"
        "../../main/hal"
        "../../main/apps"
    REQUIRES
        json
        M5GFX
        M5Unified
        esp_hid
        mooncake_log
    EMBED_FILES
        "assets/cardpokemon_logo.png"
)
