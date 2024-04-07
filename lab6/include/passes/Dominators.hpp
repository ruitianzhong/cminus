#pragma once

#include "BasicBlock.hpp"
#include "PassManager.hpp"

#include <list>
#include <map>
#include <set>

class Dominators : public Pass {
  public:
    using BBSet = std::set<BasicBlock *>;

    explicit Dominators(Module *m) : Pass(m) {}
    ~Dominators() = default;
    void run() override;

    BasicBlock *get_idom(BasicBlock *bb) { return idom_.at(bb); }
    const BBSet &get_dominance_frontier(BasicBlock *bb) {
        return dom_frontier_.at(bb);
    }
    const BBSet &get_dom_tree_succ_blocks(BasicBlock *bb) {
        return dom_tree_succ_blocks_.at(bb);
    }

  private:
    void create_reverse_post_order(Function *f);
    void create_idom(Function *f);
    void create_dominance_frontier(Function *f);
    void create_dom_tree_succ(Function *f);

    void post_order_visit(BasicBlock *bb, BBSet &visited);

    void set_idom(BasicBlock *bb, BasicBlock *idom) { idom_[bb] = idom; }
    void add_dominance_frontier(BasicBlock *bb, BasicBlock *dom_frontier_bb) {
        dom_frontier_[bb].insert(dom_frontier_bb);
    }
    void set_dominance_frontier(BasicBlock *bb, BBSet &df) {
        dom_frontier_[bb].clear();
        dom_frontier_[bb].insert(df.begin(), df.end());
    }
    void add_dom_tree_succ_block(BasicBlock *bb, BasicBlock *dom_tree_succ_bb) {
        dom_tree_succ_blocks_[bb].insert(dom_tree_succ_bb);
    }

    BasicBlock *intersect(BasicBlock *b1, BasicBlock *b2);

    // for debug
    void print_idom(Function *f);
    void print_dominance_frontier(Function *f);

    std::list<BasicBlock *> reverse_post_order_{};
    std::map<BasicBlock *, int> post_order_id_{}; // the root has highest ID

    std::map<BasicBlock *, BasicBlock *> idom_{};  // immediate dominance
    std::map<BasicBlock *, BBSet> dom_frontier_{}; // dominance frontier set
    std::map<BasicBlock *, BBSet>
        dom_tree_succ_blocks_{}; // successor blocks of this node in dominance
                                 // tree
};
