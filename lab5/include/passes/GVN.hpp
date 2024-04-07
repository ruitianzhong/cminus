#pragma once
#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "DeadCode.hpp"
#include "FuncInfo.hpp"
#include "Function.hpp"
#include "IRprinter.hpp"
#include "Instruction.hpp"
#include "Module.hpp"
#include "PassManager.hpp"
#include "Value.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GVNExpression {

    // fold the constant value 常量折叠
    class ConstFolder {
    public:
        explicit ConstFolder(Module *m) : module_(m) {}
        Constant *compute(Instruction *instr, Constant *value1, Constant *value2);
        Constant *compute(Instruction *instr, Constant *value1);

    private:
        Module *module_;
    };

    /**
     * for constructor of class derived from `Expression`, we make it public
     * because `std::make_shared` needs the constructor to be publicly available,
     * but you should call the static factory method `create` instead the constructor itself to get
     * the desired data
     */
    class Expression {
    public:
        // TODO: you need to extend expression types according to testcases
        enum gvn_expr_t {
            e_bin,
        };
        explicit Expression(gvn_expr_t t) : expr_type(t) {}
        virtual ~Expression()       = default;
        virtual std::string print() = 0;
        [[nodiscard]] gvn_expr_t get_expr_type() const {
            return expr_type;
        }

    private:
        gvn_expr_t expr_type;
    };

    bool operator==(const std::shared_ptr<Expression> &lhs, const std::shared_ptr<Expression> &rhs);
    bool operator==(const GVNExpression::Expression &lhs, const GVNExpression::Expression &rhs);

    // arithmetic expression
    class BinaryExpression : public Expression {
    public:
        static std::shared_ptr<BinaryExpression> create(Instruction::OpID op,
                                                        std::shared_ptr<Expression> lhs,
                                                        std::shared_ptr<Expression> rhs) {
            return std::make_shared<BinaryExpression>(op, lhs, rhs);
        }
        std::string print() override {
            return "(" + print_instr_op_name(op_) + " " + lhs_->print() + " " + rhs_->print() + ")";
        }

        bool equiv(const BinaryExpression *other) const {
            return op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_;
        }

        BinaryExpression(Instruction::OpID op,
                         std::shared_ptr<Expression> lhs,
                         std::shared_ptr<Expression> rhs)
            : Expression(e_bin), op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

        bool both_phi() {
            // TODO: determine whether both operands are phi functions
            return false;
        }

        std::shared_ptr<Expression> get_lhs() {
            return lhs_;
        }

        std::shared_ptr<Expression> get_rhs() {
            return rhs_;
        }

        Instruction::OpID get_op() {
            return op_;
        }

    private:
        Instruction::OpID op_;
        std::shared_ptr<Expression> lhs_, rhs_;
    };

    // TODO: add other expression subclasses here
} // namespace GVNExpression

/**
 * Congruence class in each partitions
 * Note: for constant propagation, you might need to add other fields
 */
struct CongruenceClass {
    size_t index_;
    // representative of the congruence class, used to replace all the members (except itself) when
    // analysis is done
    Value *leader_{};
    // value expression in congruence class
    // Note: type of value_expr_ and value_phi_ is shared_ptr<Expression>, which correspond to the
    // return type of valueExpr and valuePhiFunc function, if you want to design your own value
    // expression, you need to modify the type of value_expr_, value_phi_ and the return type of
    // valueExpr and valuePhiFunc
    std::shared_ptr<GVNExpression::Expression> value_expr_;
    // value φ-function is an annotation of the congruence class
    std::shared_ptr<GVNExpression::Expression> value_phi_;
    // equivalent variables in one congruence class
    std::set<Value *> members_;

    explicit CongruenceClass(size_t index) : index_(index) {}

    bool operator<(const CongruenceClass &other) const {
        return this->index_ < other.index_;
    }
    bool operator==(const CongruenceClass &other) const;
};

namespace std {
    template<>
    // overload std::less for std::shared_ptr<CongruenceClass>, i.e. how to sort the congruence
    // classes
    struct less<std::shared_ptr<CongruenceClass>> {
        bool operator()(const std::shared_ptr<CongruenceClass> &a,
                        const std::shared_ptr<CongruenceClass> &b) const {
            // nullptrs should never appear in partitions, so we just dereference it
            return *a < *b;
        }
    };
} // namespace std

class GVN : public Pass {
public:
    using partitions = std::set<std::shared_ptr<CongruenceClass>>;
    static bool isTop(const partitions &p) {
        return (*p.begin())->index_ == 0;
    }
    GVN(Module *m, bool dump_json) : Pass(m), dump_json_(dump_json) {}
    // pass start
    void run() override;
    // init for pass metadata;
    void initPerFunction();

    // fill the following functions according to Pseudocode, **you might need to add more
    // arguments**
    void detectEquivalences(llvm::ilist<GlobalVariable> *global_list);
    partitions join(const partitions &P1, const partitions &P2, BasicBlock *lbb, BasicBlock *rbb);
    std::shared_ptr<CongruenceClass> intersect(const std::shared_ptr<CongruenceClass> &,
                                               const std::shared_ptr<CongruenceClass> &,
                                               BasicBlock *lbb,
                                               BasicBlock *rbb);
    partitions transferFunction(Instruction *x, Value *e, const partitions &pin);
    std::shared_ptr<GVNExpression::Expression>
    valuePhiFunc(const std::shared_ptr<GVNExpression::Expression> &, const partitions &);
    std::shared_ptr<GVNExpression::Expression> valueExpr(const partitions &pout, Value *v);

    static std::shared_ptr<GVNExpression::Expression>
    getVN(const partitions &pout, std::shared_ptr<GVNExpression::Expression> ve);

    // replace cc members with leader
    void replace_cc_members();

    // note: be careful when to use copy constructor or clone
    static partitions clone(const partitions &p);

    // create congruence class helper
    static std::shared_ptr<CongruenceClass> createCongruenceClass(size_t index = 0) {
        return std::make_shared<CongruenceClass>(index);
    }

private:
    bool dump_json_;
    std::uint64_t next_value_number_ = 1;
    Function *func_{};
    std::map<BasicBlock *, partitions> pin_, pout_;
    std::unique_ptr<FuncInfo> func_info_;
    std::unique_ptr<GVNExpression::ConstFolder> folder_;
    std::unique_ptr<DeadCode> dce_;
};

bool operator==(const GVN::partitions &p1, const GVN::partitions &p2);
