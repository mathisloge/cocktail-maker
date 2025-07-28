add_library(cocktail-maker-core STATIC)
add_library(cm::core ALIAS cocktail-maker-core)
target_compile_features(cocktail-maker-core PUBLIC cxx_std_23)
set_target_properties(
    cocktail-maker-core
    PROPERTIES POSITION_INDEPENDENT_CODE ON VISIBILITY_INLINES_HIDDEN ON
)

target_link_libraries(cocktail-maker-core PUBLIC PkgConfig::libgpiodcxx Boost::asio mp-units::core mp-units::systems mp-units::mp-units mp-units::mp-units-contracts )

target_sources(
    cocktail-maker-core
    PUBLIC
        FILE_SET headers
            TYPE HEADERS
            BASE_DIRS include
            FILES
                include/cm/units.hpp
    PRIVATE
        src/units.cpp
)
