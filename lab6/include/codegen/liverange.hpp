#ifndef LIVERANGE_HPP
#define LIVERANGE_HPP

#include "Function.hpp"
#include "Module.hpp"
#include "Value.hpp"

#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <regex>

using std::map;
using std::pair;
using std::set;
using std::string;
using std::to_string;
using std::vector;

#define UNINITIAL -1

#define __LRA_PRINT__

namespace LRA {

struct Interval {
    Interval(int a = UNINITIAL, int b = UNINITIAL) : i(a), j(b) {}
    int i; // 0 means uninitialized
    int j;
};

using LiveSet = set<Value *>;
using PhiMap = map<BasicBlock *, vector<pair<Value *, Value *>>>;
using LiveInterval = pair<Interval, Value *>;

//对活跃变量进行排序，此处采用按活跃区间的起始点进行排序
struct LiveIntervalCMP {
    bool operator()(LiveInterval const &lhs, LiveInterval const &rhs) const {
        std::regex pattern_arg("arg\\d+");
        bool is_lhs_arg = std::regex_match(lhs.second->get_name(), pattern_arg);
        bool is_rhs_arg = std::regex_match(rhs.second->get_name(), pattern_arg);
        if(is_lhs_arg && !is_rhs_arg)
            return true;
        if(!is_lhs_arg && is_rhs_arg)
            return false;
        if (lhs.first.i != rhs.first.i)
            return lhs.first.i < rhs.first.i;
        else
            return lhs.second < rhs.second;
    }
};
using LVITS = set<LiveInterval, LiveIntervalCMP>;

class LiveRangeAnalyzer {
    friend class CodeGen;

  private:
    Module *m;
    map<Value *, Interval> intervalmap;
    vector<BasicBlock*> BB_DFS_Order;
    map<int, LiveSet> IN, OUT;
    //phi_map: 标识在bb中的copy-statement
    const PhiMap &phi_map;
    LVITS liveIntervals;    
    //TODO:添加你需要的变量

    void get_dfs_order(Function*);
    void make_id(Function *);
    void make_interval(Function *);

    LiveSet joinFor(BasicBlock *bb);
    void union_ip(LiveSet &dest, LiveSet &src) {
        LiveSet res;
        set_union(dest.begin(),
                  dest.end(),
                  src.begin(),
                  src.end(),
                  std::inserter(res, res.begin()));
        dest = res;
    }
    LiveSet transferFunction(Instruction *);

  public:
    LiveRangeAnalyzer(Module *m_, PhiMap &phi_map_)
        : m(m_), phi_map(phi_map_) {}
    LiveRangeAnalyzer() = delete;

    // void run();
    void run(Function *);
    void clear();
    static string print_liveSet(const LiveSet &ls) {
        string s = "[ ";
        for (auto k : ls)
            s += k->get_name() + " ";
        s += "]";
        return s;
    }
    static string print_interval(const Interval &i) {
        return "<" + to_string(i.i) + ", " + to_string(i.j) + ">";
    }
    const LVITS &get() { return liveIntervals; }
    const decltype(intervalmap) &get_interval_map() const {
        return intervalmap;
    }
    const decltype(IN) &get_in_set() const { return IN; }
    const decltype(OUT) &get_out_set() const { return OUT; }
};
} // namespace LRA
#endif
//TODO: 这只是一个样例，你可以自行对框架进行修改以符合你自己的心意
//TODO: 对框架不满可尽情修改