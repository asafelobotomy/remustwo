add_library(remustwo-catalog STATIC
    catalog.cpp
    catalog_ingest.cpp
    catalog_match.cpp
    ${CMAKE_SOURCE_DIR}/src/metadata/metadata_provider.cpp
    clrmamepro_parser.cpp
    thumbnail_url_helper.cpp
    compendium_compiler_service.cpp
    compendium_dat_extractor.cpp
    compendium_normalizer.cpp
    compendium_identity_linker.cpp
    compendium_fact_inserter.cpp
    compendium_disc_set_inserter.cpp
    compendium_source_purge.cpp
    compendium_merge_resolver.cpp
    compendium_provider.cpp
    compendium_provider_fetch.cpp
    compendium_provider_lookup.cpp
    compendium_provider_match.cpp
    compendium_provider_disc_sets.cpp
)
target_include_directories(remustwo-catalog PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(remustwo-catalog PUBLIC
    REMUSTWO_CATALOG_DATA_DIR="${CMAKE_SOURCE_DIR}/data/catalog"
    REMUSTWO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(remustwo-catalog PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Concurrent
    remustwo-core
)
