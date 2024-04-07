# TODO: 添加cpp文件后需要在这里进行添加
add_library(
    codegen STATIC
    CodeGen.cpp
    CodeGenRegister.cpp
    liverange.cpp
    Register.cpp
    regalloc.cpp
)

target_link_libraries(codegen common IR_lib)