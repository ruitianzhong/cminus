#include "LoopInvHoist.hpp"

#include "LoopSearch.hpp"
#include "logging.hpp"

#include <algorithm>

void LoopInvHoist::run() {
    LoopSearch loop_searcher(m_, false);
    loop_searcher.run();

    LOG(INFO) << "====== Loop invariant motion started ======";

    LoopTree loop_tree;
    // TODO: construct loop tree and do loop invariant hoisting

    LOG(INFO) << "====== Loop invariant motion ended ======";
}

// Optimize from leaf nodes on the loop tree up to the root nodes.
void LoopInvHoist::hoist(BBset_t *loop,
                         LoopTree &loop_tree,
                         LoopSearch &loop_searcher,
                         BBset_t &vis) {
    for (auto subloop : loop_tree[loop]) {
        hoist(subloop, loop_tree, loop_searcher, vis);
    }

    if (!loop) {
        return;
    }

    auto base = loop_searcher.get_loop_base(loop);
    std::vector<Instruction *> loop_invs;
    // TODO: find loop invariants, insert them into loop_invs

    if (!loop_invs.empty()) {
        // Insert to the block just before the base block.
        BasicBlock *dest = nullptr;
        for (auto prec : base->get_pre_basic_blocks()) {
            if (!loop->count(prec)) {
                dest = prec;
                break;
            }
        }
        if (dest) {
            // TODO: insert loop_invs to dest
        } else {
            LOG(ERROR) << "This loop doesn't have an entry block?!";
        }
    }

    // Mark this loop body as analyzed
    for (auto bb : *loop) {
        vis.insert(bb);
    }
}

// A instruction can be moved <= no side effects (memory stores included)
// PHIs are excluded because we don't want to modify them.
bool LoopInvHoist::can_move(Instruction *instr) {
    return instr->isBinary() || instr->is_si2fp() || instr->is_fp2si() || instr->is_zext() ||
           instr->is_cmp() || instr->is_fcmp() || instr->is_gep();
}

// Returns false if instr involves any value that is assigned inside loop.
bool LoopInvHoist::is_inv(Value *value, BBset_t *loop) {
    // TODO
    return false;
}
