include(FetchContent)

set(JSON_MultipleHeaders OFF)

# deprecated, move to nlohmann-json releases
FetchContent_Declare(
    nhjson
    GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent.git
    GIT_TAG        326308d7512d2168ae00199aec4dd0f714526e89
    GIT_PROGRESS TRUE
)
message("Fetching nlohmann-json")

# This pinned FetchContent wrapper uses an obsolete cmake_minimum_required()
# declaration (< 3.5). CMake 4.x removed compatibility with those versions.
# Raise the policy floor only while configuring this legacy dependency.
if(DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(_GDMO_HAD_POLICY_VERSION_MINIMUM TRUE)
    set(_GDMO_SAVED_POLICY_VERSION_MINIMUM "${CMAKE_POLICY_VERSION_MINIMUM}")
else()
    set(_GDMO_HAD_POLICY_VERSION_MINIMUM FALSE)
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(nhjson)

if(_GDMO_HAD_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM "${_GDMO_SAVED_POLICY_VERSION_MINIMUM}")
else()
    unset(CMAKE_POLICY_VERSION_MINIMUM)
endif()
unset(_GDMO_HAD_POLICY_VERSION_MINIMUM)
unset(_GDMO_SAVED_POLICY_VERSION_MINIMUM)
