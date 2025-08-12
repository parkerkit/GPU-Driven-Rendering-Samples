add_library(third_party_meshoptimizer STATIC
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/meshoptimizer.h"

    "${THIRD_PARTY_ROOT}/meshoptimizer/src/allocator.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/clusterizer.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/indexanalyzer.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/indexcodec.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/indexgenerator.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/meshletcodec.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/meshletutils.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/opacitymap.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/overdrawoptimizer.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/partition.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/quantization.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/rasterizer.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/simplifier.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/spatialorder.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/stripifier.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/tangentspace.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/vcacheoptimizer.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/vertexcodec.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/vertexfilter.cpp"
    "${THIRD_PARTY_ROOT}/meshoptimizer/src/vfetchoptimizer.cpp"
)

add_library(
    ThirdParty::meshoptimizer
    ALIAS third_party_meshoptimizer
)

target_include_directories(third_party_meshoptimizer
    SYSTEM PUBLIC
        "${THIRD_PARTY_ROOT}/meshoptimizer/src"
)

set_target_properties(third_party_meshoptimizer PROPERTIES
    FOLDER "ThirdParty"
)

third_party_disable_warnings(third_party_meshoptimizer)
