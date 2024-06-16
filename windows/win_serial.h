#ifdef WIN32

#ifndef _WIN_SERIAL_H_
#define _WIN_SERIAL_H_

#include <Windows.h>
#include <queue>
#include <list>
#include "serial_intf.h"
#include "serial_data.h"

using namespace std;

class win_serial : serial_intf
{
public:
    struct thread_ctx {
        HANDLE h_comm;
        HANDLE wait_evt;
        queue<serial_data*> *buf;
        received_cb rcv_callback;
        bool exit;
    };

    static win_serial* get_instance(void);
    ~win_serial(void);

    bool open(unsigned int port_num, baud_rate baud, parity parity = parity::no_parity, unsigned int data_bits = 8,
        stop_bits stop_bits = stop_bits::one_stop_bits, flow_control flow_control = flow_control::no_flow_control);
    void close(void);
    list<int> get_available_port(void);
    bool write(unsigned char* buf, int len);
    void register_data_cb(received_cb callback);

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