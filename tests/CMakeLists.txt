find_package(Qt6 REQUIRED COMPONENTS Test)

function(add_remustwo_test target_name ctest_name)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES LIBS)
    cmake_parse_arguments(ART "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ART_SOURCES)
        set(ART_SOURCES "${target_name}.cpp")
    endif()

    add_executable(${target_name} ${ART_SOURCES})
    target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(${target_name} PRIVATE ${ART_LIBS})
    target_compile_definitions(${target_name} PRIVATE REMUSTWO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME ${ctest_name} COMMAND ${target_name})
    set_tests_properties(${ctest_name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_remustwo_test(test_catalog_pipeline CatalogPipelineTest
    SOURCES test_catalog_pipeline.cpp
    LIBS Qt6::Test remustwo-catalog
)

add_remustwo_test(test_catalog_init CatalogInitTest
    SOURCES test_catalog_init.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-catalog
)

add_remustwo_test(test_result ResultTest LIBS Qt6::Test)

add_remustwo_test(test_constants ConstantsTest
    SOURCES test_constants.cpp
    LIBS Qt6::Test remustwo-constants
)

add_remustwo_test(test_hash_algorithms HashAlgorithmsTest
    SOURCES test_hash_algorithms.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_hasher HasherTest
    SOURCES test_hasher.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_header_detector HeaderDetectorTest
    SOURCES test_header_detector.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_ra_hasher RaHasherTest
    SOURCES test_ra_hasher.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_chd_header ChdHeaderTest
    SOURCES test_chd_header.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_dat_parser DatParserTest
    SOURCES test_dat_parser.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_matching_engine MatchingEngineTest
    SOURCES test_matching_engine.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_title_similarity TitleSimilarityTest
    SOURCES test_title_similarity.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_match_utils MatchUtilsTest
    SOURCES test_match_utils.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_disc_title_parser DiscTitleParserTest
    SOURCES test_disc_title_parser.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_disc_set_utils DiscSetUtilsTest
    SOURCES test_disc_set_utils.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_scanner ScannerTest
    SOURCES test_scanner.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_system_detector SystemDetectorTest
    SOURCES test_system_detector.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_system_name_resolution SystemNameResolutionTest
    SOURCES test_system_name_resolution.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-core
)

add_remustwo_test(test_multi_file_detection MultiFileDetectionTest
    SOURCES test_multi_file_detection.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_database DatabaseTest
    SOURCES test_database.cpp test_database_files.cpp test_database_relations.cpp test_database_fixture.h
    LIBS Qt6::Test Qt6::Sql remustwo-core
)

add_remustwo_test(test_database_disc_sets DatabaseDiscSetsTest
    SOURCES test_database_disc_sets.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-core
)

add_remustwo_test(test_template_engine TemplateEngineTest
    SOURCES test_template_engine.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_organize_engine OrganizeEngineTest
    SOURCES test_organize_engine.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-core
)

add_remustwo_test(test_verification_hash_matcher VerificationHashMatcherTest
    SOURCES test_verification_hash_matcher.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_chd_converter ChdConverterTest
    SOURCES test_chd_converter.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_hasheous_import HasheousImportTest
    SOURCES test_hasheous_import.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-metadata
)

add_remustwo_test(test_library_enrich LibraryEnrichTest
    SOURCES test_library_enrich.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-metadata
)

add_remustwo_test(test_rom_bundler RomBundlerTest
    SOURCES test_rom_bundler.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-catalog remustwo-core
)

add_remustwo_test(test_verification_engine VerificationEngineTest
    SOURCES test_verification_engine.cpp test_verification_engine_reports.cpp test_verification_engine_fixture.h
    LIBS Qt6::Test Qt6::Sql remustwo-core
)

add_remustwo_test(test_m3u_generator M3UGeneratorTest
    SOURCES test_m3u_generator.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-core
)

add_remustwo_test(test_patch_engine PatchEngineTest
    SOURCES test_patch_engine.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_cso_converter CsoConverterTest
    SOURCES test_cso_converter.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_wbfs_converter WbfsConverterTest
    SOURCES test_wbfs_converter.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_rvz_converter RvzConverterTest
    SOURCES test_rvz_converter.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_pbp_exporter PbpExporterTest
    SOURCES test_pbp_exporter.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_external_tool_runner ExternalToolRunnerTest
    SOURCES test_external_tool_runner.cpp
    LIBS Qt6::Test remustwo-core
)

add_remustwo_test(test_compendium_dat_extractor CompendiumDatExtractorTest
    SOURCES test_compendium_dat_extractor.cpp
    LIBS Qt6::Test remustwo-catalog
)

add_remustwo_test(test_compendium_source_purge CompendiumSourcePurgeTest
    SOURCES test_compendium_source_purge.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-catalog
)

add_remustwo_test(test_compendium_merge_resolver CompendiumMergeResolverTest
    SOURCES test_compendium_merge_resolver.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-catalog
)

add_remustwo_test(test_compendium_disc_set_inserter CompendiumDiscSetInserterTest
    SOURCES test_compendium_disc_set_inserter.cpp
    LIBS Qt6::Test Qt6::Sql remustwo-catalog
)

if(REMUSTWO_ENABLE_LIBARCHIVE)
    add_remustwo_test(test_archive_extractor ArchiveExtractorTest
        SOURCES test_archive_extractor.cpp
        LIBS Qt6::Test remustwo-core
    )
endif()
