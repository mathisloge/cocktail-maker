add_executable(cocktail-maker src/main.cpp)
target_link_libraries(cocktail-maker PRIVATE Slint::Slint)
slint_target_sources(cocktail-maker 
  ui/app-window.slint NAMESPACE ui
)