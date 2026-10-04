include(CheckIPOSupported)

if(ROCKETPLOT_ENABLE_IPO)
    check_ipo_supported(RESULT ROCKETPLOT_IPO_SUPPORTED OUTPUT ipo_output)
    if(NOT ROCKETPLOT_IPO_SUPPORTED)
        message(WARNING "ROCKETPLOT_ENABLE_IPO is ON, but this toolchain cannot do IPO:\n${ipo_output}")
    endif()
endif()

if("thread" IN_LIST ROCKETPLOT_SANITIZERS AND "address" IN_LIST ROCKETPLOT_SANITIZERS)
    message(FATAL_ERROR "ROCKETPLOT_SANITIZERS: 'thread' cannot be combined with 'address'")
endif()

if(ROCKETPLOT_SANITIZERS AND MSVC)
    message(FATAL_ERROR "ROCKETPLOT_SANITIZERS needs GCC or Clang (asan/tsan presets: Linux, macOS)")
endif()

if(ROCKETPLOT_ENABLE_COVERAGE AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "ROCKETPLOT_ENABLE_COVERAGE uses llvm-cov and needs Clang. Use the coverage preset.")
endif()

if(ROCKETPLOT_ENABLE_CLANG_TIDY)
    # Xcode has no clang-tidy; on macOS it comes from Homebrew's llvm, which is not on PATH.
    find_program(CLANG_TIDY_PROGRAM clang-tidy
                 HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin REQUIRED)
    # GCC-only -W flags are unknown to clang-tidy's Clang frontend; don't report them.
    set(ROCKETPLOT_CLANG_TIDY_COMMAND ${CLANG_TIDY_PROGRAM} --extra-arg=-Wno-unknown-warning-option)
    if(ROCKETPLOT_WARNINGS_AS_ERRORS)
        list(APPEND ROCKETPLOT_CLANG_TIDY_COMMAND --warnings-as-errors=*)
    endif()
endif()

# rocketplot_sanitize_target(<target>)
# Builds a target with ROCKETPLOT_SANITIZERS (GCC and Clang; nothing without sanitizers). rocketplot_configure_target()
# does this for our targets. Called by itself for a dependency that is built here and handles the same standard
# containers as our code, like GoogleTest (see Dependencies.cmake).
function(rocketplot_sanitize_target target)
    if(NOT ROCKETPLOT_SANITIZERS)
        return()
    endif()
    list(JOIN ROCKETPLOT_SANITIZERS "," sanitizers)
    set(sanitize -fsanitize=${sanitizers})
    if("undefined" IN_LIST ROCKETPLOT_SANITIZERS)
        list(APPEND sanitize -fno-sanitize-recover=all)   # UB stops the program, so a test fails
    endif()
    target_compile_options(${target} PRIVATE ${sanitize} -fno-omit-frame-pointer)
    # BUILD_INTERFACE: a sanitized build's users link the runtime too, but the installed package never asks.
    target_link_options(${target} PUBLIC "$<BUILD_INTERFACE:${sanitize}>")   # quoted: a list
    if("address" IN_LIST ROCKETPLOT_SANITIZERS)
        # libstdc++ annotates std::vector's spare capacity for ASan only on request (libc++ does it by default),
        # so reads past size() but within capacity() are caught. Ignored by libc++ and MSVC. All the code that
        # handles a vector must agree on this: a vector annotated in one place and grown in code built without
        # the annotations is reported as a container-overflow.
        target_compile_definitions(${target} PRIVATE _GLIBCXX_SANITIZE_VECTOR)
    endif()
endfunction()

# rocketplot_configure_target(<target>)
# Applies warnings, sanitizers, coverage, clang-tidy and IPO to one of *our* targets (never to dependencies).
# Call it for every target you add.
function(rocketplot_configure_target target)
    if(MSVC)   # cl, and clang-cl (which takes the same options)
        set(msvc_options
            /W4 /permissive- /utf-8 /Zc:__cplusplus $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
            # Off-by-default warnings, enabled at level 1: narrowing conversions (4242, 4254, 4826),
            # a missed override (4263), a non-virtual destructor (4265), always-false comparisons
            # (4287, 4296), pointer truncation (4311), comma/no-effect mistakes (4545-4555),
            # thread-unsafe statics (4640), string literal casts (4905, 4906), copy-init (4928).
            /w14242 /w14254 /w14263 /w14265 /w14287 /w14296 /w14311 /w14545 /w14546 /w14547 /w14549
            /w14555 /w14640 /w14826 /w14905 /w14906 /w14928
            # C4251: an exported class has a member of a non-exported type (std::unique_ptr, QList). Harmless
            # here, because the DLL and its users must use the same compiler and C++ library anyway.
            /wd4251
            $<$<BOOL:${ROCKETPLOT_WARNINGS_AS_ERRORS}>:/WX>)
        # C++ sources only: the resource compiler (rc) stops at options it does not know.
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${msvc_options}>")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnon-virtual-dtor
            -Wold-style-cast -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2
            -Wimplicit-fallthrough -Wcast-align
            $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast>
            $<$<BOOL:${ROCKETPLOT_WARNINGS_AS_ERRORS}>:-Werror>)
        # Bounds-checked operator[] etc. in the standard library, ABI-compatible (unlike _GLIBCXX_DEBUG):
        # libstdc++ (Linux) and libc++ (macOS) each ignore the other's macro. MSVC's Debug STL checks itself.
        target_compile_definitions(${target} PRIVATE
            $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>
            $<$<CONFIG:Debug>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE>)

        rocketplot_sanitize_target(${target})
    endif()

    if(ROCKETPLOT_ENABLE_COVERAGE)
        target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        target_link_options(${target} PUBLIC $<BUILD_INTERFACE:-fprofile-instr-generate>)
    endif()

    if(ROCKETPLOT_ENABLE_CLANG_TIDY)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${ROCKETPLOT_CLANG_TIDY_COMMAND}")
    endif()

    if(ROCKETPLOT_ENABLE_IPO AND ROCKETPLOT_IPO_SUPPORTED)
        set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endfunction()

# rocketplot_configure_qt_target(<target>)
# rocketplot_configure_target() plus what a target that uses Qt needs: moc, no Qt keyword macros (use
# Q_SIGNALS/Q_SLOTS/Q_EMIT), and no Qt API deprecated after the minimum Qt version. Without the deprecation cap, a
# newer Qt (Arch ships the latest) would break -Werror builds with its own deprecation warnings.
function(rocketplot_configure_qt_target target)
    rocketplot_configure_target(${target})
    set_target_properties(${target} PROPERTIES AUTOMOC ON)
    target_compile_definitions(${target} PRIVATE
        QT_NO_KEYWORDS
        QT_DISABLE_DEPRECATED_UP_TO=0x060800
        QT_WARN_DEPRECATED_UP_TO=0x060800)
endfunction()
