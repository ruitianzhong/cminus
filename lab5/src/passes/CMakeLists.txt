add_library(
    opt_lib STATIC
    ConstPropagation.cpp
    DeadCode.cpp
    Dominators.cpp
    FuncInfo.cpp
    GVN.cpp
    LoopInvHoist.cpp
    LoopSearch.cpp
    Mem2Reg.cpp
)
