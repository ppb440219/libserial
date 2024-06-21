#ifndef _SERIAL_INTF_H_
#define _SERIAL_INTF_H_

#include <list>

enum class baud_rate {
    BR_1200 = 1200,
    BR_2400 = 2400,
    BR_4800 = 4800,
    BR_9600 = 9600,
    BR_14400 = 14400,
    BR_19200 = 19200,
    BR_38400 = 38400,
    BR_57600 = 57600,
    BR_115200 = 115200,
    BR_128000 = 128000,
    BR_256000 = 256000,
};

enum class parity {
    no_parity,
    odd_parity,
    even_parity,
    mark_parity,
    space_parity,
};

enum class stop_bits {
    one_stop_bits,
    one_point_five_stop_bits,
    two_stop_bits,
};

enum class flow_control {
    no_flow_control,
    cts_rts_control,
    cts_dtr_control,
    dsr_rts_control,
    dsr_dtr_control,
    xon_xoff_control,
};

typedef void (*received_cb)(uint8_t);

class serial_intf {
public:
    virtual int open(unsigned int port_num, baud_rate baud, parity parity, unsigned int data_bits,
        stop_bits stop_bits, flow_control flow_control) = 0;

    virtual void close(void) = 0;

    virtual std::list<int> get_available_port(void) = 0;

    virtual bool write(const uint8_t *buf, const uint32_t len) = 0;

    virtual void register_data_cb(received_cb cb) = 0;

    unsigned int get_rate(baud_rate rate) {
        return static_cast<unsigned int>(rate);
    }
};

#endif //_SERIAL_INTF_H_