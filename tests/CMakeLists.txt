add_executable(abc_essential_prediction_weapons PredictionWeapons.cpp)
target_include_directories(abc_essential_prediction_weapons PRIVATE
    "${PROJECT_SOURCE_DIR}/src")
target_compile_features(abc_essential_prediction_weapons PRIVATE cxx_std_20)
add_test(NAME abc_essential_prediction_weapons
    COMMAND abc_essential_prediction_weapons)
