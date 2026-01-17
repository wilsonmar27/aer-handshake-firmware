## 12 MHz System Clock
set_property -dict { PACKAGE_PIN M9    IOSTANDARD LVCMOS33 } [get_ports { sys_clk }]; # IO_L13P_T2_MRCC_14 Sch=gclk
create_clock -add -name sys_clk_pin -period 83.33 -waveform {0 41.66} [get_ports { sys_clk }];

## Push Buttons
set_property -dict { PACKAGE_PIN D2    IOSTANDARD LVCMOS33 } [get_ports { btn0 }]; # IO_L6P_T0_34 Sch=btn[0]

## RGB LEDs
#set_property -dict { PACKAGE_PIN F1    IOSTANDARD LVCMOS33 } [get_ports { led0_b }]; # IO_L10N_T1_34 Sch=led0_b
#set_property -dict { PACKAGE_PIN D3    IOSTANDARD LVCMOS33 } [get_ports { led0_g }]; # IO_L9N_T1_DQS_34 Sch=led0_g
set_property -dict { PACKAGE_PIN F2    IOSTANDARD LVCMOS33 } [get_ports { led0_r }]; # IO_L10P_T1_34 Sch=led0_r

## Dedicated Digital I/O on the PIO Headers
set_property -dict { PACKAGE_PIN L1    IOSTANDARD LVCMOS33 } [get_ports { data[0] }]; # IO_L18N_T2_34 Sch=pio[01]
set_property -dict { PACKAGE_PIN M4    IOSTANDARD LVCMOS33 } [get_ports { data[1] }]; # IO_L19P_T3_34 Sch=pio[02]
set_property -dict { PACKAGE_PIN M3    IOSTANDARD LVCMOS33 } [get_ports { data[2] }]; # IO_L19N_T3_VREF_34 Sch=pio[03]
set_property -dict { PACKAGE_PIN N2    IOSTANDARD LVCMOS33 } [get_ports { data[3] }]; # IO_L20P_T3_34 Sch=pio[04]
set_property -dict { PACKAGE_PIN M2    IOSTANDARD LVCMOS33 } [get_ports { data[4] }]; # IO_L20N_T3_34 Sch=pio[05]
set_property -dict { PACKAGE_PIN P3    IOSTANDARD LVCMOS33 } [get_ports { data[5] }]; # IO_L21P_T3_DQS_34 Sch=pio[06]
set_property -dict { PACKAGE_PIN N3    IOSTANDARD LVCMOS33 } [get_ports { data[6] }]; # IO_L21N_T3_DQS_34 Sch=pio[07]
set_property -dict { PACKAGE_PIN P1    IOSTANDARD LVCMOS33 } [get_ports { data[7] }]; # IO_L22P_T3_34 Sch=pio[08]
set_property -dict { PACKAGE_PIN N1    IOSTANDARD LVCMOS33 } [get_ports { ack }]; # IO_L22N_T3_34 Sch=pio[09]
set_property -dict { PACKAGE_PIN P14   IOSTANDARD LVCMOS33 } [get_ports { data[8] }]; # IO_L11P_T1_SRCC_14 Sch=pio[16]
set_property -dict { PACKAGE_PIN P15   IOSTANDARD LVCMOS33 } [get_ports { data[9] }]; # IO_L11N_T1_SRCC_14 Sch=pio[17]
set_property -dict { PACKAGE_PIN N13   IOSTANDARD LVCMOS33 } [get_ports { data[10] }]; # IO_L8N_T1_D12_14 Sch=pio[18]
set_property -dict { PACKAGE_PIN N15   IOSTANDARD LVCMOS33 } [get_ports { data[11] }]; # IO_L10N_T1_D15_14 Sch=pio[19]

set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property BITSTREAM.CONFIG.CONFIGRATE 33 [current_design]
set_property CONFIG_MODE SPIx4 [current_design]