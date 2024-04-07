#include "regalloc.hpp"

#include "Function.hpp"
#include "Instruction.hpp"
#include "liverange.hpp"

#include <algorithm>

using std::for_each;

using namespace RA;

#define ASSERT_CMPINST_USED_ONCE(cmpinst) (assert(cmpinst->get_use_list().size() <= 1))

int get_arg_id(Argument *arg) {
    int id = 1;
    for(auto &arg_test : arg->get_parent()->get_args()){
        if(&arg_test == arg)
            break;
        ++id;
    }
    return id;
}

bool RegAllocator::no_reg_alloca(Value *v) {
    auto instr = dynamic_cast<Instruction *>(v);
    auto arg = dynamic_cast<Argument *>(v);
    if (instr) {
        //TODO: 判断哪些指令需要分配寄存器
    }
    if (arg) { // 只为前8个参数分配寄存器
        return get_arg_id(arg) > ARG_MAX_R;
    } else
        assert(false && "only instruction and argument's LiveInterval exits");
}

void RegAllocator::reset(Function *func) {
    //TODO: 对相关参数进行初始化
}

void RegAllocator::ReserveForArg(const LVITS &liveints) {
    //TODO: 先对函数参数进行分配，分完为止，在本实验框架中会对INT/FLOAT进行分类讨论，在考虑INT类型时无需考虑FLOAT类型变量，反之同理
}

// input set is sorted by increasing start point
void RegAllocator::LinearScan(const LVITS &liveints, Function *func) {
    reset(func);
    ReserveForArg(liveints);
    int reg;
    for (auto liveint : liveints) {

        //TODO: 考虑有哪些情况的liveint不需要进行分析

        ExpireOldIntervals(liveint);
        if (active.size() == R)
            SpillAtInterval(liveint);
        else {
            for (reg = 1; reg <= R and used[reg]; ++reg);
            used[reg] = true;
            regmap[liveint.second] = reg;
            active.insert(liveint);
        }
    }
}

void RegAllocator::ExpireOldIntervals(LiveInterval liveint) {
    //TODO: 清除与当前的liveint已无交集的活跃变量
}

void RegAllocator::SpillAtInterval(LiveInterval liveint) {
    //TODO: 处理溢出情况
}
//TODO: 对框架不满可尽情修改