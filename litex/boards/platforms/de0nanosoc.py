from litex.build.generic_platform import *
from litex.build.altera import AlteraPlatform
from litex.build.altera.programmer import USBBlaster

_io = [
    ("fpga_clk1_50", 0, Pins("V11"), IOStandard("3.3-V LVTTL")),
    ("fpga_clk2_50", 0, Pins("Y13"), IOStandard("3.3-V LVTTL")),
    ("fpga_clk3_50", 0, Pins("E11"), IOStandard("3.3-V LVTTL")),
    ("hps_clk1_25", 0, Pins("E20"), IOStandard("3.3-V LVTTL")),
    ("hps_clk2_25", 0, Pins("D20"), IOStandard("3.3-V LVTTL")),

    ("user_led", 0, Pins("W15"), IOStandard("3.3-V LVTTL")),
    ("user_led", 1, Pins("AA24"), IOStandard("3.3-V LVTTL")),
    ("user_led", 2, Pins("V16"), IOStandard("3.3-V LVTTL")),
    ("user_led", 3, Pins("V15"), IOStandard("3.3-V LVTTL")),
    ("user_led", 4, Pins("AF26"), IOStandard("3.3-V LVTTL")),
    ("user_led", 5, Pins("AE26"), IOStandard("3.3-V LVTTL")),
    ("user_led", 6, Pins("Y16"), IOStandard("3.3-V LVTTL")),
    ("user_led", 7, Pins("AA23"), IOStandard("3.3-V LVTTL")),

    ("key", 0, Pins("AH17"), IOStandard("3.3-V LVTTL")),
    ("key", 1, Pins("AH16"), IOStandard("3.3-V LVTTL")),

    ("sw", 0, Pins("L10"), IOStandard("3.3-V LVTTL")),
    ("sw", 1, Pins("L9"), IOStandard("3.3-V LVTTL")),
    ("sw", 2, Pins("H6"), IOStandard("3.3-V LVTTL")),
    ("sw", 3, Pins("H5"), IOStandard("3.3-V LVTTL")),

    ("serial", 0,
        Subsignal("tx", Pins("V12"), IOStandard("3.3-V LVTTL")),
        Subsignal("rx", Pins("AF7"), IOStandard("3.3-V LVTTL"))
    ),
    ("serial", 1,
        Subsignal("tx", Pins("Y15"), IOStandard("3.3-V LVTTL")),
        Subsignal("rx", Pins("AG28"), IOStandard("3.3-V LVTTL"))
    ),

    ("gpio_0", 0,
        # V12 AF7 used for serial 0
        Pins("V12 AF7 W12 AF8 Y8 AB4 W8 Y4 Y5 U11 T8 T12 AH5 AH6 AH4 AG5",
            "AH3 AH2 AF4 AG6 AF5 AE4 T13 T11 AE7 AF6 AF9 AE8 AD10 AE9 AD11 AF10",
            "AD12 AE11 AF11 AE12"),
        IOStandard("3.3-V LVTTL")
    ),
    ("gpio_1", 0,
        # Y15 AG28 used for serial 1
        Pins("Y15 AG28 AA15 AH27 AG26 AH24 AF23 AE22 AF21 AG20 AG19 AF20 AC23 AG18 AH26 AA19",
            "AG24 AF25 AH23 AG23 AE19 AF18 AD19 AE20 AE24 AD20 AF22 AH22 AH19 AH21 AG21 AH18",
            "AD23 AE23 AA18 AC22"),
        IOStandard("3.3-V LVTTL")
    ),
]

class Platform(AlteraPlatform):
    default_clk_name = "fpga_clk1_50"
    default_clk_period = 20

    def __init__(self):
        AlteraPlatform.__init__(self, "5CSEMA4U23C6", _io)

    def create_programmer(self):
        return USBBlaster()
