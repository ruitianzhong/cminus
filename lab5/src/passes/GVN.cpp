#include "GVN.hpp"

#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "DeadCode.hpp"
#include "FuncInfo.hpp"
#include "Function.hpp"
#include "Instruction.hpp"
#include "logging.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

using namespace GVNExpression;
using std::string_literals::operator""s;
using std::shared_ptr;

static auto get_const_int_value = [](Value *v) {
    return dynamic_cast<ConstantInt *>(v)->get_value();
};
static auto get_const_fp_value = [](Value *v) {
    return dynamic_cast<ConstantFP *>(v)->get_value();
};
// Constant Propagation helper, folders are done for you
Constant *ConstFolder::compute(Instruction *instr, Constant *value1, Constant *value2) {
    auto op = instr->get_instr_type();
    switch (op) {
    case Instruction::add:
        return ConstantInt::get(get_const_int_value(value1) + get_const_int_value(value2), module_);
    case Instruction::sub:
        return ConstantInt::get(get_const_int_value(value1) - get_const_int_value(value2), module_);
    case Instruction::mul:
        return ConstantInt::get(get_const_int_value(value1) * get_const_int_value(value2), module_);
    case Instruction::sdiv:
        return ConstantInt::get(get_const_int_value(value1) / get_const_int_value(value2), module_);
    case Instruction::fadd:
        return ConstantFP::get(get_const_fp_value(value1) + get_const_fp_value(value2), module_);
    case Instruction::fsub:
        return ConstantFP::get(get_const_fp_value(value1) - get_const_fp_value(value2), module_);
    case Instruction::fmul:
        return ConstantFP::get(get_const_fp_value(value1) * get_const_fp_value(value2), module_);
    case Instruction::fdiv:
        return ConstantFP::get(get_const_fp_value(value1) / get_const_fp_value(value2), module_);
    case Instruction::eq:
        return ConstantInt::get(get_const_int_value(value1) == get_const_int_value(value2),
                                module_);
    case Instruction::ne:
        return ConstantInt::get(get_const_int_value(value1) != get_const_int_value(value2),
                                module_);
    case Instruction::gt:
        return ConstantInt::get(get_const_int_value(value1) > get_const_int_value(value2), module_);
    case Instruction::ge:
        return ConstantInt::get(get_const_int_value(value1) >= get_const_int_value(value2),
                                module_);
    case Instruction::lt:
        return ConstantInt::get(get_const_int_value(value1) < get_const_int_value(value2), module_);
    case Instruction::le:
        return ConstantInt::get(get_const_int_value(value1) <= get_const_int_value(value2),
                                module_);
    case Instruction::feq:
        return ConstantInt::get(get_const_fp_value(value1) == get_const_fp_value(value2), module_);
    case Instruction::fne:
        return ConstantInt::get(get_const_fp_value(value1) != get_const_fp_value(value2), module_);
    case Instruction::fgt:
        return ConstantInt::get(get_const_fp_value(value1) > get_const_fp_value(value2), module_);
    case Instruction::fge:
        return ConstantInt::get(get_const_fp_value(value1) >= get_const_fp_value(value2), module_);
    case Instruction::flt:
        return ConstantInt::get(get_const_fp_value(value1) < get_const_fp_value(value2), module_);
    case Instruction::fle:
        return ConstantInt::get(get_const_fp_value(value1) <= get_const_fp_value(value2), module_);
    default:
        return nullptr;
    }
}

Constant *ConstFolder::compute(Instruction *instr, Constant *value1) {
    auto op = instr->get_instr_type();
    switch (op) {
    case Instruction::sitofp:
        return ConstantFP::get(static_cast<float>(get_const_int_value(value1)), module_);
    case Instruction::fptosi:
        return ConstantInt::get(static_cast<int>(get_const_fp_value(value1)), module_);
    case Instruction::zext:
        return ConstantInt::get(static_cast<int>(get_const_int_value(value1)), module_);
    default:
        return nullptr;
    }
}

namespace utils {
    static std::string print_congruence_class(const CongruenceClass &cc) {
        std::stringstream ss;
        if (cc.index_ == 0) {
            ss << "top class\n";
            return ss.str();
        }
        ss << "\nindex: " << cc.index_ << "\nleader: " << cc.leader_->print()
           << "\nvalue phi: " << (cc.value_phi_ ? cc.value_phi_->print() : "nullptr"s)
           << "\nvalue expr: " << (cc.value_expr_ ? cc.value_expr_->print() : "nullptr"s)
           << "\nmembers: {";
        for (const auto &member : cc.members_) {
            ss << member->print() << "; ";
        }
        ss << "}\n";
        return ss.str();
    }

    static std::string dump_cc_json(const CongruenceClass &cc) {
        std::string json;
        json += "[";
        for (auto *member : cc.members_) {
            if (dynamic_cast<Constant *>(member) != nullptr) {
                json += member->print() + ", ";
            } else {
                json += "\"%" + member->get_name() + "\", ";
            }
        }
        json += "]";
        return json;
    }

    static std::string dump_partition_json(const GVN::partitions &p) {
        std::string json;
        json += "[";
        for (const auto &cc : p) {
            json += dump_cc_json(*cc) + ", ";
        }
        json += "]";
        return json;
    }

    static std::string dump_bb2partition(const std::map<BasicBlock *, GVN::partitions> &map) {
        std::string json;
        json += "{";
        for (auto [bb, p] : map) {
            json += "\"" + bb->get_name() + "\": " + dump_partition_json(p) + ",";
        }
        json += "}";
        return json;
    }

    // logging utility for you
    static void print_partitions(const GVN::partitions &p) {
        if (p.empty()) {
            LOG_DEBUG << "empty partitions\n";
            return;
        }
        std::string log;
        for (const auto &cc : p) {
            log += print_congruence_class(*cc);
        }
        LOG_DEBUG << log; // please don't use std::cout
    }
} // namespace utils

GVN::partitions
GVN::join(const partitions &P1, const partitions &P2, BasicBlock *lbb, BasicBlock *rbb) {
    // TODO: do intersection pair-wise
}

std::shared_ptr<CongruenceClass> GVN::intersect(const std::shared_ptr<CongruenceClass> &Ci,
                                                const std::shared_ptr<CongruenceClass> &Cj,
                                                BasicBlock *lbb,
                                                BasicBlock *rbb) {
    // TODO: do intersection
}

void GVN::detectEquivalences(llvm::ilist<GlobalVariable> *global_list) {
    auto top = std::set<shared_ptr<CongruenceClass>>();
    top.insert(createCongruenceClass(0));
    bool changed = false;
    // initialize pout with top
    for (auto &bb : func_->get_basic_blocks()) {
        pout_[&bb] = top;
    }
    // iterate until convergence
    BasicBlock *entry = func_->get_entry_block();
    pin_[entry]       = {};

    // TODO: you might need to do something here

    do {
        changed = false;

        for (auto &bb : func_->get_basic_blocks()) {
            GVN::partitions p = {};
            // TODO: compute p (pin of bb) according to predecessors of bb

            // iterate through all instructions in the block
            for (auto &instr : bb.get_instructions()) {
                p = transferFunction(&instr, &instr, p);
            }

            // check changes in pout
            if (p != pout_[&bb]) {
                changed = true;
            }
            pout_[&bb] = std::move(p);
        }
    } while (changed);
}

shared_ptr<Expression> GVN::valueExpr(const partitions &pout, Value *v) {
    // TODO: do something here for const propagation and other cases

    auto instr = dynamic_cast<Instruction *>(v);
    if (instr == nullptr or instr->is_void()) {
        return nullptr;
    }

    // TODO: create value expression for instr according to its type
    // Hint: you might need to use valueExpr recursively
    // Although TA's implementation use Expression as value expression, you can design your own
    return nullptr;
}

GVN::partitions GVN::transferFunction(Instruction *x, Value *e, const partitions &pin) {
    partitions pout = clone(pin);

    if (x->is_void()) {
        return pout;
    }

    // TODO: remove x from any cc which contains x

    auto ve  = valueExpr(pout, dynamic_cast<Instruction *>(e));
    auto vpf = valuePhiFunc(ve, pin);

    for (const auto &Ci : pout) {
        // TODO: if ve or vpf is in Ci, insert x into Ci
    }

    auto cc = createCongruenceClass(next_value_number_++);
    // TODO: you might need to do something here for const propagation

    cc->members_    = {x};
    cc->value_expr_ = ve;
    cc->value_phi_  = vpf;

    pout.insert(cc);

    return pout;
}

shared_ptr<Expression> GVN::valuePhiFunc(const shared_ptr<Expression> &ve, const partitions &P) {
    auto binary_ve = std::dynamic_pointer_cast<BinaryExpression>(ve);
    if (binary_ve != nullptr and binary_ve->both_phi()) {
        // TODO: if ve is binary expression and both of its operands are phi expression, return phi
        // expression according to the algorithm
    }
    return nullptr;
}

shared_ptr<Expression> GVN::getVN(const partitions &pout, shared_ptr<Expression> ve) {
    // TODO
    return nullptr;
}

void GVN::initPerFunction() {
    next_value_number_ = 1;
    pin_.clear();
    pout_.clear();
}

void GVN::replace_cc_members() {
    for (auto &[_bb, part] : pout_) {
        auto *bb = _bb; // workaround: structured bindings can't be captured in C++17
        for (const auto &cc : part) {
            if (cc->index_ == 0) {
                continue;
            }
            // if you are planning to do constant propagation, leaders should be set to constant at
            // some point
            for (const auto &member : cc->members_) {
                bool member_is_phi = dynamic_cast<PhiInst *>(member) != nullptr;
                bool value_phi     = cc->value_phi_ != nullptr;
                if (member != cc->leader_ and (value_phi or !member_is_phi)) {
                    // only replace the members if users are in the same block as bb
                    member->replace_use_with_if(cc->leader_, [bb](Use *use) {
                        auto *user = use->val_;
                        if (auto *instr = dynamic_cast<Instruction *>(user)) {
                            auto *parent = instr->get_parent();
                            auto &bb_pre = parent->get_pre_basic_blocks();
                            if (instr->is_phi()) { // as copy stmt, the phi belongs to this block
                                return std::find(bb_pre.begin(), bb_pre.end(), bb) != bb_pre.end();
                            }
                            return parent == bb;
                        }
                        return false;
                    });
                }
            }
        }
    }
}

// top-level function, done for you
void GVN::run() {
    m_->set_print_name();
    std::ofstream gvn_json;
    if (dump_json_) {
        gvn_json.open("gvn.json", std::ios::out);
        gvn_json << "[";
    }

    folder_    = std::make_unique<ConstFolder>(m_);
    func_info_ = std::make_unique<FuncInfo>(m_);
    func_info_->run();
    dce_ = std::make_unique<DeadCode>(m_);
    dce_->run(); // let dce take care of some dead phis with undef

    for (auto &f : m_->get_functions()) {
        if (f.get_basic_blocks().empty()) {
            continue;
        }
        func_ = &f;
        initPerFunction();
        LOG_INFO << "Processing " << f.get_name();
        detectEquivalences(&m_->get_global_variable());
        LOG_INFO << "===============pin=========================\n";
        for (auto &[bb, part] : pin_) {
            LOG_INFO << "\n===============bb: " << bb->get_name()
                     << "=========================\npartitionIn: ";
            for (const auto &cc : part) {
                LOG_DEBUG << f.get_name();
                LOG_INFO << utils::print_congruence_class(*cc);
            }
        }
        LOG_INFO << "\n===============pout=========================\n";
        for (auto &[bb, part] : pout_) {
            LOG_INFO << "\n=====bb: " << bb->get_name() << "=====\npartitionOut: ";
            for (const auto &cc : part) {
                LOG_DEBUG << f.get_name();
                LOG_INFO << utils::print_congruence_class(*cc);
            }
        }
        if (dump_json_) {
            gvn_json << "{\n\"function\": ";
            gvn_json << "\"" << f.get_name() << "\", ";
            gvn_json << "\n\"pout\": " << utils::dump_bb2partition(pout_);
            gvn_json << "},";
        }
        replace_cc_members(); // don't delete instructions, just replace them
    }
    dce_->run(); // let dce do that for us
    if (dump_json_) {
        gvn_json << "]";
    }
}

template<typename T>
static bool equiv_as(const Expression &lhs, const Expression &rhs) {
    // we use static_cast because we are very sure that both operands are actually T, not other
    // types.
    return static_cast<const T *>(&lhs)->equiv(static_cast<const T *>(&rhs));
}

bool GVNExpression::operator==(const Expression &lhs, const Expression &rhs) {
    if (lhs.get_expr_type() != rhs.get_expr_type()) {
        return false;
    }
    switch (lhs.get_expr_type()) {
    case Expression::e_bin:
        return equiv_as<BinaryExpression>(lhs, rhs);
    // TODO: add other cases here
    default:
        return false;
    }
}

bool GVNExpression::operator==(const shared_ptr<Expression> &lhs,
                               const shared_ptr<Expression> &rhs) {
    return lhs and rhs and *lhs == *rhs;
}

GVN::partitions GVN::clone(const partitions &p) {
    partitions data;
    for (const auto &cc : p) {
        data.insert(std::make_shared<CongruenceClass>(*cc));
    }
    return data;
}

bool operator==(const GVN::partitions &p1, const GVN::partitions &p2) {
    if (p1.size() != p2.size()) {
        return false;
    }
    for (auto it1 = p1.begin(), it2 = p2.begin(); it1 != p1.end(); ++it1, ++it2) {
        if (!(**it1 == **it2)) {
            return false;
        }
    }
    return true;
}

bool CongruenceClass::operator==(const CongruenceClass &other) const {
    // TODO: you might need to change this function to fit your implementation
    return std::tie(leader_, members_) == std::tie(other.leader_, other.members_);
}
