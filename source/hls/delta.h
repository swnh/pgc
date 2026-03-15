#include <systemc.h>

SC_MODULE(delta) {
    // Clock and Reset
    sc_in<bool>             clk;
    sc_in<bool>             rst;

    // Input
    sc_in<sc_uint<53>>      data_in;
    sc_in<bool>             tb_out_vld;
    sc_in<bool>             binarizer_in_rdy;
    
    // Output
    sc_out<sc_uint<53>>     data_out;
    sc_out<bool>            delta_in_rdy;
    sc_out<bool>            delta_out_vld;

    // History Buffer
    sc_uint<53>             history_buffer[32];


    // Process
    void delta_calc() {
        // Reset state
        data_out.write(0);
        delta_in_rdy.write(false);
        delta_out_vld.write(false);
        
        for (int i = 0; i < 32; i++) {
            history_buffer[i] = 0;
        }
         
        sc_uint<5>          laser_id;
        sc_uint<16>         curr_x, curr_y, curr_z;
        sc_uint<16>         prev_x, prev_y, prev_z;
        sc_int<16>          dx, dy, dz;
        sc_uint<53>         curr_data, prev_data, out_data;

        wait();

        while (true) {
            // Assert ready
            delta_in_rdy.write(true);
            wait();

            if (tb_out_vld.read() && delta_in_rdy.read()) {
                delta_in_rdy.write(false);
                curr_data = data_in.read();

                laser_id = curr_data.range(52, 48);
                curr_x = curr_data.range(47, 32);
                curr_y = curr_data.range(31, 16);
                curr_z = curr_data.range(15, 0);

                prev_data = history_buffer[laser_id];
                prev_x = prev_data.range(47, 32);
                prev_y = prev_data.range(31, 16);
                prev_z = prev_data.range (15, 0);

                dx = curr_x - prev_x;
                dy = curr_y - prev_y;
                dz = curr_z - prev_z;

                history_buffer[laser_id] = curr_data;

                out_data.range(52, 48) = laser_id;
                out_data.range(47, 32) = dx;
                out_data.range(31, 16) = dy;
                out_data.range(15,  0) = dz;

                // Drive output and wait for downstream ready
                delta_out_vld.write(true);
                data_out.write(out_data);

                do { wait(); } while (!binarizer_in_rdy.read());
                
                delta_out_vld.write(false);
            }
            // if not valid, loop back - ready goes high again next iteration
        }
    }

    SC_CTOR(delta) {
        SC_CTHREAD(delta_calc, clk.pos());
        async_reset_signal_is(rst, true);
    }
};