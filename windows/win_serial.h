#ifndef _WIN_SERIAL_H_
#define _WIN_SERIAL_H_

#include <Windows.h>
#include <queue>
#include <list>
#include "../serial_intf.h"
#include "../serial_data.h"

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

    win_serial(void);

    ~win_serial(void);

    int open(unsigned int port_num, baud_rate baud, parity parity = parity::no_parity, unsigned int data_bits = 8,
        stop_bits stop_bits = stop_bits::one_stop_bits, flow_control flow_control = flow_control::no_flow_control);

    void close(void);

    list<int> get_available_port(void);

    bool write(const uint8_t *buf, const uint32_t len);

    void register_data_cb(received_cb callback);

private:
    bool serial_init(unsigned int port_num);

    int serial_config(baud_rate baud, parity parity, unsigned int data_bits, stop_bits stop_bits,
        flow_control flow_control);

    BYTE get_stop_bits(stop_bits stop_bits);

    BYTE get_parity(parity parity);

    struct thread_ctx output_thread_ctx;
    struct thread_ctx input_thread_ctx;
    HANDLE port_handle;
    queue<serial_data*> output_queue;
};

#endif //_WIN_SERIAL_H_
