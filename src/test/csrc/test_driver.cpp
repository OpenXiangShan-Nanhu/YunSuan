#include <stdio.h>
#include <cstdlib>
#include <stdint.h>
extern "C" {
#include <softfloat.h>
}
#include "VSimTop.h"
#include "verilated_vcd_c.h"
#include "include/vpu_constant.h"
#include "include/test_driver.h"

TestDriver::TestDriver():
  issued(false), verbose(false), keepinput(false)
{
  // aviod random value
  set_test_type();
  gen_next_test_case();
}

TestDriver::~TestDriver() {
}

void TestDriver::set_default_value(VSimTop *dut_ptr) {
  dut_ptr->io_in_valid = false;
  dut_ptr->io_out_ready = true;
}

void TestDriver::set_test_type() {
  test_type.pick_fuType = true;
  test_type.pick_fuOpType = false;
  test_type.fuType = VFloatAdder;
  test_type.fuOpType = VFADD;
  // printf("Set Test Type Res: fuType:%d fuOpType:%d\n", test_type.fuType, test_type.fuOpType);
}

void TestDriver::gen_next_test_case() {
  issued = false;
  get_random_input();
  if (verbose) { display_ref_input(); }
  get_expected_output();
  if (verbose) { display_ref_output(); }
}


uint8_t TestDriver::gen_random_futype(std::initializer_list<uint8_t> futype_list) {
  return *(futype_list.begin() + (rand() % futype_list.size()));
}

uint8_t TestDriver::gen_random_optype() {
  switch (input.fuType)
  {
    case VFloatAdder: {
      uint8_t vfadd_all_optype[VFA_NUM] = VFA_ALL_OPTYPES;
      return vfadd_all_optype[rand() % VFA_NUM];
      break;
      }
    case VFloatFMA: break;
    case VFloatDivider: break;
    case VIntegerALU: break;
    case VPermutation: return VSLIDEUP; break;
    default:
      printf("Unsupported FuType %d\n", input.fuType);
      exit(1);
      return 0;
  }
  return 0;
}

uint8_t TestDriver::gen_random_sew() {
  switch (input.fuType)
  {
    case VIntegerALU: return rand()%4; break;
    case VPermutation: return rand()%4; break;
    default: return (rand()%3)+1; break;
  }
}

bool TestDriver::gen_random_widen() {
  if(input.sew > 1){
    switch (input.fuType)
    {
      case VFloatAdder: {
        if( input.fuOpType == VFADD || input.fuOpType == VFSUB )  return rand()%2 == 1; 
        else return false;
        break;
        }
      default: return false; break;
    }
  }
  else return false;
}

bool TestDriver::gen_random_src_widen() {
  if (input.widen) {
    switch (test_type.fuType)
    {
      case VFloatAdder: return rand()%2 == 1; break;
      default: return false; break;
    }
  }
  else return false;
}

void TestDriver::gen_random_vecinfo() {
  //               lmul =  8, 4, 2, 1,  1/2, 1/4, 1/8
  uint8_t vlmul_list[7] = {3, 2, 1, 0,  7,   6,   5};

  input.vinfo.vlmul = vlmul_list[rand() % (7 - input.sew)];
  int elements_per_reg = (VLEN / 8) >> input.sew;
  int vlmax = (input.vinfo.vlmul > 4) ? (elements_per_reg >> (8 - input.vinfo.vlmul)) : (elements_per_reg << input.vinfo.vlmul);
  input.vinfo.vstart = rand() % vlmax;
  input.vinfo.vl = rand() % (vlmax + 1);

  input.vinfo.vm = rand() % 2;
  input.vinfo.ta = rand() % 2;
  input.vinfo.ma = rand() % 2;
}

void TestDriver::gen_random_uopidx() {
  switch(input.fuType) {
    case VPermutation: {
      switch(input.fuOpType) {
        case VSLIDEUP: {
          if (input.vinfo.vlmul == 1) input.uop_idx = rand() % 3;
          else if (input.vinfo.vlmul == 2) input.uop_idx = rand() % 10;
          else if (input.vinfo.vlmul == 3) input.uop_idx = rand() % 36;
          else input.uop_idx = 0;
          break;
        }
        default: input.uop_idx = 0;
      }
      break;
    }
    case VFloatAdder: input.uop_idx = input.widen ? rand() % 2 : 0; break;
    default: input.uop_idx = 0;
  }
}

void TestDriver::get_random_input() {
  if (keepinput) { return; }
  input.src1[0] = rand64();
  input.src1[1] = rand64();
  input.src2[0] = rand64();
  input.src2[1] = rand64();
  input.src3[0] = rand64();
  input.src3[1] = rand64();
  input.src4[0] = rand64();
  input.src4[1] = rand64();
  if (!test_type.pick_fuType) { input.fuType = gen_random_futype(ALL_FUTYPES); }
  else { input.fuType = test_type.fuType; }
  if (!test_type.pick_fuOpType) { input.fuOpType = gen_random_optype(); }
  else { input.fuOpType = test_type.fuOpType; }
  input.sew = gen_random_sew();
  input.widen = gen_random_widen();
  input.src_widen = gen_random_src_widen();
  input.rm = rand() % 5;
  input.rm_s = rand() % 5;
  gen_random_vecinfo();
  gen_random_uopidx();

  // input.sew = 2;
  // input.widen = true;
  // input.src_widen = false;
  // input.uop_idx = 0;
}

void TestDriver::get_expected_output() {
  switch (input.fuType) {
    case VIntegerALU:
      if (verbose) { printf("FuType:%d, choose VIntegerALU %d\n", input.fuType, VIntegerALU); }
      expect_output = valu.get_expected_output(input); return;
    case VFloatAdder:
      if (verbose) { printf("FuType:%d, choose VFloatAdder %d\n", input.fuType, VFloatAdder); }
      expect_output = vfa.get_expected_output(input); return;
    case VFloatDivider:
      if (verbose) { printf("FuType:%d, choose VFloatDivider %d\n", input.fuType, VFloatDivider); }
      expect_output = vfd.get_expected_output(input); return;
    case VPermutation:
      if (verbose) { printf("FuType:%d, choose VPermutation %d\n", input.fuType, VPermutation); }
      expect_output = vperm.get_expected_output(input); return;
    default:
      printf("Unsupported FuType %d\n", input.fuType);
      exit(1);
      return;
  }
}
uint64_t TestDriver::rand64() {
  uint64_t tmp = rand();
  tmp = (tmp << 32) + (uint32_t) rand();
  return tmp;
}
// dut io check, return fire or not
bool TestDriver::assign_input_raising(VSimTop *dut_ptr) {
  if (!issued) {
    dut_ptr->io_in_valid = true;
    if (dut_ptr->io_in_ready) {
      issued = true;
      stuck_count = 0;
    }
  } else {
    dut_ptr->io_in_valid = false;
  }
  dut_ptr->io_in_bits_src_0_0 = input.src1[0];
  dut_ptr->io_in_bits_src_0_1 = input.src1[1];
  dut_ptr->io_in_bits_src_1_0 = input.src2[0];
  dut_ptr->io_in_bits_src_1_1 = input.src2[1];
  dut_ptr->io_in_bits_src_2_0 = input.src3[0];
  dut_ptr->io_in_bits_src_2_1 = input.src3[1];
  dut_ptr->io_in_bits_src_3_0 = input.src4[0];
  dut_ptr->io_in_bits_src_3_1 = input.src4[1];
  dut_ptr->io_in_bits_fuType  = input.fuType;
  dut_ptr->io_in_bits_fuOpType = input.fuOpType;
  dut_ptr->io_in_bits_sew     = input.sew;
  dut_ptr->io_in_bits_uop_idx = input.uop_idx;
  dut_ptr->io_in_bits_src_widen = input.src_widen;
  dut_ptr->io_in_bits_widen   = input.widen;
  dut_ptr->io_in_bits_rm      = input.rm;
  dut_ptr->io_in_bits_vinfo_vstart = input.vinfo.vstart;
  dut_ptr->io_in_bits_vinfo_vl     = input.vinfo.vl;
  dut_ptr->io_in_bits_vinfo_vlmul  = input.vinfo.vlmul;
  dut_ptr->io_in_bits_vinfo_vm     = input.vinfo.vm;
  dut_ptr->io_in_bits_vinfo_ta     = input.vinfo.ta;
  dut_ptr->io_in_bits_vinfo_ma     = input.vinfo.ma;
  // printf("fuType:%d fuOpType:%d inFuType:%d inFuOpType:%d\n", dut_ptr->io_in_bits_fuType,dut_ptr->io_in_bits_fuOpType,input.fuType,input.fuOpType);
  return  dut_ptr->io_in_valid;
}

int TestDriver::diff_output_falling(VSimTop *dut_ptr) {
  bool finish = dut_ptr->io_out_valid;
  if (finish) {
    // printf("Finished\n");
    dut_output.result[0] = dut_ptr->io_out_bits_result_0;
    dut_output.result[1] = dut_ptr->io_out_bits_result_1;
    dut_output.fflags[0] = dut_ptr->io_out_bits_fflags_0;
    dut_output.fflags[1] = dut_ptr->io_out_bits_fflags_1;

    if (memcmp(&dut_output, &expect_output, sizeof(dut_output))) {
      printf("Error, compare failed\n");
      display();
      return STATE_BADTRAP;
    } else {
      gen_next_test_case();
    }
    return STATE_FINISH_OPERATION;
  } else {
    stuck_count ++;
    if (stuck_count >= stuck_limit) {
      printf("DUT stucked. Not finished in %lu cycles\n", stuck_limit);
      stuck_count = 0;
      return STATE_BADTRAP;
    }
    return STATE_RUNNING;
  }
}

void TestDriver::display_ref_input() {
  printf("REF Input:\n");
  printf("  src1 %016lx_%016lx src2 %016lx_%016lx src3 %016lx_%016lx src4 %016lx_%016lx\n", input.src1[1], input.src1[0], input.src2[1], input.src2[0], input.src3[1], input.src3[0], input.src4[1], input.src4[0]);
  printf("  fuType %x fuOpType %x sew %x uop_idx %d src_widen %d widen %d rm %d\n", input.fuType, input.fuOpType, input.sew, input.uop_idx, input.src_widen, input.widen, input.rm);
  printf("  vstart %d vl %d vlmul %x vm %d ta %d ma %d\n", input.vinfo.vstart, input.vinfo.vl, input.vinfo.vlmul, input.vinfo.vm, input.vinfo.ta, input.vinfo.ma);
}

void TestDriver::display_ref_output() {
  printf("Expected Output \n");
  printf("  result  %016lx_%016lx fflags: %x_%x\n", expect_output.result[1], expect_output.result[0], expect_output.fflags[1], expect_output.fflags[0]);
}

void TestDriver::display_dut() {
  printf("DUT Output:\n");
  printf("  result  %016lx_%016lx fflags: %x_%x\n", dut_output.result[1], dut_output.result[0], dut_output.fflags[1], dut_output.fflags[0]);
}

void TestDriver::display() {
  display_ref_input();
  display_ref_output();
  display_dut();
}