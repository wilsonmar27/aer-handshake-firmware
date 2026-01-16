set_false_path -from [get_ports ack];
set_false_path -from [get_ports btn0];
set_false_path -to [get_ports -regexp {data\\[.*\\]}];
set_false_path -to [get_ports led0_r];