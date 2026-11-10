include(FetchContent)

FetchContent_Declare(
    gdr
    GIT_REPOSITORY https://github.com/maxnut/GDReplayFormat.git
    GIT_TAG        4950cc287aaf12d4a92a6bd967aa664d4846d760
    GIT_PROGRESS TRUE
)
message("Fetching gdr")
FetchContent_MakeAvailable(gdr)

# GDReplayFormat adds a native test executable and runs it after linking.
# During an Android cross-build that executable targets Android ARM64 and
# cannot be launched by the Linux x86_64 GitHub Actions runner. The mod only
# needs the header-only libGDR target, so keep the test out of the default build.
if(ANDROID AND TARGET libGDR_test)
    set_property(TARGET libGDR_test PROPERTY EXCLUDE_FROM_ALL TRUE)
endif()
