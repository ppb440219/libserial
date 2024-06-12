#ifdef WIN32

#ifndef _WIN_SERIAL_H_
#define _WIN_SERIAL_H_

#include <Windows.h>
#include <queue>
#include <list>
#include "serial_data.h"

using namespace std;

enum baud_rate {
    BR_1200 = CBR_1200,
    BR_2400 = CBR_2400,
    BR_4800 = CBR_4800,
    BR_9600 = CBR_9600,
    BR_14400 = CBR_14400,
    BR_19200 = CBR_19200,
    BR_38400 = CBR_38400,
    BR_57600 = CBR_57600,
    BR_115200 = CBR_115200,
    BR_128000 = CBR_128000,
    BR_256000 = CBR_256000,
};

enum parity {
    no_parity = NOPARITY,
    odd_parity = ODDPARITY,
    even_parity = EVENPARITY,
    mark_parity = MARKPARITY,
    space_parity = SPACEPARITY,
};

enum stop_bits {
    one_stop_bits = ONESTOPBIT,
    one_point_five_stop_bits = ONE5STOPBITS,
    two_stop_bits = TWOSTOPBITS,
};

enum flow_control {
    no_flow_control,
    cts_rts_control,
    cts_dtr_control,
    dsr_rts_control,
    dsr_dtr_control,
    xon_xoff_control,
};

class win_serial
{
public:
    struct thread_ctx {
        HANDLE h_comm;
        HANDLE wait_evt;
        queue<serial_data*> *buf;
        bool exit;
    };

    static win_serial* get_instance(void);
    ~win_serial(void);

    bool open(unsigned int port_num, unsigned int baud, parity parity = parity::no_parity, unsigned int data_bits = 8,
        stop_bits stop_bits = stop_bits::one_stop_bits, flow_control flow_control = flow_control::no_flow_control);
    void close(void);
    list<int> get_available_port(void);
    bool write(unsigned char* buf, int len);

private:
    win_serial(void);
    bool serial_init(unsigned int port_num);
    bool serial_config(unsigned int baud, parity parity, unsigned int data_bits, stop_bits stop_bits,
        flow_control flow_control);

    static win_serial* instance;
    static CRITICAL_SECTION lock;
    struct thread_ctx output_thread_ctx;
    struct thread_ctx input_thread_ctx;
    HANDLE port_handle;
    queue<serial_data*> output_queue;
};

#endif //_WIN_SERIAL_H_
#endif //WIN32