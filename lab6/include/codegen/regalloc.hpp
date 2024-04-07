#ifndef REGALLOCA_HPP
#define REGALLOCA_HPP
#include "Function.hpp"
#include "Value.hpp"
#include "liverange.hpp"
#include "logging.hpp"

#include <iostream>
#include <string>
#include <regex>

using namespace LRA;

namespace RA {

//一共32个寄存器，只使用其中8个
#define MAXR 32
#define ARG_MAX_R 8

struct ActiveCMP {
    bool operator()(LiveInterval const &lhs, LiveInterval const &rhs) const {
        std::regex pattern_arg("arg\\d+");
        bool is_lhs_arg = std::regex_match(lhs.second->get_name(), pattern_arg);
        bool is_rhs_arg = std::regex_match(rhs.second->get_name(), pattern_arg);
        if(is_lhs_arg && !is_rhs_arg)
            return true;
        if(!is_lhs_arg && is_rhs_arg)
            return false;
        if (lhs.first.j != rhs.first.j)
            return lhs.first.j < rhs.first.j;
        else if (lhs.first.i != rhs.first.i)
            return lhs.first.i < rhs.first.i;
        else
            return lhs.second < rhs.second;
    }
};

class RegAllocator {
  private:
    Function *cur_func;
    const bool FLOAT;
    const uint R;
    bool used[MAXR + 1]; // index range: 1 ~ R
    map<Value *, int> regmap;
    set<LiveInterval, ActiveCMP> active;
    //TODO:添加你需要的变量
    
    void reset(Function * = nullptr);
    void ReserveForArg(const LVITS &);
    void ExpireOldIntervals(LiveInterval);
    void SpillAtInterval(LiveInterval);

  public:
    RegAllocator(const uint R_, bool fl) : FLOAT(fl), R(R_), used{false} {
        LOG_DEBUG << "RegAllocator initialize: R=" << R << "\n";
        assert(R <= MAXR);
    }
    RegAllocator() = delete;

    //判断当前v是否需要分配寄存器，需要则返回false
    static bool no_reg_alloca(Value *v);
    void LinearScan(const LVITS &, Function *);
    const map<Value *, int> &get() const { return regmap; }
    void print(string (*regname)(int)) {
        for (auto [op, reg] : regmap)
            LOG_DEBUG << op->get_name() << " ~ " << regname(reg) << "\n";
    }
};
} // namespace RA
#endif
//TODO: 这只是一个样例，你可以自行对框架进行修改以符合你自己的心意
//TODO: 对框架不满可尽情修改