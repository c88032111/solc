aux_source_directory(. SRC_LIST)

set(EXECUTABLE lllc)

file(GLOB HEADERS "*.h")
include_directories(BEFORE ..)
gdtu_simple_add_executable(${EXECUTABLE} ${SRC_LIST} ${HEADERS})

gdtu_use(${EXECUTABLE} REQUIRED Solidity::lll Dev::buildinfo Solidity::solevmasm)

install( TARGETS ${EXECUTABLE} DESTINATION bin )

