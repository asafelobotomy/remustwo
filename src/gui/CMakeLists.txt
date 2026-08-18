set(CMAKE_AUTORCC ON)

qt_add_executable(remustwo-gui
    main.cpp
    qml/resources.qrc
)
target_link_libraries(remustwo-gui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Sql
    remustwo-core
    remustwo-catalog
    remustwo-metadata
)
