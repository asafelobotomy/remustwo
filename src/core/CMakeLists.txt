add_library(remustwo-constants STATIC
    systems.cpp
    systems_defs.cpp
    systems_extensions.cpp
    systems_db_extensions.cpp
    systems_registry.cpp
)
target_include_directories(remustwo-constants PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/catalog
)
target_compile_definitions(remustwo-constants PUBLIC
    REMUSTWO_CATALOG_DATA_DIR="${CMAKE_SOURCE_DIR}/data/catalog"
)
target_link_libraries(remustwo-constants PUBLIC Qt6::Core Qt6::Sql)

add_library(remustwo-core STATIC
    chd_header.cpp
    database.cpp
    database_disc_sets.cpp
    database_files.cpp
    database_games.cpp
    database_migrations.cpp
    database_migrations_systems.cpp
    database_schema.cpp
    database_patches.cpp
    database_file_state.cpp
    dat_parser.cpp
    disc_set_key.cpp
    disc_set_utils.cpp
    disc_title_parser.cpp
    hasher.cpp
    header_detector.cpp
    logging_categories.cpp
    matching_engine.cpp
    match_utils.cpp
    ra_hasher.cpp
    scanner.cpp
    scanner_multifile.cpp
    system_detector.cpp
    system_resolver.cpp
    system_resolver_dat.cpp
    system_resolver_provider_mappings.cpp
    title_similarity.cpp
    archive_extractor.cpp
    archive_creator.cpp
    library_paths.cpp
    library_scan.cpp
    compendium_disc_bridge.cpp
    patched_rom_parser.cpp
    disc_magic_detector.cpp
    verification_hash_matcher.cpp
    organize_engine.cpp
    organize_engine_undo.cpp
    template_engine.cpp
    verification_engine.cpp
    verification_engine_verify.cpp
    verification_engine_report.cpp
    verification_storage_schema.cpp
    verification_storage.cpp
    verification_storage_dat.cpp
    verification_engine_disc_sets.cpp
    m3u_generator.cpp
    patch_engine.cpp
    patch_engine_apply.cpp
    disc_converter.cpp
    chd_converter.cpp
    cso_converter.cpp
    wbfs_converter.cpp
    rvz_converter.cpp
    pbp_exporter.cpp
    external_tool_runner.cpp
    rom_bundler.cpp
)
target_include_directories(remustwo-core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
find_package(ZLIB REQUIRED)
target_link_libraries(remustwo-core PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Concurrent
    ZLIB::ZLIB
    remustwo-constants
)
if(REMUSTWO_ENABLE_LIBARCHIVE)
    target_sources(remustwo-core PRIVATE
        archive_extractor_extract.cpp
        archive_extractor_info.cpp
        archive_creator_compress.cpp
    )
    target_compile_definitions(remustwo-core PUBLIC REMUSTWO_HAS_LIBARCHIVE=1)
    target_link_libraries(remustwo-core PUBLIC LibArchive::LibArchive)
endif()
