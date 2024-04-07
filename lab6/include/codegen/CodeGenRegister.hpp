#pragma once

#include "ASMInstruction.hpp"
#include "Module.hpp"
#include "Register.hpp"
#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "Function.hpp"
#include "IRprinter.hpp"
#include "Instruction.hpp"
#include "Module.hpp"
#include "Value.hpp"
#include "liverange.hpp"
#include "regalloc.hpp"
#include <functional>

// #define STACK_ALIGN(x) (((x / 16) + (x % 16 ? 1 : 0)) * 16)
#define STACK_ALIGN(x) ALIGN(x, 16)
#define CONST_0 ConstantInt::get(0, m)
#define FP "$fp"
#define SP "$sp"
#define RA_reg "$ra"
// #a = 8, #t = 9, reserve $t0, $t1 for temporary
#define R_USABLE (17 - 2)
// #fa = 8, #ft=16, reserve $ft0, $ft1 for temporary
#define FR_USABLE (24 - 2)
#define ARG_R 8

class CodeGenRegister {
  public:
    explicit CodeGenRegister(Module *module) 
        : m(module), LRA(module, phi_map), RA_int(R_USABLE, false), RA_float(FR_USABLE, true) {}

    std::string print() const;

    void run();

    template <class... Args> void append_inst(Args... arg) {
        output.emplace_back(arg...);
    }

    void
    append_inst(const char *inst, std::initializer_list<std::string> args,
                ASMInstruction::InstType ty = ASMInstruction::Instruction) {
        auto content = std::string(inst) + " ";
        for (const auto &arg : args) {
            content += arg + ", ";
        }
        content.pop_back();
        content.pop_back();
        output.emplace_back(content, ty);
    }

  private:
    //TODO: 添加你需要的函数
    void getPhiMap();
    __attribute__((warn_unused_result)) string value2reg(Value *, int i = 0, string = "");
    void copy_stmt(); 
    void pass_arguments(CallInst *);
    void makeSureInRange(string instr_ir,
                         string reg1,
                         string reg2,
                         int imm,
                         string tinstr,
                         int tid = 0,
                         int bits = 12,
                         bool u = false){
        /* this function will tranfser
            * `addi.d $a0, $fp, imm` to `add.d $a0, $fp, tmp_ireg` if imm
            * overfloats for `addi.d`. During the time we move `imm` to `tmp_ireg`,
            * another tmp ireg will be used, they can be same, but we must specify
            * it.
            */
        auto treg = tmpregname(tid, false);
        assert(treg != reg2 && "it's possible to write tid before reg2's use");
        auto [l, h] = immRange(bits, u);
        if (l <= imm and imm <= h){
            append_inst(instr_ir.c_str(),{reg1,reg2,to_string(imm)});
        } else {
            assert(value2reg(ConstantInt::get(imm, m), tid, treg) == treg);
            append_inst(tinstr.c_str(),{reg1,reg2,treg});
        }
    }

    //进行copy操作
    bool gencopy(Value *lhs, string rhs_reg);
    void gencopy(string lhs_reg, string rhs_reg, bool is_float);
    //处理指针情况
    void ptrContent2reg(Value *, string);
    pair<string, bool> getRegName(Value *, int = 0) const;
    string bool2branch(Instruction *);

    // 向寄存器中装载数据
    void load_to_greg(Value *, const Reg &);
    void load_to_freg(Value *, const FReg &);
    void load_from_stack_to_greg(Value *, const Reg &);

    // 向寄存器中加载立即数
    void load_large_int32(int32_t, const Reg &);
    void load_large_int64(int64_t, const Reg &);
    void load_float_imm(float, const FReg &);

    // 将寄存器中的数据保存回栈上
    void store_from_greg(Value *, const Reg &);
    void store_from_freg(Value *, const FReg &);

    void allocate();
    void gen_prologue();
    void gen_ret();
    void gen_br();
    void gen_binary();
    void gen_alloca();
    void gen_load();
    void gen_store();
    void gen_icmp();
    void gen_fcmp();
    void gen_zext();
    void gen_call();
    void gen_gep();
    void gen_sitofp();
    void gen_fptosi();
    void gen_epilogue();

    static std::string label_name(BasicBlock *bb) {
        return "." + bb->get_parent()->get_name() + "_" + bb->get_name();
    }

    static std::string func_exit_label_name(Function *func) {
        return func->get_name() + "_exit";
    }

    static std::string fcmp_label_name(BasicBlock *bb, unsigned cnt) {
        return label_name(bb) + "_fcmp_" + std::to_string(cnt);
    }

    static string regname(uint i, bool is_float) {
        string name;
        if (is_float) {
            // assert(false && "not implemented!");
            if (1 <= i and i <= 8)
                name = "$fa" + to_string(i - 1);
            else if (9 <= i and i <= FR_USABLE)
                name = "$ft" + to_string(i - 9 + 2);
            else
                name = "WRONG_REG_" + to_string(i);
        } else {
            if (1 <= i and i <= 8)
                name = "$a" + to_string(i - 1);
            else if (9 <= i and i <= R_USABLE)
                name = "$t" + to_string(i - 9 + 2);
            else
                name = "WRONG_REG_" + to_string(i);
        }
        return name;
    }


    string tmpregname(int i, bool is_float) const {
        assert(i == 0 or i == 1);
        return (is_float ? "$ft" : "$t") + to_string(i);
    }

    static pair<int, int> immRange(int bit, bool u) {
        pair<int, int> res;
        if (u) {
            res.first = 0;
            res.second = (1 << bit) - 1;
        } else {
            bit--;
            res.first = -(1 << bit);
            res.second = (1 << bit) - 1;
        }

        return res;
    };

    static int typeLen(Type *type) {
        if (type->is_float_type())
            return 4;
        else if (type->is_integer_type()) {
            if (static_cast<IntegerType *>(type)->get_num_bits() == 32)
                return 4;
            else
                return 1;
        } else if (type->is_pointer_type())
            return 8;
        else if (type->is_array_type()) {
            auto arr_tp = static_cast<ArrayType *>(type);
            int n = arr_tp->get_num_of_elements();
            return n * typeLen(arr_tp->get_element_type());
        } else {
            assert(false && "unexpected case while computing type-length");
        }
    }

    static string suffix(Type *type) {
        int len = typeLen(type);
        switch (len) {
        case 1: return ".b";
        case 2: return ".h";
        case 4: return type->is_float_type() ? ".s" : ".w";
        case 8: return ".d";
        }
        assert(false && "no such suffix");
    }

    bool no_stack_alloca(Instruction *instr) const {
        if (instr->is_void())
            return true;
        if (instr->is_fcmp() or instr->is_cmp() or instr->is_zext())
            return true;
        auto regmap = (instr->get_type()->is_float_type() ? RA_float.get() : RA_int.get());
        if (regmap.find(instr) != regmap.end())
            return true;

        return false;
    }

    string label_in_assem(BasicBlock *bb) const { return (context.func)->get_name() + bb->get_name().substr(5); }

    struct {
        /* 随着ir遍历设置 */
        Function *func{nullptr};    // 当前函数
        BasicBlock *bb{nullptr};    // 当前基本块
        Instruction *inst{nullptr}; // 当前指令
        /* 在allocate()中设置 */
        unsigned frame_size{0}; // 当前函数的栈帧大小
        std::unordered_map<Value *, int> offset_map{}; // 指针相对 fp 的偏移
        unsigned fcmp_cnt{0}; // fcmp 的计数器, 用于创建 fcmp 需要的 label

        void clear() {
            func = nullptr;
            bb = nullptr;
            inst = nullptr;
            frame_size = 0;
            fcmp_cnt = 0;
            offset_map.clear();
        }

    } context;

    Module *m;
    std::list<ASMInstruction> output;

    LRA::LiveRangeAnalyzer LRA;
    LRA::LVITS LVITS_int, LVITS_float;
    RA::RegAllocator RA_int, RA_float;

    std::map<BasicBlock *, std::vector<std::pair<Value *, Value *>>> phi_map;
    std::map<Constant *, std::string> ROdata;

    //TODO:添加你需要的变量
};
//TODO:本次实验为开放性实验，你可以自行设计实验框架并自行对提供的实验框架进行修改。本框架只作为参考，不要让它束缚住你的设计思路。
//TODO: 对框架不满可尽情修改