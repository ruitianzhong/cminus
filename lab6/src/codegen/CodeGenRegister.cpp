#include "CodeGenRegister.hpp"

#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "Function.hpp"
#include "GlobalVariable.hpp"
#include "Instruction.hpp"
#include "Type.hpp"
#include "Value.hpp"
#include "ast.hpp"
#include "logging.hpp"
#include "regalloc.hpp"
#include "CodeGenUtil.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

void CodeGenRegister::getPhiMap() {
    phi_map.clear();
    //TODO:对phi函数进行处理，为phi_map赋值
}

string CodeGenRegister::value2reg(Value *v, int i, string recommend) {
    bool is_float = v->get_type()->is_float_type();
    auto tmp_ireg = Reg::t(i);
    auto [reg_name, find] = getRegName(v, i);
    if (find)
        return reg_name;
    else if (recommend != "")
        reg_name = recommend;
    // now is the stack allocation case
    if (dynamic_cast<Constant *>(v)) {
        if (v == CONST_0)
            return "$zero";
        //TODO:处理constant情况，注意一些特殊情况
        auto constant = static_cast<Constant *>(v);
        if (dynamic_cast<ConstantInt *>(constant))
            append_inst(LOAD WORD,{reg_name,tmp_ireg.print(),"0"});
        else if (dynamic_cast<ConstantFP *>(constant))
            append_inst(FLOAD SINGLE,{reg_name,tmp_ireg.print(),"0"});
        else
            assert(false && "wait for completion");
    } else if (dynamic_cast<GlobalVariable *>(v)) {
        append_inst(LOAD_ADDR,{reg_name,v->get_name()});
    } else if (dynamic_cast<AllocaInst *>(v)) {
        makeSureInRange("addi.d", reg_name, FP, context.offset_map.at(v), "add.d", i);
    } else if (dynamic_cast<Argument *>(v)) {
        int id = 1;        
        for(auto &arg : context.func->get_args()){
            if((&arg) == v)
                break;
            id++;
        }
        if (id <= ARG_R)
            return regname(ARG_R, is_float);
        else {
            //TODO: 当传入参数大于8的时候该如何处理
        }
    } else {
        string instr_ir = is_float ? "fld" : "ld";
        auto suff = suffix(v->get_type());
        makeSureInRange(instr_ir + suff, reg_name, FP, context.offset_map.at(v), instr_ir + "x" + suff, i);
    }
    return reg_name;
}

void CodeGenRegister::copy_stmt() {
    // all the phi instruction is transformed to copy-stmt
    for (auto &copy : phi_map[context.bb]) {
        append_inst("# " + print_as_op(copy.first, false) + " = " + print_as_op(copy.second, false));
        //TODO:相比于栈式分配的差别?
    }
}

void CodeGenRegister::pass_arguments(CallInst *instr) {
    /*TODO: 将参数正确传递，注意成环情况。你可以使用临时寄存器来暂存参数内容。
    你也可以自行设计这一部分(gen_call + pass_arguments)来正确完成对callinst的处理*/
    const int N = 8;
    auto func = static_cast<Function *>(instr->get_operand(0));

}

void CodeGenRegister::ptrContent2reg(Value *ptr, string dest_reg) {
    auto ele_tp = ptr->get_type()->get_pointer_element_type();
    assert(ele_tp);
    bool is_float = ele_tp->is_float_type();
    string instr_ir = (is_float ? "fld" : "ld");
    string suff = suffix(ele_tp);

    auto [addr_reg, find] = getRegName(ptr, 0);

    //TODO:如何处理剩下两种情况?
    if (dynamic_cast<GlobalVariable *>(ptr)) {

    } else if (dynamic_cast<AllocaInst *>(ptr)) {
        makeSureInRange(instr_ir + suff, dest_reg, FP, context.offset_map.at(ptr), instr_ir + "x" + suff);
    } else if (dynamic_cast<GetElementPtrInst *>(ptr)) {

    } else
        assert(false && "unknown type");
}

bool CodeGenRegister::gencopy(Value *lhs, string rhs_reg) {
    auto [lhs_reg, find] = getRegName(lhs);
    if (not find) {
        //TODO:此时该怎么做?
        return false;
    }
    auto is_float = lhs->get_type()->is_float_type();
    gencopy(lhs_reg, rhs_reg, is_float);
    return true;
}

void CodeGenRegister::gencopy(string lhs_reg, string rhs_reg, bool is_float) {
    if (rhs_reg != lhs_reg) {
        if (is_float)
            append_inst("fmov.s " + lhs_reg + ", " + rhs_reg);
        else
            append_inst("or " + lhs_reg + ", $zero, " + rhs_reg);
    }
}

pair<string, bool> CodeGenRegister::getRegName(Value *v, int i) const {
    assert(i == 0 or i == 1);
    bool find;
    string name;
    //TODO: 寻找v对应的寄存器名称，你可能会用到regalloc.hpp中的类
    return {name, find};
}

string CodeGenRegister::bool2branch(Instruction *cond) {
    assert(cond->get_type() == m->get_int1_type());
    auto icmp_instr = dynamic_cast<ICmpInst *>(cond);
    auto fcmp_instr = dynamic_cast<FCmpInst *>(cond);
    assert(icmp_instr or fcmp_instr);
    //TODO: 处理eq、ne、gt、ge、lt、le情况
}

void CodeGenRegister::allocate() {
    // 备份 $ra $fp
    unsigned offset = PROLOGUE_OFFSET_BASE;

    for (auto &bb : (context.func)->get_basic_blocks()){
        for (auto &instr : bb.get_instructions()) {
            //TODO:相比于栈式分配，它需要做出哪些修改?
        }        
    }

    // 分配栈空间，需要是 16 的整数倍
    context.frame_size = ALIGN(offset, PROLOGUE_ALIGN);
}

void CodeGenRegister::gen_prologue() {
    makeSureInRange("addi.d", SP, SP, -context.frame_size, "add.d");
    makeSureInRange("st.d", RA_reg, SP, context.frame_size - 8, "stx.d");
    makeSureInRange("st.d", FP, SP, context.frame_size - 16, "stx.d");
    makeSureInRange("addi.d", FP, SP, context.frame_size, "add.d");
}

void CodeGenRegister::gen_epilogue() {
    append_inst(func_exit_label_name(context.func), ASMInstruction::Label);
    output.emplace_back("# epilog");

    makeSureInRange("ld.d", RA_reg, SP, context.frame_size - 8, "ldx.d");
    makeSureInRange("ld.d", FP, SP, context.frame_size - 16, "ldx.d");
    makeSureInRange("addi.d", SP, SP, context.frame_size, "add.d");
    append_inst("jr $ra");
}

void CodeGenRegister::load_to_greg(Value *val, const Reg &reg) {
    assert(val->get_type()->is_integer_type() ||
           val->get_type()->is_pointer_type());

    if (auto *constant = dynamic_cast<ConstantInt *>(val)) {
        int32_t val = constant->get_value();
        if (IS_IMM_12(val)) {
            append_inst(ADDI WORD, {reg.print(), "$zero", std::to_string(val)});
        } else {
            load_large_int32(val, reg);
        }
    } else if (auto *global = dynamic_cast<GlobalVariable *>(val)) {
        append_inst(LOAD_ADDR, {reg.print(), global->get_name()});
    } else {
        load_from_stack_to_greg(val, reg);
    }
}

void CodeGenRegister::load_large_int32(int32_t val, const Reg &reg) {
    int32_t high_20 = val >> 12; // si20
    uint32_t low_12 = val & LOW_12_MASK;
    append_inst(LU12I_W, {reg.print(), std::to_string(high_20)});
    append_inst(ORI, {reg.print(), reg.print(), std::to_string(low_12)});
}

void CodeGenRegister::load_large_int64(int64_t val, const Reg &reg) {
    auto low_32 = static_cast<int32_t>(val & LOW_32_MASK);
    load_large_int32(low_32, reg);

    auto high_32 = static_cast<int32_t>(val >> 32);
    int32_t high_32_low_20 = (high_32 << 12) >> 12; // si20
    int32_t high_32_high_12 = high_32 >> 20;        // si12
    append_inst(LU32I_D, {reg.print(), std::to_string(high_32_low_20)});
    append_inst(LU52I_D,
                {reg.print(), reg.print(), std::to_string(high_32_high_12)});
}

void CodeGenRegister::load_from_stack_to_greg(Value *val, const Reg &reg) {
    auto offset = context.offset_map.at(val);
    auto offset_str = std::to_string(offset);
    auto *type = val->get_type();
    if (IS_IMM_12(offset)) {
        if (type->is_int1_type()) {
            append_inst(LOAD BYTE, {reg.print(), "$fp", offset_str});
        } else if (type->is_int32_type()) {
            append_inst(LOAD WORD, {reg.print(), "$fp", offset_str});
        } else { // Pointer
            append_inst(LOAD DOUBLE, {reg.print(), "$fp", offset_str});
        }
    } else {
        load_large_int64(offset, reg);
        append_inst(ADD DOUBLE, {reg.print(), "$fp", reg.print()});
        if (type->is_int1_type()) {
            append_inst(LOAD BYTE, {reg.print(), reg.print(), "0"});
        } else if (type->is_int32_type()) {
            append_inst(LOAD WORD, {reg.print(), reg.print(), "0"});
        } else { // Pointer
            append_inst(LOAD DOUBLE, {reg.print(), reg.print(), "0"});
        }
    }
}

void CodeGenRegister::store_from_greg(Value *val, const Reg &reg) {
    auto offset = context.offset_map.at(val);
    auto offset_str = std::to_string(offset);
    auto *type = val->get_type();
    if (IS_IMM_12(offset)) {
        if (type->is_int1_type()) {
            append_inst(STORE BYTE, {reg.print(), "$fp", offset_str});
        } else if (type->is_int32_type()) {
            append_inst(STORE WORD, {reg.print(), "$fp", offset_str});
        } else { // Pointer
            append_inst(STORE DOUBLE, {reg.print(), "$fp", offset_str});
        }
    } else {
        auto addr = Reg::t(8);
        load_large_int64(offset, addr);
        append_inst(ADD DOUBLE, {addr.print(), "$fp", addr.print()});
        if (type->is_int1_type()) {
            append_inst(STORE BYTE, {reg.print(), addr.print(), "0"});
        } else if (type->is_int32_type()) {
            append_inst(STORE WORD, {reg.print(), addr.print(), "0"});
        } else { // Pointer
            append_inst(STORE DOUBLE, {reg.print(), addr.print(), "0"});
        }
    }
}

void CodeGenRegister::load_to_freg(Value *val, const FReg &freg) {
    assert(val->get_type()->is_float_type());
    if (auto *constant = dynamic_cast<ConstantFP *>(val)) {
        float val = constant->get_value();
        load_float_imm(val, freg);
    } else {
        auto offset = context.offset_map.at(val);
        auto offset_str = std::to_string(offset);
        if (IS_IMM_12(offset)) {
            append_inst(FLOAD SINGLE, {freg.print(), "$fp", offset_str});
        } else {
            auto addr = Reg::t(8);
            load_large_int64(offset, addr);
            append_inst(ADD DOUBLE, {addr.print(), "$fp", addr.print()});
            append_inst(FLOAD SINGLE, {freg.print(), addr.print(), "0"});
        }
    }
}

void CodeGenRegister::load_float_imm(float val, const FReg &r) {
    int32_t bytes = *reinterpret_cast<int32_t *>(&val);
    load_large_int32(bytes, Reg::t(8));
    append_inst(GR2FR WORD, {r.print(), Reg::t(8).print()});
}

void CodeGenRegister::store_from_freg(Value *val, const FReg &r) {
    auto offset = context.offset_map.at(val);
    if (IS_IMM_12(offset)) {
        auto offset_str = std::to_string(offset);
        append_inst(FSTORE SINGLE, {r.print(), "$fp", offset_str});
    } else {
        auto addr = Reg::t(8);
        load_large_int64(offset, addr);
        append_inst(ADD DOUBLE, {addr.print(), "$fp", addr.print()});
        append_inst(FSTORE SINGLE, {r.print(), addr.print(), "0"});
    }
}

void CodeGenRegister::gen_ret() {
    auto return_inst = static_cast<ReturnInst *>(context.inst);
    if (not return_inst->is_void_ret()) {
        auto value = return_inst->get_operand(0);
        auto is_float = value->get_type()->is_float_type();
        auto reg = value2reg(value);
        if (is_float and reg != "$fa0")
            append_inst("fmov.s $fa0, " + reg);
        else if (not is_float and reg != "$a0")
            append_inst("or $a0, $zero, " + reg);
    } else {
        append_inst("addi.w $a0, $zero, 0");        
    }

    append_inst("b " + func_exit_label_name(context.func));
}

void CodeGenRegister::gen_br() {
    auto branch_inst = static_cast<BranchInst *>(context.inst);
    if (branch_inst->is_cond_br()) {
        //TODO: 什么情况下栈式分配和寄存器分配有区别?
    } else {
        auto bb = static_cast<BasicBlock *>(context.inst->get_operand(0));
        append_inst("b " + label_name(bb));
    }
}

void CodeGenRegister::gen_binary() {
    //TODO:处理二元运算符情况，注意与栈式分配的差别    
}

void CodeGenRegister::gen_alloca() {
    //使用寄存器分配，这里不需要做处理
}

void CodeGenRegister::gen_load() {
    auto load_inst = static_cast<LoadInst *>(context.inst);
    auto [reg, find] = getRegName(context.inst);
    ptrContent2reg(load_inst->get_lval(), reg);
    gencopy(context.inst, reg);
}

void CodeGenRegister::gen_store() {
    auto store_inst = static_cast<StoreInst *>(context.inst);
    //TODO: 相比栈式分配有什么区别?
    bool is_float = store_inst->get_rval()->get_type()->is_float_type();
    string instr_ir = (is_float ? "fst" : "st");
    string suff = suffix(store_inst->get_rval()->get_type());

}

void CodeGenRegister::gen_icmp() {
    //使用寄存器分配，特殊处理了Phi, 只需用到br
}

void CodeGenRegister::gen_fcmp() {
    //使用寄存器分配，特殊处理了Phi, 只需用到br
}

void CodeGenRegister::gen_zext() {
    if (RA::RegAllocator::no_reg_alloca(context.inst))
        return;
    assert(context.inst->get_num_operand() == 1);
    auto cmp_instr = context.inst->get_operand(0);
    auto icmp_instr = dynamic_cast<ICmpInst *>(cmp_instr);
    auto fcmp_instr = dynamic_cast<FCmpInst *>(cmp_instr);
    assert(icmp_instr or fcmp_instr);

    auto [dest_reg, _] = getRegName(context.inst);

    //TODO: 处理eq、ne、gt、ge、lt、le情况

    gencopy(context.inst, dest_reg);
}

void CodeGenRegister::gen_call() {

    auto call_inst = static_cast<CallInst *>(context.inst);
    auto func = static_cast<Function *>(context.inst->get_operand(0));

    // TODO: 判断是否有寄存器中的内容需要存入栈中

    int storeN = 0;
    vector<std::tuple<Value *, string, int>> store_record;
    for (auto [op, interval] : LRA.get_interval_map()) {                                  
        if (RA::RegAllocator::no_reg_alloca(op))
            continue;
        auto [name, find] = getRegName(op);
        if (not find)
            continue;
        auto op_type = op->get_type();
        auto op_is_float = op_type->is_float_type();
        if (not call_inst->get_function_type()->get_return_type()->is_void_type()) {

        }
        bool restore = false;

    }

    //TODO: 记得处理跳转到的函数所需的额外栈空间totalN
    int totalN;
    string instr_ir, suff, v_reg;

    for (auto [op, reg, off] : store_record) {
        instr_ir = (op->get_type()->is_float_type() ? "fst" : "st");
        suff = suffix(op->get_type());
        makeSureInRange(instr_ir + suff, reg, SP, totalN - off, instr_ir + "x" + suff);
    }
    // pass arguments
    pass_arguments(call_inst);
    append_inst("bl " + func->get_name());
    gencopy(call_inst, call_inst->get_type()->is_float_type() ? "$fa0" : "$a0");

    for (auto [op, reg, off] : store_record) {
        instr_ir = (op->get_type()->is_float_type() ? "fld" : "ld");
        suff = suffix(op->get_type());
        makeSureInRange(instr_ir + suff, reg, SP, totalN - off, instr_ir + "x" + suff);
    }

}

/*
 * %op = getelementptr [10 x i32], [10 x i32]* %op, i32 0, i32 %op
 * %op = getelementptr        i32,        i32* %op, i32 %op
 *
 * Memory layout
 *       -            ^
 * +-----------+      | Smaller address
 * |  arg ptr  |---+  |
 * +-----------+   |  |
 * |           |   |  |
 * +-----------+   /  |
 * |           |<--   |
 * |           |   \  |
 * |           |   |  |
 * |   Array   |   |  |
 * |           |   |  |
 * |           |   |  |
 * |           |   |  |
 * +-----------+   |  |
 * |  Pointer  |---+  |
 * +-----------+      |
 * |           |      |
 * +-----------+      |
 * |           |      |
 * +-----------+      |
 * |           |      |
 * +-----------+      | Larger address
 *       +
 */
void CodeGenRegister::gen_gep() {

    assert(context.inst->get_num_operand() <= 3);
    auto [dest_reg, _] = getRegName(context.inst);

    auto off = context.inst->get_operand(context.inst->get_num_operand() == 3 ? 2 : 1);
    if (off == CONST_0) {
        auto final_reg = value2reg(context.inst->get_operand(0), 0, dest_reg);
        gencopy(context.inst, final_reg);
        return;
    }
    if (context.inst->get_num_operand() == 3) {
        assert(context.inst->get_operand(1) == CONST_0);
    } 
    //TODO: 为addr_reg和off_reg赋上合适的值
    std::string addr_reg;
    std::string off_reg;
    std::string tmp_reg = "$t1";

    append_inst("slli.d " + tmp_reg + ", " + off_reg + ", 2");
    append_inst("add.d " + dest_reg + ", " + addr_reg + ", " + tmp_reg);

    gencopy(context.inst, dest_reg);
}

void CodeGenRegister::gen_sitofp() {

    auto sitofp_inst = static_cast<SiToFpInst *>(context.inst);
    assert(context.inst->get_operand(0)->get_type() == m->get_int32_type());
    assert(sitofp_inst->get_dest_type() == m->get_float_type());
    string i_reg = value2reg(context.inst->get_operand(0));
    auto [f_reg, _] = getRegName(context.inst);
    append_inst("movgr2fr.w " + f_reg + ", " + i_reg);
    append_inst("ffint.s.w " + f_reg + ", " + f_reg);
    gencopy(context.inst, f_reg);
}

void CodeGenRegister::gen_fptosi() {

    auto fptosi_inst = static_cast<FpToSiInst *>(context.inst);
    assert(context.inst->get_operand(0)->get_type() == m->get_float_type());
    assert(fptosi_inst->get_dest_type() == m->get_int32_type());
    string f_reg = value2reg(context.inst->get_operand(0));
    string f_treg = "$ft0";
    auto [i_reg, _] = getRegName(context.inst);
    append_inst("ftintrz.w.s " + f_treg + ", " + f_reg);
    append_inst("movfr2gr.s " + i_reg + ", " + f_treg);
    gencopy(context.inst, i_reg);
}

void CodeGenRegister::run() {
    // 确保每个函数中基本块的名字都被设置好
    m->set_print_name();

    /* 使用 GNU 伪指令为全局变量分配空间
     * 你可以使用 `la.local` 指令将标签 (全局变量) 的地址载入寄存器中, 比如
     * 要将 `a` 的地址载入 $t0, 只需要 `la.local $t0, a`
     */
    if (!m->get_global_variable().empty()) {
        append_inst("Global variables", ASMInstruction::Comment);
        /* 虽然下面两条伪指令可以简化为一条 `.bss` 伪指令, 但是我们还是选择使用
         * `.section` 将全局变量放到可执行文件的 BSS 段, 原因如下:
         * - 尽可能对齐交叉编译器 loongarch64-unknown-linux-gnu-gcc 的行为
         * - 支持更旧版本的 GNU 汇编器, 因为 `.bss` 伪指令是应该相对较新的指令,
         *   GNU 汇编器在 2023 年 2 月的 2.37 版本才将其引入
         */
        append_inst(".text", ASMInstruction::Atrribute);
        append_inst(".section", {".bss", "\"aw\"", "@nobits"},
                    ASMInstruction::Atrribute);
        for (auto &global : m->get_global_variable()) {
            auto size =
                global.get_type()->get_pointer_element_type()->get_size();
            append_inst(".globl", {global.get_name()},
                        ASMInstruction::Atrribute);
            append_inst(".type", {global.get_name(), "@object"},
                        ASMInstruction::Atrribute);
            append_inst(".size", {global.get_name(), std::to_string(size)},
                        ASMInstruction::Atrribute);
            append_inst(global.get_name(), ASMInstruction::Label);
            append_inst(".space", {std::to_string(size)},
                        ASMInstruction::Atrribute);
        }
    }

    // 函数代码段
    getPhiMap();
    append_inst(".text", ASMInstruction::Atrribute);

    for (auto &func : m->get_functions()) {
        if (not func.is_declaration()) {
            LRA.run(&func);
            RA_int.LinearScan(LRA.get(), &func);
            RA_float.LinearScan(LRA.get(), &func);

            LOG_DEBUG << "integer register map for function: " << func.get_name() << "\n";
            RA_int.print([](int i) { return regname(i, false); });
            LOG_DEBUG << "float register map for function: " << func.get_name() << "\n";
            RA_float.print([](int i) { return regname(i, true); });

            for (auto [_, op] : LRA.get()) {
                auto regmap = op->get_type()->is_float_type() ? RA_float.get() : RA_int.get();
                if (regmap.find(op) == regmap.end())
                    LOG_DEBUG << "no reg belongs to " << op->get_name() << "\n";
            }

            // 更新 context
            context.clear();
            context.func = &func;

            // 函数信息
            append_inst(".globl", {func.get_name()}, ASMInstruction::Atrribute);
            append_inst(".type", {func.get_name(), "@function"},
                        ASMInstruction::Atrribute);
            append_inst(func.get_name(), ASMInstruction::Label);

            // 分配函数栈帧
            allocate();
            // 生成 prologue
            gen_prologue();
            for (auto &bb : func.get_basic_blocks()) {
                context.bb = &bb;
                append_inst(label_name(context.bb), ASMInstruction::Label);
                for (auto &instr : bb.get_instructions()) {
                    // For debug
                    append_inst(instr.print(), ASMInstruction::Comment);
                    context.inst = &instr; // 更新 context
                    switch (instr.get_instr_type()) {
                        case Instruction::ret:
                            copy_stmt();
                            gen_ret();
                            break;
                        case Instruction::br:
                            copy_stmt();
                            gen_br();
                            break;
                        case Instruction::add:
                        case Instruction::sub:
                        case Instruction::mul:
                        case Instruction::sdiv:
                        case Instruction::fadd:
                        case Instruction::fsub:
                        case Instruction::fmul:
                        case Instruction::fdiv:
                            gen_binary();
                            break;
                        case Instruction::alloca:
                            gen_alloca();
                            break;
                        case Instruction::load:
                            gen_load();
                            break;
                        case Instruction::store:
                            gen_store();
                            break;
                        case Instruction::ge:
                        case Instruction::gt:
                        case Instruction::le:
                        case Instruction::lt:
                        case Instruction::eq:
                        case Instruction::ne:
                            gen_icmp();
                            break;
                        case Instruction::fge:
                        case Instruction::fgt:
                        case Instruction::fle:
                        case Instruction::flt:
                        case Instruction::feq:
                        case Instruction::fne:
                            gen_fcmp();
                            break;
                        case Instruction::phi:
                            break;
                        case Instruction::call:
                            gen_call();
                            break;
                        case Instruction::getelementptr:
                            gen_gep();
                            break;
                        case Instruction::zext:
                            gen_zext();
                            break;
                        case Instruction::fptosi:
                            gen_fptosi();
                            break;
                        case Instruction::sitofp:
                            gen_sitofp();
                            break;
                    }
                }
            }
            // 生成 epilogue
            gen_epilogue();
        }
    }
    //TODO: 处理read only data
}

std::string CodeGenRegister::print() const {
    std::string result;
    for (const auto &inst : output) {
        result += inst.format();
    }
    return result;
}
//TODO: 对框架不满可尽情修改