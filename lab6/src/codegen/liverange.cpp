#include "liverange.hpp"

#include "BasicBlock.hpp"
#include "Function.hpp"
#include "logging.hpp"

#include <deque>

using namespace LRA;

void LiveRangeAnalyzer::clear() {
    //TODO:每次活跃变量分析前清空定义的变量
}

LiveSet LiveRangeAnalyzer::joinFor(BasicBlock *bb) {
    LiveSet out;
    for (auto succ : bb->get_succ_basic_blocks()) {
        auto &irs = succ->get_instructions();
        auto it = irs.begin();
        while (it != irs.end() and it->is_phi())
            ++it;
        assert(it != irs.end() && "need to find first_ir from copy-stmt");
        //TODO: 合并当前bb的所有后继块的IN集合，计算OUT[B]
    }
    return out;
}

void LiveRangeAnalyzer::make_id(Function *func) {
    /*TODO: 按排序后的BB为所有的指令标定行号，这里建议指令行号是指令数的两倍，这样便于分辨指令的IN和OUT.
    由于已经进行了copystmt,所以phi语句无需进行标定*/
    for (auto bb : BB_DFS_Order) {
        for (auto &instr : bb->get_instructions()) {
            if (instr.is_phi())
                continue;
            //TODO: 注意处理时copy-statement需放在br/ret指令前处理
        }
    }
}

void LiveRangeAnalyzer::get_dfs_order(Function *func) {
    //TODO: 对当前函数的BasicBlocks进行DFS并将结果保存在BB_DFS_Order中
}

LiveSet LiveRangeAnalyzer::transferFunction(Instruction *instr) {
    /*TODO: 在平时讲述的活跃变量分析中我们是对每个块计算 IN[B] = use + (OUT[B] - def), 但在具体程序流分析中我们可以对每条语句进行计算来达到等价的效果
    这个函数就用于对每条语句计算instr的 in = use + out - def
    思考对于instr来说它的use和def是什么?这样做相对于对整个BasicBlock计算有什么好处?
    */
}

void LiveRangeAnalyzer::run(Function *func) {
    clear();
    get_dfs_order(func);
    make_id(func);
    bool cont = true;
    while (cont) {
        cont = false;
        // 活跃变量分析是逆向搜索的

        for (auto rit_bb = BB_DFS_Order.rbegin(); rit_bb != BB_DFS_Order.rend(); ++rit_bb) {
            auto bb = *rit_bb;
            bool last_ir = true;
            //TODO:添加你在循环中需要使用的变量


            for (auto rit_ir = bb->get_instructions().rbegin(); rit_ir != bb->get_instructions().rend(); ++rit_ir) {
                auto instr = &(*rit_ir);
                if (instr->is_phi()) {
                    assert(not last_ir && "If phi is the last ir, then data "
                                          "flow fails due to ignorance of phi");
                    continue;
                }
                //TODO:进行活跃变量分析

                if (instr->is_ret() or instr->is_br()) {
                    //TODO: 对copy-statement进行处理，思考它与普通的指令有什么区别
                    auto it = phi_map.find(bb);

                }
            }
        }
    }
    // 将function中的argument添加到IN集合中
    assert(IN.find(0) == IN.end() and OUT.find(0) == OUT.end() && "no instr_id will be mapped to 0");
    IN[0] = OUT[0] = {};
    for (auto &arg : func->get_args())
        IN[0].insert(&arg);
    make_interval(func);

}

void LiveRangeAnalyzer::make_interval(Function *) {
    //TODO:计算每个变量的活跃区间
    for (auto &[op, interval] : intervalmap)
        liveIntervals.insert({interval, op});
}
//TODO: 对框架不满可尽情修改
