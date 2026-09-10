add_library(remustwo-metadata STATIC
    http_client.cpp
    hasheous_provider.cpp
    hasheous_igdb.cpp
    artwork_cache.cpp
    catalog_fetch.cpp
    library_enrich.cpp
    library_organize.cpp
)
target_include_directories(remustwo-metadata PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(remustwo-metadata PUBLIC
    Qt6::Core
    Qt6::Network
    remustwo-catalog
)
