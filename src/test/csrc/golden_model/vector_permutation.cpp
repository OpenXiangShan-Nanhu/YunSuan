#include "../include/gm_common.h"

VecOutput VGMPermutation::get_expected_output(VecInput input) {
  VecOutput output;

  switch(input.fuOpType) {
    case VSLIDEUP: {
      output = get_output_vslideup(input);
      break;
    }
    default: {
      printf("VGM Permutation, bad fuOpType %d\n", input.fuOpType);
      exit(1);
    }
  }
  return output;
}

VecOutput VGMPermutation::get_output_vslideup(VecInput input) {
  VecOutput output;

  int sew = input.sew;
  int vlmul = input.vinfo.vlmul;
  int elements_per_reg = (VLEN / 8) >> sew;
  int elements = (vlmul & 0x4) ? (elements_per_reg >> ((~vlmul & 0x7) + 1)) : elements_per_reg;

  if (verbose) {
    printf("%s::%s  vlmul:%d elements_per_reg:%d elements:%d cond:%d\n", typeid(this).name(), __func__, vlmul, elements_per_reg, elements, (vlmul & 0x4));
  }

  int uop_idx = input.uop_idx;
  int mask_start_idx;
  if (uop_idx == 0) mask_start_idx = 0;
  else if ((1 <= uop_idx) && (uop_idx <= 2)) mask_start_idx = 1;
  else if ((3 <= uop_idx) && (uop_idx <= 5)) mask_start_idx = 2;
  else if ((6 <= uop_idx) && (uop_idx <= 9)) mask_start_idx = 3;
  else if ((10 <= uop_idx) && (uop_idx <= 14)) mask_start_idx = 4;
  else if ((15 <= uop_idx) && (uop_idx <= 20)) mask_start_idx = 5;
  else if ((21 <= uop_idx) && (uop_idx <= 27)) mask_start_idx = 6;
  else if ((28 <= uop_idx) && (uop_idx <= 35)) mask_start_idx = 7;
  else { printf("VGM Permutation vslideup, bad uop_idx %d\n", uop_idx); exit(1); }
  mask_start_idx = mask_start_idx * elements_per_reg;

  uint16_t mask_selected = (mask_start_idx >= 64) ? input.src4[1] >> (mask_start_idx - 64) : input.src4[0] >> mask_start_idx;
  
  int slide_base;
  if ( uop_idx == 0 || uop_idx == 2 || uop_idx == 5 || uop_idx == 9 || uop_idx == 14 || uop_idx == 20 || uop_idx == 27 || uop_idx == 35 ) slide_base = 0;
  else if ( uop_idx == 1 || uop_idx == 4 || uop_idx == 8 || uop_idx == 13 || uop_idx == 19 || uop_idx == 26 || uop_idx == 34 ) slide_base = 1;
  else if ( uop_idx == 3 || uop_idx == 7 || uop_idx == 12 || uop_idx == 18 || uop_idx == 25 || uop_idx == 33 ) slide_base = 2;
  else if ( uop_idx == 6 || uop_idx == 11 || uop_idx == 17 || uop_idx == 24 || uop_idx == 32 ) slide_base = 3;
  else if ( uop_idx == 10 || uop_idx == 16 || uop_idx == 23 || uop_idx == 31 ) slide_base = 4;
  else if ( uop_idx == 15 || uop_idx == 22 || uop_idx == 30 ) slide_base = 5;
  else if ( uop_idx == 21 || uop_idx == 29 ) slide_base = 6;
  else if ( uop_idx == 28 ) slide_base = 7;
  else { printf("VGM Permutation vslideup, bad uop_idx %d\n", uop_idx); exit(1); }
  slide_base = slide_base * elements_per_reg;

  VSlideUpInput vslideup_input;
  vslideup_input.src_data = (uint64_t *)(input.src2);
  vslideup_input.prev_data = (uint64_t *)(input.src3);
  vslideup_input.mask = mask_selected;
  vslideup_input.slide = input.src1[0];
  vslideup_input.mask_start_idx = mask_start_idx;
  vslideup_input.slide_base = slide_base;
  vslideup_input.elements = elements;
  vslideup_input.vinfo = &(input.vinfo);

  switch(input.sew) {
    case 0: {
      VecOutputE8 output_e8 = vslideup_calculation_e8(&vslideup_input);
      output.result[0] = *(uint64_t *)(&output_e8.result[0]);
      output.result[1] = *(uint64_t *)(&output_e8.result[8]);
      break;
    }
    case 1: {
      VecOutputE16 output_e16 = vslideup_calculation_e16(&vslideup_input);
      output.result[0] = *(uint64_t *)(&output_e16.result[0]);
      output.result[1] = *(uint64_t *)(&output_e16.result[4]);
      break;
    }
    case 2: {
      VecOutputE32 output_e32 = vslideup_calculation_e32(&vslideup_input);
      output.result[0] = *(uint64_t *)(&output_e32.result[0]);
      output.result[1] = *(uint64_t *)(&output_e32.result[2]);
      break;
    }
    case 3: {
      VecOutput output_e64 = vslideup_calculation_e64(&vslideup_input);
      output.result[0] = output_e64.result[0];
      output.result[1] = output_e64.result[1];
      break;
    }
    default: {
      printf("VGM Permutation vslideup, bad sew %d\n", input.sew);
      exit(1);
    }
  }

  if (input.vinfo.vstart >= input.vinfo.vl) {
    output.result[0] = input.src3[0];
    output.result[1] = input.src3[1];
  }
  output.fflags[0] = output.fflags[1] = 0;

  if (verbose) {
    printf("%s::%s  src: %016lx_%016lx prev: %016lx_%016lx mask: %x\n", typeid(this).name(), __func__, vslideup_input.src_data[1], vslideup_input.src_data[0], vslideup_input.prev_data[1], vslideup_input.prev_data[0], vslideup_input.mask);
    printf("%s::%s  slide:%ld mask_start_idx:%d slide_base:%d elements:%d\n", typeid(this).name(), __func__, vslideup_input.slide, vslideup_input.mask_start_idx, vslideup_input.slide_base, vslideup_input.elements);
  }

  return output;
}

VecOutputE8 VGMPermutation::vslideup_calculation_e8(VSlideUpInput *input) {
  VecOutputE8 output;
  uint8_t *src_data = (uint8_t *)(input->src_data);
  uint8_t *prev_data = (uint8_t *)(input->prev_data);
  uint16_t mask = input->mask;
  uint64_t slide = input->slide;
  int mask_start_idx = input->mask_start_idx;
  int slide_base = input->slide_base;
  int elements = input->elements;
  int vstart = input->vinfo->vstart;
  int vl = input->vinfo->vl;
  bool vm = input->vinfo->vm;
  bool ta = input->vinfo->ta;
  bool ma = input->vinfo->ma;

  for (int i = 0; i < elements; i++) {
    int elements_idx = mask_start_idx + i;
    bool mask_i = (mask >> i) & 0x1;
    bool res_keep_old_vd = (!vm && !mask_i && !ma) || (elements_idx < vstart) || ((elements_idx >= vl) && !ta);
    bool res_agnostic = ((elements_idx >= vl) && ta) || (!vm && !mask_i && ma);

    if(res_keep_old_vd)
      output.result[i] = prev_data[i];
    else if(res_agnostic)
      output.result[i] = 0xff;
    else if((0 <= (slide_base - slide + i)) && ((slide_base - slide + i) < elements))
      output.result[i] = src_data[slide_base - slide + i];
    else
      output.result[i] = prev_data[i];
  }

  for (int i = elements; i < 16; i++) {
    output.result[i] = ta ? 0xff : prev_data[i];
  }

  return output;
}

VecOutputE16 VGMPermutation::vslideup_calculation_e16(VSlideUpInput *input) {
  VecOutputE16 output;
  uint16_t *src_data = (uint16_t *)(input->src_data);
  uint16_t *prev_data = (uint16_t *)(input->prev_data);
  uint16_t mask = input->mask;
  uint64_t slide = input->slide;
  int mask_start_idx = input->mask_start_idx;
  int slide_base = input->slide_base;
  int elements = input->elements;
  int vstart = input->vinfo->vstart;
  int vl = input->vinfo->vl;
  bool vm = input->vinfo->vm;
  bool ta = input->vinfo->ta;
  bool ma = input->vinfo->ma;

  for (int i = 0; i < elements; i++) {
    int elements_idx = mask_start_idx + i;
    bool mask_i = (mask >> i) & 0x1;
    bool res_keep_old_vd = (!vm && !mask_i && !ma) || (elements_idx < vstart) || ((elements_idx >= vl) && !ta);
    bool res_agnostic = ((elements_idx >= vl) && ta) || (!vm && !mask_i && ma);

    if (verbose) {
      printf("%s::%s  src: %016lx_%016lx prev: %016lx_%016lx mask: %x\n", typeid(this).name(), __func__, input->src_data[1], input->src_data[0], input->prev_data[1], input->prev_data[0], mask);
      printf("%s::%s  slide:%ld mask_start_idx:%d slide_base:%d elements:%d\n", typeid(this).name(), __func__, slide, mask_start_idx, slide_base, elements);
    }

    if(res_keep_old_vd)
      output.result[i] = prev_data[i];
    else if(res_agnostic)
      output.result[i] = 0xffff;
    else if((0 <= (slide_base - slide + i)) && ((slide_base - slide + i) < elements))
      output.result[i] = src_data[slide_base - slide + i];
    else
      output.result[i] = prev_data[i];

    if (verbose) {
      printf("%s::%s  elements_idx:%d mask_i:%d elements:%d res_keep_old_vd:%d res_agnostic:%d output.result[i]:%d\n", typeid(this).name(), __func__, elements_idx, mask_i, elements, res_keep_old_vd, res_agnostic, output.result[i]);
    }
  }

  for (int i = elements; i < 8; i++) {
    output.result[i] = ta ? 0xffff : prev_data[i];
  }

  return output;
}

VecOutputE32 VGMPermutation::vslideup_calculation_e32(VSlideUpInput *input) {
  VecOutputE32 output;
  uint32_t *src_data = (uint32_t *)(input->src_data);
  uint32_t *prev_data = (uint32_t *)(input->prev_data);
  uint16_t mask = input->mask;
  uint64_t slide = input->slide;
  int mask_start_idx = input->mask_start_idx;
  int slide_base = input->slide_base;
  int elements = input->elements;
  int vstart = input->vinfo->vstart;
  int vl = input->vinfo->vl;
  bool vm = input->vinfo->vm;
  bool ta = input->vinfo->ta;
  bool ma = input->vinfo->ma;

  for (int i = 0; i < elements; i++) {
    int elements_idx = mask_start_idx + i;
    bool mask_i = (mask >> i) & 0x1;
    bool res_keep_old_vd = (!vm && !mask_i && !ma) || (elements_idx < vstart) || ((elements_idx >= vl) && !ta);
    bool res_agnostic = ((elements_idx >= vl) && ta) || (!vm && !mask_i && ma);

    if(res_keep_old_vd)
      output.result[i] = prev_data[i];
    else if(res_agnostic)
      output.result[i] = 0xffffffff;
    else if((0 <= (slide_base - slide + i)) && ((slide_base - slide + i) < elements))
      output.result[i] = src_data[slide_base - slide + i];
    else
      output.result[i] = prev_data[i];
  }

  for (int i = elements; i < 4; i++) {
    output.result[i] = ta ? 0xffffffff : prev_data[i];
  }

  return output;
}

VecOutput VGMPermutation::vslideup_calculation_e64(VSlideUpInput *input) {
  VecOutput output;
  uint64_t *src_data = (uint64_t *)(input->src_data);
  uint64_t *prev_data = (uint64_t *)(input->prev_data);
  uint16_t mask = input->mask;
  uint64_t slide = input->slide;
  int mask_start_idx = input->mask_start_idx;
  int slide_base = input->slide_base;
  int elements = input->elements;
  int vstart = input->vinfo->vstart;
  int vl = input->vinfo->vl;
  bool vm = input->vinfo->vm;
  bool ta = input->vinfo->ta;
  bool ma = input->vinfo->ma;

  for (int i = 0; i < elements; i++) {
    int elements_idx = mask_start_idx + i;
    bool mask_i = (mask >> i) & 0x1;
    bool res_keep_old_vd = (!vm && !mask_i && !ma) || (elements_idx < vstart) || ((elements_idx >= vl) && !ta);
    bool res_agnostic = ((elements_idx >= vl) && ta) || (!vm && !mask_i && ma);

    if(res_keep_old_vd)
      output.result[i] = prev_data[i];
    else if(res_agnostic)
      output.result[i] = 0xffffffffffffffff;
    else if((0 <= (slide_base - slide + i)) && ((slide_base - slide + i) < elements))
      output.result[i] = src_data[slide_base - slide + i];
    else
      output.result[i] = prev_data[i];
  }

  for (int i = elements; i < 2; i++) {
    output.result[i] = ta ? 0xffffffffffffffff : prev_data[i];
  }

  return output;
}

ElementOutput VGMPermutation::calculation_e8(ElementInput  input) {ElementOutput rs; return rs;}
ElementOutput VGMPermutation::calculation_e16(ElementInput input) {ElementOutput rs; return rs;}
ElementOutput VGMPermutation::calculation_e32(ElementInput input) {ElementOutput rs; return rs;}
ElementOutput VGMPermutation::calculation_e64(ElementInput input) {ElementOutput rs; return rs;}