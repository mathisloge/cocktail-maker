add_library(cocktail-maker-simulator)
add_sanitizers(cocktail-maker-simulator)
target_link_libraries(cocktail-maker-simulator PUBLIC cm::lib PRIVATE proto)
target_sources(
    cocktail-maker-simulator
    PUBLIC FILE_SET CXX_MODULES FILES lib.cppm client.cppm
)
