#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Vdut.h"
#include "Vdut.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <verilated.h>


VerilatedVcdC* tfp;

extern "C" void lambdasim_eval(void *vdut)
{
  Vdut *dut = (Vdut*)vdut;
  dut->eval();
}

extern "C" void lambdasim_init_tracer(void *vdut)
{
  Vdut *dut = (Vdut*)vdut;
  Verilated::traceEverOn(true);
  tfp = new VerilatedVcdC;
  dut->trace(tfp, 99);
  tfp->open("dut.vcd");
}

extern "C" void lambdasim_tracer_dump()
{
  static unsigned int ticks=0;
  tfp->dump(ticks++);
}


vluint64_t main_time = 0;
double sc_time_stamp()
{
  return main_time;
}
