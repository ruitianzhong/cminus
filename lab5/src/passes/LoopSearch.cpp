#include "LoopSearch.hpp"
#include "logging.hpp"

#include <fstream>
#include <iostream>
#include <unordered_set>

struct CFGNode {
    std::unordered_set<CFGNodePtr> succs;
    std::unordered_set<CFGNodePtr> prevs;
    BasicBlock *bb;
    int index;   // the index of the node in CFG
    int lowlink; // the min index of the node in the strongly connected componets
    bool onStack;
};

void LoopSearch::build_cfg(Function *func, std::unordered_set<CFGNode *> &result) {
    // TODO: build control flow graph used in loop search pass
}

// Tarjan algorithm
bool LoopSearch::strongly_connected_components(CFGNodePtrSet &nodes,
                                               std::unordered_set<CFGNodePtrSet *> &result) {
    index_count = 0;
    stack.clear();
    for (auto n : nodes) {
        if (n->index == -1) {
            traverse(n, result);
        }
    }
    return result.size() != 0;
}

void LoopSearch::traverse(CFGNodePtr n, std::unordered_set<CFGNodePtrSet *> &result) {
    n->index   = index_count++;
    n->lowlink = n->index;
    stack.push_back(n);
    n->onStack = true;

    for (auto su : n->succs) {
        // has not visited su
        if (su->index == -1) {
            traverse(su, result);
            n->lowlink = std::min(su->lowlink, n->lowlink);
        }
        // has visited su
        else if (su->onStack) {
            n->lowlink = std::min(su->index, n->lowlink);
        }
    }

    if (n->index == n->lowlink) {
        // TODO: pop out the nodes in the same strongly connected component from stack
    }
}

CFGNodePtr LoopSearch::find_loop_base(CFGNodePtrSet *set, CFGNodePtrSet &reserved) {
    CFGNodePtr base = nullptr;
    // TODO: find the loop base node

    return base;
}
void LoopSearch::run() {
    auto &func_list = m_->get_functions();
    for (auto &func1 : func_list) {
        auto func = &func1;
        if (func->get_basic_blocks().size() == 0) {
            continue;
        } else {
            CFGNodePtrSet nodes;
            CFGNodePtrSet reserved;
            std::unordered_set<CFGNodePtrSet *> sccs;

            // step 1: build cfg
            // TODO
            // dump graph
            dump_graph(nodes, func->get_name());
            // step 2: find strongly connected graph from external to internal
            // step 3: find loop base node for each strongly connected graph
            // step 4: store result
            // step 5: map each node to loop base
            // step 6: remove loop base node for researching inner loop
            // TODO
            reserved.clear();
            for (auto node : nodes) {
                delete node;
            }
            nodes.clear();
        }

    }
}

void LoopSearch::dump_graph(CFGNodePtrSet &nodes, std::string title) {
    if (dump) {
        std::vector<std::string> edge_set;
        for (auto node : nodes) {
            if (node->bb->get_name() == "") {
                return;
            }
            if (base2loop.find(node->bb) != base2loop.end()) {
                for (auto succ : node->succs) {
                    if (nodes.find(succ) != nodes.end()) {
                        edge_set.insert(edge_set.begin(),
                                        '\t' + node->bb->get_name() + "->" + succ->bb->get_name() +
                                            ';' + '\n');
                    }
                }
                edge_set.insert(edge_set.begin(),
                                '\t' + node->bb->get_name() + " [color=red]" + ';' + '\n');
            } else {
                for (auto succ : node->succs) {
                    if (nodes.find(succ) != nodes.end()) {
                        edge_set.push_back('\t' + node->bb->get_name() + "->" +
                                           succ->bb->get_name() + ';' + '\n');
                    }
                }
            }
        }
        std::string digragh = "digraph G {\n";
        for (auto edge : edge_set) {
            digragh += edge;
        }
        digragh += '}';
        std::ofstream file_output;
        file_output.open(title + ".dot", std::ios::out);

        file_output << digragh;
        file_output.close();
        std::string dot_cmd = "dot -Tpng " + title + ".dot" + " -o " + title + ".png";
        std::system(dot_cmd.c_str());
    }
}

BBset_t *LoopSearch::get_parent_loop(BBset_t *loop) {
    auto base = loop2base[loop];
    for (auto prev : base->get_pre_basic_blocks()) {
        if (loop->find(prev) != loop->end()) {
            continue;
        }
        auto loop = get_inner_loop(prev);
        if (loop == nullptr || loop->find(base) == loop->end()) {
            return nullptr;
        } else {
            return loop;
        }
    }
    return nullptr;
}

std::unordered_set<BBset_t *> LoopSearch::get_loops_in_func(Function *f) {
    return func2loop.count(f) ? func2loop[f] : std::unordered_set<BBset_t *>();
}
