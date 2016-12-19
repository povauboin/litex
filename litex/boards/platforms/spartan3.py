# This file is Copyright (c) 2015 Matt O'Gorman <mog@rldn.net>
# License: BSD

from litex.build.generic_platform import *
from litex.build.xilinx import XilinxPlatform
from litex.build.xilinx.programmer import XC3SProg, FpgaProg

_io = [
    ("clk", 0, Pins("T9"), IOStandard("LVCMOS33")),
    ("segment", 0, Pins("N16 F13 R16 P15 N15 G13 E14"), IOStandard("LVCMOS33")),
    ("digit_en", 0, Pins("E13 F14 G14 D14"), IOStandard("LVCMOS33")),
    ("sw", 0, Pins("F12 G12 H14 K14"), IOStandard("LVCMOS33")),
]

class Platform(XilinxPlatform):
    default_clk_name = "clk"
    default_clk_period = 20

    def __init__(self, programmer="xc3sprog"):
        self.programmer = programmer
        XilinxPlatform.__init__(self, "xc3s400-ft256-4", _io)

    def create_programmer(self):
        if self.programmer == "xc3sprog":
            return XC3SProg("minispartan6", "bscan_spi_minispartan6.bit")
        elif self.programmer == "fpgaprog":
            return FpgaProg()
        else:
            raise ValueError("{} programmer is not supported".format(programmer))
