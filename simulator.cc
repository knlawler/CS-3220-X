#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <bitset>
#include <stdint.h>
#include <string.h> 
#include <cstring> 
#include <limits.h> 
// #include <cstdint> 
#include "simulator.h"


#define FLOAT_TO_FIXED1114(n) ((int)((n) * (float)(1<<(4)))) & 0xffff
#define FIXED_TO_FLOAT1114(n) ((float)(-1*((n>>15)&0x1)*(1<<11)) + (float)((n&(0x7fff)) / (float)(1<<4)))
#define FIXED1114_TO_INT(n) (( (n>>15)&0x1) ?  ((n>>4)|0xf000) : (n>>4)) 
#define DEBUG

using namespace std;

///////////////////////////////////
///  architectural structures /// 
///////////////////////////////////


ScalarRegister g_condition_code_register; // store conditional code 
ScalarRegister g_scalar_registers[NUM_SCALAR_REGISTER];  
VectorRegister g_vector_registers[NUM_VECTOR_REGISTER];

VertexRegister g_gpu_vertex_registers[NUM_VERTEX_REGISTER]; 
ScalarRegister g_gpu_status_register; 
 
unsigned char g_memory[MEMORY_SIZE]; // data memory 

////////////////////////////////////

vector<TraceOp> g_trace_ops;

unsigned int g_instruction_count = 0;
unsigned int g_vertex_id = 0; 
unsigned int g_current_pc = 0; 
unsigned int g_program_halt = 0; 

////////////////////////////////////////////////////////////////////////
// desc: Set g_condition_code_register depending on the values of val1 and val2
// hint: bit0 (N) is set only when val1 < val2
// bit 2: negative 
// bit 1: zero
// bit 0: positive 
////////////////////////////////////////////////////////////////////////
void SetConditionCodeInt(const int16_t val1, const int16_t val2) 
{
  if (val1 < val2)
    g_condition_code_register.int_value = 0x01;
  else if (val1 == val2)
    g_condition_code_register.int_value = 0x02;
  else
    g_condition_code_register.int_value = 0x04; 

}

void SetConditionCodeFloat(const float val1, const float val2) 
{
  if (val1 < val2)
    g_condition_code_register.int_value = 0x01;
  else if (val1 == val2)
    g_condition_code_register.int_value = 0x02;
  else
    g_condition_code_register.int_value = 0x04; 
}

////////////////////////////////////////////////////////////////////////
// Initialize global variables
////////////////////////////////////////////////////////////////////////
void InitializeGlobalVariables() 
{
  g_vertex_id = 0;  // internal setting variables 
  memset(&g_condition_code_register, 0x00, sizeof(ScalarRegister));
  memset(&g_gpu_status_register, 0x00, sizeof(ScalarRegister));
  memset(g_scalar_registers, 0x00, sizeof(ScalarRegister) * NUM_SCALAR_REGISTER);
  memset(g_vector_registers, 0x00, sizeof(VectorRegister) * NUM_VECTOR_REGISTER);
  memset(g_gpu_vertex_registers, 0x00, sizeof(VertexRegister) * NUM_VERTEX_REGISTER);
  memset(g_memory, 0x00, sizeof(unsigned char) * MEMORY_SIZE);
}

////////////////////////////////////////////////////////////////////////
// desc: Convert 16-bit 2's complement signed integer to 32-bit
////////////////////////////////////////////////////////////////////////
int SignExtension(const int16_t value) 
{
  return (value >> 15) == 0 ? value : ((0xFFFF << 16) | value);
}



////////////////////////////////////////////////////////////////////////
// desc: Decode binary-encoded instruction and Parse into TraceOp structure
//       which we will use execute later
// input: 32-bit encoded instruction
// output: TraceOp structure filled with the information provided from the input
////////////////////////////////////////////////////////////////////////
TraceOp DecodeInstruction(const uint32_t instruction) 
{
  TraceOp ret_trace_op;
  memset(&ret_trace_op, 0x00, sizeof(ret_trace_op));

  uint8_t opcode = (instruction & 0xFF000000) >> 24;
  ret_trace_op.opcode = opcode;

  switch (opcode) {
    
   
   
   case OP_ADD_D: 
   {
      int destination_register_idx = (instruction & 0x00F00000) >> 20;
      int source_register_1_idx = (instruction & 0x000F0000) >> 16;
      int source_register_2_idx = (instruction & 0x00000F00) >> 8;
      ret_trace_op.scalar_registers[0] = destination_register_idx;
      ret_trace_op.scalar_registers[1] = source_register_1_idx;
      ret_trace_op.scalar_registers[2] = source_register_2_idx;
   }
    break;
    
    /* fill out all the instruction's decode information */ 

    case OP_ADD_F:
    {
      int destination_register_idx = (instruction & 0x00F00000) >> 20;
      int source_register_1_idx = (instruction & 0x000F0000) >> 16;
      int source_register_2_idx = (instruction & 0x00000F00) >> 8;
      ret_trace_op.scalar_registers[0] = destination_register_idx;
      ret_trace_op.scalar_registers[1] = source_register_1_idx;
      ret_trace_op.scalar_registers[2] = source_register_2_idx;
    }
    break;

    case OP_ADDI_D:
    {
      int destination_register_idx = (instruction & 0x00F00000) >> 20;
      int source_register_idx_idx = (instruction & 0x000F0000) >> 16;
      int immediate_value = SignExtension(instruction & 0x0000FFFF);
      ret_trace_op.scalar_registers[0] = destination_register_idx;
      ret_trace_op.scalar_registers[1] = source_register_idx_idx;
      ret_trace_op.int_value = immediate_value;
    }
    break;

    case OP_ADDI_F: 
    {
      int destination_register_idx = (instruction & 0x00F00000) >> 20;
      int source_register_idx_idx = (instruction & 0x000F0000) >> 16;
      float immediate_value = FIXED_TO_FLOAT1114(instruction & 0x0000FFFF);
      ret_trace_op.scalar_registers[0] = destination_register_idx;
      ret_trace_op.scalar_registers[1] = source_register_idx;
      ret_trace_op.float_value = immediate_value;
      ret_trace_op.it_value = (int) immediate_value;
    }
    break;


    case OP_VADD:
    {
      int destination_register_idx = (instruction & 0x003F0000) >> 16;
      int source_register_1_idx = (instruction & 0x00003F00) >> 8;
      int source_register_2_idx = (instruction & 0x0000003F);
      ret_trace_op.vector_registers[0] = destination_register_idx;
      ret_trace_op.vector_registers[1] = source_register_1_idx;
      ret_trace_op.vector_registers[2] = source_register_2_idx;
    }
    break;

    case OP_AND_D:
    {
      int destination_register_idx = (instruction & 0x00F00000) >> 20;
      int source_register_1_idx = (instruction & 0x000F0000) >> 16;
      int source_register_2_idx = (instruction & 0x00000F00) >> 8;
      ret_trace_op.scalar_registers[0] = destination_register_idx;
      ret_trace_op.scalar_registers[1] = source_register_1_idx;
      ret_trace_op.scalar_registers[2] = source_register_2_idx;
    }
    break;

    case OP_ANDI_D:
    {
      int destination_register_idx = (instruction & 0x00F00000) >> 20;
      int source_register_1_idx = (instruction & 0x000F0000) >> 16;
      int immediate_value = SignExtension(instruction & 0x0000FFFF);
      ret_trace_op.scalar_registers[0] = destination_register_idx;
      ret_trace_op.scalar_registers[1] = source_register_idx_idx;
      ret_trace_op.int_value = immediate_value;
    }
    break;


    case OP_MOV:
    {
      int destination_register_idx = (instruction & 0x000F0000) >> 16;
      int source_register_idx = (instruction & 0x00000F00) >> 8;
      ret_trace_op.scalar_registers[0] = destination_register_idx;
      ret_trace_op.scalar_registers[1] = source_register_idx;
    }
    break;

    case OP_MOVI_D:
    {
      int destination_register_idx = (instruction & 0x000F0000) >> 16;
      int immediate_value = SignExtension(instruction & 0x0000FFFF);
      ret_trace_op.scalar_registers[0] = destination_register_idx;
      ret_trace_op.int_value = immediate_value;
    }
    break;

    case OP_MOVI_F:
    {
      int destination_register_idx = (instruction & 0x000F0000) >> 16;
      float immediate_value =  FIXED_TO_FLOAT1114(instruction & 0x0000FFFF);
      ret_trace_op.scalar_registers[0] = destination_register_idx;
      ret_trace_op.float_value = immediate_value;
    }
    break;

    case OP_VMOV:
    {
      int destination_register_idx = (instruction & 0x003F0000) >> 16;
      int source_register_idx = (instruction & 0x00003F00) >> 8;
      ret_trace_op.vector_registers[0] = destination_register_idx;
      ret_trace_op.vector_registers[1] = source_register_idx;
    }
    break;
 
    case OP_VMOVI:
    {
      int destination_register_idx = (instruction & 0x003F0000) >> 16;
      float immediate_value = FIXED_TO_FLOAT1114(instruction & 0x0000FFFF)
      ret_trace_op.vector_registers[0] = destination_register_idx;
      ret_trace_op.float_value = immediate_value;
    }
    break;

    case OP_CMP:
    {
      int source_register_1_idx = (instruction & 0x000F0000) >> 16;
      int source_register_2_idx = (instruction & 0x00000F00) >> 8;
      ret_trace_op.scalar_registers[0] = source_register_1_idx;
      ret_trace_op.scalar_registers[1] = source_register_2_idx;
    }
    break;

    case OP_CMPI:
    {
      int source_register_idx = (instruction & 0x000F0000) >> 16;
      int immediate_value = SignExtension(instruction & 0x0000FFFF);
      ret_trace_op.scalar_registers[0] = source_register_idx;
      ret_trace_op.int_value = immediate_value;
    }
    break;

    case OP_VCOMPMOV: 
    {
      int element_idx = (instruction & 0x00C00000) >> 22;
      int destination_register_idx = (instruction & 0x003F0000) >> 16;
      int source_register_idx = (instruction & 0x00000F00) >> 8;
      ret_trace_op.idx = element_idx;
      ret_trace_op.vector_registers[0] = destination_register_idx;
      ret_trace_op.scalar_registers[0] = source_register_idx;
    }
    break;

    case OP_VCOMPMOVI:
    {
      int element_idx = (instruction & 0x00C00000) >> 22;
      int destinatiob_register_idx = (instruction & 0x003F0000) >> 16;
      float immediate_value = FIXED_TO_FLOAT1114(instruction & 0x0000FFFF);
      ret_trace_op.idx = element_idx;
      ret_trace_op.vector_registers[0] = destination_register_idx;
      ret_trace_op.float_value = immediate_value;
    }
    break;

    case OP_LDB: 
    case OP_LDW:
    {
      int destination_register_idx = (instruction & 0x00F00000) >> 20;
      int base_register_idx = (instruction & 0x000F0000) >> 16;
      int offset = SignExtension((int16_t)(instruction & 0x0000FFFF));
      ret_trace_op.scalar_registers[0] = destination_register_idx;
      ret_trace_op.scalar_registers[1] = base_register_idx;
      ret_trace_op.int_value = offset;
    }
    break;

    case OP_STB:  
    case OP_STW:
    {
      int source_register_idx = (instruction & 0x00F00000) >> 20;
      int base_register_idx = (instruction & 0x000F0000) >> 16;
      int offset = SignExtension((int16_t)(instruction & 0x0000FFFF));
      ret_trace_op.scalar_registers[0] = source_register_idx;
      ret_trace_op.scalar_registers[1] = base_register_idx;
      ret_trace_op.int_value = offset;
    }
    break;

    case OP_SETVERTEX: 
    case OP_SETCOLOR:
    case OP_ROTATE:  // optional 
    case OP_TRANSLATE: 
    case OP_SCALE:  // optional
    {
      int vector_register_idx = (instruction & 0x003F0000) >> 16;
      ret_trace_op.vector_registers[0] vector_register_idx;
    }
    break;

    case OP_PUSHMATRIX:       // deprecated
    break;
 
    case OP_POPMATRIX:   // deprecated
    break;

    case OP_BEGINPRIMITIVE:
    {
      int primitive_type = (instruction & 0x000F0000) >> 16;
      ret_trace_op.scalar_registers[0] = base_register;
    }
    break;

    case OP_ENDPRIMITIVE:
    case OP_LOADIDENTITY:  // deprecated 
    break;

    case OP_FLUSH: 
    case OP_DRAW: 
    case OP_BRN: 
    case OP_BRZ:
    case OP_BRP:
    case OP_BRNZ:
    case OP_BRNP:
    case OP_BRZP:
    case OP_BRNZP:
    case OP_JSR:
    {
      int pc_offset = SignExtension((int16_t)(instruction & 0x0000FFFF));
      ret_trace_op.int_value = pc_offset;
    }
    break;

    case OP_JMP: 
    case OP_JSRR:
    {
      int base_register = (instruction & 0x000F0000) >> 16;
      ret_trace_op.scalar_registers[0] = base_register;
    } 
    case OP_HALT: 
      break; 
      
    default:
    break;
  }

  return ret_trace_op;
}

////////////////////////////////////////////////////////////////////////
// desc: Execute the behavior of the instruction (Simulate)
// input: Instruction to execute 
// output: Non-branch operation ? -1 : OTHER (PC-relative or absolute address)
////////////////////////////////////////////////////////////////////////
int ExecuteInstruction(const TraceOp &trace_op) 
{
  int ret_next_instruction_idx = -1;

  uint8_t opcode = trace_op.opcode;
  switch (opcode) {
    case OP_ADD_D: 
    {
      int source_value_1 = g_scalar_registers[trace_op.scalar_registers[1]].int_value;
      int source_value_2 = g_scalar_registers[trace_op.scalar_registers[2]].int_value;
      g_scalar_registers[trace_op.scalar_registers[0]].int_value = 
        source_value_1 + source_value_2;
      SetConditionCodeInt(g_scalar_registers[trace_op.scalar_registers[0]].int_value, 0);
    }
    break;


    /* fill out instruction behaviors */ 

    case OP_ADD_F:
    {
      float source_value_1 = g_scalar_registers[trace_op.scalar_registers[1]].float_value;
      float source_value_2 = g_scalar_registers[trace_op.scalar_registers[2]].float_value;
      g_scalar_registers[trace_op.scalar_registers[0]].float_value = 
        source_value_1 + source_value_2;
      SetConditionCodeFloat(g_scalar_registers[trace_op.scalar_registers[0]].float_value, 0.0f);
    }
    break;
 
    case OP_ADDI_D:
    {
      int source_value_1 = g_scalar_registers[trace_op.scalar_registers[1]].int_value;
      int source_value_2 = trace_op.int_value;
      g_scalar_registers[trace_op.scalar_registers[0]].int_value = 
        source_value_1 + source_value_2;
      SetConditionCodeInt(g_scalar_registers[trace_op.scalar_registers[0]].int_value, 0);
    }
    break;

    case OP_ADDI_F:
    {
      float source_value_1 = g_scalar_registers[trace_op.scalar_registers[1]].float_value;
      float source_value_2 = trace_op.float_value;
      g_scalar_registers[trace_op.scalar_registers[0]].float_value = 
        source_value_1 + source_value_2;
      SetConditionCodeFloat(g_scalar_registers[trace_op.scalar_registers[0]].int_value, 0.0f);
    }
    break;

    case OP_VADD:
    {
      for (int i = 0; i < NUM_VECTOR_ELEMENTS; i++)
        g_vector_registers[trace_op.vector_registers[0]].element[i].float_value = 
          g_vector_registers[trace_op.vector_registers[1]].element[i].float_value + 
          g_vector_registers[trace_op.vector_registers[2]].element[i].float_value;
    }
    break;

    case OP_AND_D:
    {
      int source_value_1 = g_scalar_registers[trace_op.scalar_registers[1]].int_value;
      int source_value_2 = g_scalar_registers[trace_op.scalar_registers[2]].int_value;
      g_scalar_registers[trace_op.scalar_registers[0]].int_value = 
        source_value_1 & source_value_2;
      SetConditionCodeInt(g_scalar_registers[trace_op.scalar_registers[0]].int_value, 0);
    }
    break;


    case OP_ANDI_D:
    {
      int source_value_1 = g_scalar_registers[trace_op.scalar_registers[1]].int_value;
      int source_value_2 = trace_op.int_value;
      g_scalar_registers[trace_op.scalar_registers[0]].int_value = 
        source_value_1 & source_value_2;
      SetConditionCodeInt(g_scalar_registers[trace_op.scalar_registers[0]].int_value, 0);
    }
    break;

    case OP_MOV:
     {
      if (trace_op.scalar_registers[0] < 7) {
        g_scalar_registers[trace_op.scalar_registers[0]].int_value =
          g_scalar_registers[trace_op.scalar_registers[1]].int_value;
        SetConditionCodeInt(g_scalar_registers[trace_op.registers[0]].int_value, 0);
      } else if (trace_op.scalar_registers[0] > 7) {
        g_scalar_registers[trace_op.scalar_registers[0]].float_value =
          g_scalar_registers[trace_op.scalar_registers[1]].float_value;
        SetConditionCodeFloat(g_scalar_registers[trace_op.scalar_registers[0]].float_value, 0.0f);
      }
    }
    break;


    case OP_MOVI_D:
    {
      g_scalar_registers[trace_op.scalar_registers[0]].int_value = trace_op.int_value;
      SetConditionCodeInt(g_scalar_registers[trace_op.scalar_registers[0]].int_value, 0);
    }
    break;

    case OP_MOVI_F:
    {
      g_scalar_registers[trace_op.scalar_registers[0]].float_value = trace_op.float_value;
      SetConditionCodeFloat(g_scalar_registers[trace_op.scalar_registers[0]].float_value, 0.0f);
    }
    break;

    case OP_VMOV:
    {
      for (int i = 0; i < NUM_VECTOR_ELEMENTS; i++) {
        g_vector_registers[trace_op.vector_registers[0]].element[i].float_value =
          g_vector_registers[trace_op.vector_registers[1]].element[i].float_value;
      }
    }
    break;
  
    case OP_VMOVI:
    {
      for (int i = 0; i < NUM_VECTOR_ELEMENTS; i++) {
        g_vector_registers[trace_op.vector_registers[0]].element[i].float_value =
          trace_op.float_value;
      }
    }
    break;
 
    case OP_CMP:
    {
      if (trace_op.scalar_registers[0] < 7)
        SetConditionCodeInt(
          g_scalar_registers[trace_op.scalar_registers[0]].int_value,
          g_scalar_registers[trace_op.scalar_registers[1]].int_value);
      else if (trace_op.scalar_registers[0] > 7)
        SetConditionCodeFloat(
          g_scalar_registers[trace_op.scalar_registers[0]].float_value,
          g_scalar_registers[trace_op.scalar_registers[1]].float_value);
 
    }
    break;

    case OP_CMPI:
    {
      if (trace_op.scalar_registers[0] < 7)
        SetConditionCodeInt(
          g_scalar_registers[trace_op.scalar_registers[0]].int_value,
          trace_op.int_value);
      else if (trace_op.scalar_registers[0] > 7)
        SetConditionCodeFloat(
          g_scalar_registers[trace_op.scalar_registers[0]].float_value,
          trace_op.float_value);
 
    }
    break;

    case OP_VCOMPMOV:
    {
      int idx = trace_op.idx;
      g_vector_registers[trace_op.vector_registers[0]].element[idx].float_value =
        g_scalar_registers[trace_op.scalar_registers[0]].float_value;
    }
    break; 

    case OP_VCOMPMOVI:
    {
      int idx - trace_op.idx;
      g_vector_registers[trace_op.vector_registers[0]].element[idx].float_value =
        trace_op.float_value;
    }
    break;
  
    case OP_LDB: 
    {
      int address = g_scalar_registers[trace_op.scalar_registers[1]].int_value
        + trace_op.int_value;
      memcpy(&g_scalar_registers[trace_op.scalar_registers[0]],
        &g_memory[address], sizeof(int8_t));
    }
    break;

    case OP_LDW:
    {
      int address = g_scalar_registers[trace_op.scalar_registers[1]].int_value
        + trace_op.int_value;
      memcpy(&g_scalar_registers[trace_op.scalar_registers[0]],
        &g_memory[address], sizeof(int16_t));
    }
    break;

    case OP_STB:
    {
      int address = g_scalar_registers[trace_op.scalar_registers[1]].int_value
        + trace_op.int_value;
      memcpy(&g_memory[address], 
        &g_scalar_registers[trace_op.scalar_registers[0]], sizeof(int8_t));
    }
    break;

 
    case OP_STW:
    {
      int address = g_scalar_registers[trace_op.scalar_registers[1]].int_value
        + trace_op.int_value;
      memcpy(&g_memory[address], 
        &g_scalar_registers[trace_op.scalar_registers[0]], sizeof(int16_t));
    }
    break;


    case OP_SETVERTEX:
    {
      float x_value =
        g_vector_registers[(trace_op.vector_registers[0])].element[1].float_value;
      float y_value =
        g_vector_registers[(trace_op.vector_registers[0])].element[2].float_value;
      float z_value =
        g_vector_registers[(trace_op.vector_registers[0])].element[3].float_value;
    }
    break;

    case OP_SETCOLOR:
    {
      int r_value = 
        (int) g_vector_registers[(trace_op.vector_registers[0])].element[0].float_value;
      int g_value = 
        (int) g_vector_registers[(trace_op.vector_registers[0])].element[1].float_value;
      int b_value = 
        (int) g_vector_registers[(trace_op.vector_registers[0])].element[2].float_value;
    }
    break;

    case OP_ROTATE:  // optional 
    case OP_TRANSLATE:
    {
      float x_value = 
        g_vector_registers[(trace_op.vector_registers[0])].element[1].float_value;
      float y_value = 
        g_vector_registers[(trace_op.vector_registers[0])].element[2].float_value;
    }
    break;
 
    case OP_SCALE:  // optional 
    case OP_PUSHMATRIX:       // deprecated 
    case OP_POPMATRIX:   // deprecated 
    case OP_BEGINPRIMITIVE: 
    case OP_ENDPRIMITIVE:
    case OP_LOADIDENTITY:  // deprecated 
    case OP_FLUSH: 
    case OP_DRAW: 
    case OP_BRN:
    {
      if (g_condition_code_register.int_value == 0x01)
      ret_next_instruction_idx = trace_op.int_value;
    }
    break;
 
    case OP_BRZ:
    {
      if (g_condition_code_register.int_value == 0x02)
      ret_next_instruction_idx = trace_op.int_value;
    }
    break;
 
    case OP_BRP:
    {
      if (g_condition_code_register.int_value == 0x04)
      ret_next_instruction_idx = trace_op.int_value;
    }
    break;
 
    case OP_BRNZ:
    {
      if (g_condition_code_register.int_value == 0x03)
      ret_next_instruction_idx = trace_op.int_value;
    }
    break;
 
    case OP_BRNP:
    {
      if (g_condition_code_register.int_value == 0x05)
      ret_next_instruction_idx = trace_op.int_value;
    }
    break;
 
    case OP_BRZP:
    {
      if (g_condition_code_register.int_value == 0x06)
      ret_next_instruction_idx = trace_op.int_value;
    }
    break;
 
    case OP_BRNZP:
    {
      if (g_condition_code_register.int_value == 0x07)
      ret_next_instruction_idx = trace_op.int_value;
    }
    break;
 
    case OP_JMP:
    {
      if (g_scalar_registers[trace_op.scalar_registers[0]].int_value == 0x07)
        ret_next_instruction_idx = g_scalar_registers[LR_IDX].int_value;
      else
        ret_next_instruction_idx = g_scalar_registers[trace_op.scalar_registers[0]].int_value;
    }
    break;

    case OP_JSR:
    {
      ret_next_instruction_idx = trace_op.int_value;
    }
    break;
 
    case OP_JSRR:
    {
      ret_next_instruction_idx = g_scalar_registers[trace_op.scalar_registers[0]].int_value;
    } 
    break; 
      
    case OP_HALT: 
      g_program_halt = 1; 
      break; 

    default:
    break;
    }

  return ret_next_instruction_idx;
}

////////////////////////////////////////////////////////////////////////
// desc: Dump given trace_op
////////////////////////////////////////////////////////////////////////
void PrintTraceOp(const TraceOp &trace_op) 
{  
  cout << "  opcode: " << SignExtension(trace_op.opcode);
  cout << ", scalar_register[0]: " << (int) trace_op.scalar_registers[0];
  cout << ", scalar_register[1]: " << (int) trace_op.scalar_registers[1];
  cout << ", scalar_register[2]: " << (int) trace_op.scalar_registers[2];
  cout << ", vector_register[0]: " << (int) trace_op.vector_registers[0];
  cout << ", vector_register[1]: " << (int) trace_op.vector_registers[1];
  cout << ", idx: " << (int) trace_op.idx;
  cout << ", primitive_index: " << (int) trace_op.primitive_type;
  cout << ", int_value: " << (int) SignExtension(trace_op.int_value) << endl; 
  //c  cout << ", float_value: " << (float) trace_op.float_value << endl;
}

////////////////////////////////////////////////////////////////////////
// desc: This function is called every trace is executed
//       to provide the contents of all the registers
////////////////////////////////////////////////////////////////////////
void PrintContext(const TraceOp &current_op)
{

  cout << "--------------------------------------------------" << endl;
  cout << "3220X-Instruction Count: " << g_instruction_count
       << " C_PC: " << (g_current_pc *4)
       << " C_PC_IND: " << g_current_pc 
       << ", Curr_Opcode: " << current_op.opcode
       << " NEXT_PC: " << ((g_scalar_registers[PC_IDX].int_value)<<2) 
       << " NEXT_PC_IND: " << (g_scalar_registers[PC_IDX].int_value)
       << ", Next_Opcode: " << g_trace_ops[g_scalar_registers[PC_IDX].int_value].opcode 
       << endl;
  cout <<"3220X-"; 
  for (int srIdx = 0; srIdx < NUM_SCALAR_REGISTER; srIdx++) {
    cout << "R" << srIdx << ":" 
         << ((srIdx < 8 || srIdx == 15) ? SignExtension(g_scalar_registers[srIdx].int_value) : (float) FIXED_TO_FLOAT1114(g_scalar_registers[srIdx].int_value)) 
         << (srIdx == NUM_SCALAR_REGISTER-1 ? "" : ", ");
  }

  cout << " CC :N: " << ((g_condition_code_register.int_value &0x4) >>2) << " Z: " 
       << ((g_condition_code_register.int_value &0x2) >>1) << " P: " << (g_condition_code_register.int_value &0x1) << "  "; 
  cout << " draw: " << (g_gpu_status_register.int_value &0x01) << " fush: " << ((g_gpu_status_register.int_value & 0x2)>>1) ;
  cout << " prim_type: "<< ((g_gpu_status_register.int_value & 0x4) >> 2)  << " "; 
   
  cout << endl;
  
  // for (int vrIdx = 0; vrIdx < NUM_VECTOR_REGISTER; vrIdx++) {
  
  for (int vrIdx = 0; vrIdx < 6; vrIdx++) {
    cout <<"3220X-"; 
    cout << "V" << vrIdx << ":";
    for (int elmtIdx = 0; elmtIdx < NUM_VECTOR_ELEMENTS; elmtIdx++) { 
      cout << "Element[" << elmtIdx << "] = " 
           << (float)FIXED_TO_FLOAT1114(g_vector_registers[vrIdx].element[elmtIdx].int_value) 
           << (elmtIdx == NUM_VECTOR_ELEMENTS-1 ? "" : ",");
    }
    cout << endl;
  }
  cout << endl;
  cout <<"3220X-"; 
  cout <<" vertices P1_X: " << g_gpu_vertex_registers[0].x_value; 
  cout <<" vertices P1_Y: " << g_gpu_vertex_registers[0].y_value; 
  cout <<" r: " << g_gpu_vertex_registers[0].r_value; 
  cout <<" g: " << g_gpu_vertex_registers[0].g_value; 
  cout <<" b: " << g_gpu_vertex_registers[0].b_value; 
  cout <<" P2_X: " << g_gpu_vertex_registers[1].x_value; 
  cout <<" P2_Y: " << g_gpu_vertex_registers[1].y_value; 
  cout <<" r: " << g_gpu_vertex_registers[1].r_value; 
  cout <<" g: " << g_gpu_vertex_registers[1].g_value; 
  cout <<" b: " << g_gpu_vertex_registers[1].b_value; 
  cout <<" P3_X: " << g_gpu_vertex_registers[2].x_value; 
  cout <<" P3_Y: " << g_gpu_vertex_registers[2].y_value; 
  cout <<" r: " << g_gpu_vertex_registers[2].r_value; 
  cout <<" g: " << g_gpu_vertex_registers[2].g_value; 
  cout <<" b: " << g_gpu_vertex_registers[2].b_value << endl; 
  
  cout << "--------------------------------------------------" << endl;
}

int main(int argc, char **argv) 
{
  ///////////////////////////////////////////////////////////////
  // Initialize Global Variables
  ///////////////////////////////////////////////////////////////
  //
  InitializeGlobalVariables();

  ///////////////////////////////////////////////////////////////
  // Load Program
  ///////////////////////////////////////////////////////////////
  //
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " <input>" << endl;
    return 1;
  }

  ifstream infile(argv[1]);
  if (!infile) {
    cerr << "Error: Failed to open input file " << argv[1] << endl;
    return 1;
  }
  vector< bitset<sizeof(uint32_t)*CHAR_BIT> > instructions;
  while (!infile.eof()) {
    bitset<sizeof(uint32_t)*CHAR_BIT> bits;
    infile >> bits;
    if (infile.eof())  break;
    instructions.push_back(bits);
  }
  
  infile.close();

#ifdef DEBUG
  cout << "The contents of the instruction vectors are :" << endl;
  for (vector< bitset<sizeof(uint32_t)*CHAR_BIT> >::iterator ii =
      instructions.begin(); ii != instructions.end(); ii++) {
    cout << "  " << *ii << endl;
  }
#endif // DEBUG

  ///////////////////////////////////////////////////////////////
  // Decode instructions into g_trace_ops
  ///////////////////////////////////////////////////////////////
  //
  for (vector< bitset<sizeof(uint32_t)*CHAR_BIT> >::iterator ii =
      instructions.begin(); ii != instructions.end(); ii++) {
    uint32_t inst = (uint32_t) ((*ii).to_ulong());
    TraceOp trace_op = DecodeInstruction(inst);
    g_trace_ops.push_back(trace_op);
  }

#ifdef DEBUG
  cout << "The contents of the g_trace_ops vectors are :" << endl;
  for (vector<TraceOp>::iterator ii = g_trace_ops.begin();
      ii != g_trace_ops.end(); ii++) {
    PrintTraceOp(*ii);
  }
#endif // DEBUG

  ///////////////////////////////////////////////////////////////
  // Execute 
  ///////////////////////////////////////////////////////////////
  //
  g_scalar_registers[PC_IDX].int_value = 0;
  for (;;) {
    TraceOp current_op = g_trace_ops[g_scalar_registers[PC_IDX].int_value];
    int idx = ExecuteInstruction(current_op);
    g_current_pc = g_scalar_registers[PC_IDX].int_value; // debugging purpose only 
    if (current_op.opcode == OP_JSR || current_op.opcode == OP_JSRR)
      g_scalar_registers[LR_IDX].int_value = (g_scalar_registers[PC_IDX].int_value + 1) << 2 ;


    
    g_scalar_registers[PC_IDX].int_value += 1; 
    if (idx != -1) { // Branch
      if (current_op.opcode == OP_JMP || current_op.opcode == OP_JSRR) // Absolute addressing
        g_scalar_registers[PC_IDX].int_value = idx; 
      else // PC-relative addressing (OP_JSR || OP_BRXXX)
        g_scalar_registers[PC_IDX].int_value += idx; 
    }

#ifdef DEBUG
    g_instruction_count++;
    PrintContext(current_op);
#endif // DEBUG

    if (g_program_halt == 1) 
      break;
  }

  return 0;
}
