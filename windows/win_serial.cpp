#include <iostream>
#include <string>
#include "win_serial.h"
#include "../serial_err.h"

#define NUM_INPUT_HANDLES 2

#define TAG "win_serial"

using namespace std;

win_serial *win_serial::instance = NULL;
CRITICAL_SECTION win_serial::lock;

DWORD write_to_hal(HANDLE h_comm, const unsigned char *buf, DWORD len)
{
    OVERLAPPED ove = { 0 };
    DWORD bytes_write;
    DWORD ret;

    ove.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ove.hEvent == NULL)
        return FALSE;

    if (!WriteFile(h_comm, buf, len, &bytes_write, &ove)) {
        if (GetLastError() != ERROR_IO_PENDING)
            ret = GetLastError();
        else {
            DWORD dwRes = WaitForSingleObject(ove.hEvent, INFINITE);
            switch (dwRes) {
                case WAIT_OBJECT_0:
                    if (!GetOverlappedResult(h_comm, &ove, &bytes_write, FALSE))
                        ret = GetLastError();
                    else {
                        // write completed successfully
                        ret = ERROR_SUCCESS;
                    }
                    break;
                default:
                    // an error has occurred in WaitForSingleObject.
                    // this usually indicates a problem with the OVERLAPPED structure's event handle.
                    ret = GetLastError();
                    break;
            }
        }
    }
    else {
        // write completed successfully
        ret = ERROR_SUCCESS;
    }

    CloseHandle(ove.hEvent);
    return ret;
}

DWORD WINAPI output_thread_handle(void *param)
{
    struct win_serial::thread_ctx *ctx = (struct win_serial::thread_ctx*)param;

    while (1) {
        WaitForSingleObject(ctx->wait_evt, INFINITE);

        if (ctx->exit)
            break;

        // obtain buf
        queue<serial_data*> *q = ctx->buf;
        if (q != NULL && q->size() > 0) {
            serial_data *data = q->front();
            DWORD ret = write_to_hal(ctx->h_comm, data->buf, data->length);

            if (ret == ERROR_SUCCESS) {
                data->clear_self();
                q->pop();
                delete data;
            }
            else {
                // TODO: how to handle write error?
            }
        }
    }

    CloseHandle(ctx->wait_evt);

    return 0;
}

DWORD WINAPI input_thread_handle(void *param)
{
    struct win_serial::thread_ctx *ctx = (struct win_serial::thread_ctx*)param;
    DWORD bytes_read;
    unsigned char buf;
    HANDLE evt_array[NUM_INPUT_HANDLES];

    OVERLAPPED read_evt = { 0 };
    HANDLE oev = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (oev == NULL)
        return 0;

    evt_array[0] = oev;
    evt_array[1] = ctx->wait_evt;

    while (1) {
        memset(&read_evt, 0, sizeof(read_evt));
        read_evt.hEvent = oev;

        bool readret = ReadFile(ctx->h_comm, &buf, 1, &bytes_read, &read_evt);
        if (!readret && GetLastError() != ERROR_IO_PENDING) {
            // get IO error from GetLastError()
            break;
        }

        DWORD ret = WaitForMultipleObjects(NUM_INPUT_HANDLES, evt_array, FALSE, INFINITE);
        switch (ret) {
            case WAIT_OBJECT_0:
                readret = GetOverlappedResult(ctx->h_comm, &read_evt, &bytes_read, TRUE);

                if (bytes_read > 0 && ctx->rcv_callback != NULL)
                    ctx->rcv_callback(buf);
                break;
            case WAIT_OBJECT_0 + 1:
                if (ctx->exit)
                    goto end_thread;
                break;
        }
    }

end_thread:
    CloseHandle(oev);
    CloseHandle(ctx->wait_evt);

    return 0;
}

win_serial *win_serial::get_instance(void)
{
    InitializeCriticalSection(&lock);
    EnterCriticalSection(&lock);

    if (instance == NULL)
        instance = new win_serial();

    LeaveCriticalSection(&lock);

    return instance;
}

win_serial::win_serial(void)
{
    port_handle = INVALID_HANDLE_VALUE;
    output_thread_ctx = { 0 };
    input_thread_ctx = { 0 };
}

win_serial::~win_serial(void)
{
    if (port_handle != INVALID_HANDLE_VALUE)
       close();
    DeleteCriticalSection(&lock);
}

list<int> win_serial::get_available_port()
{
    char path[2000]; // store the path of the COM port
    list<int> portList;

    // checking ports from COM0 to COM255
    for (int i = 0; i < 255; i++) {
        string port_name = "COM" + to_string(i);
        std::fill(path, path + 2000, 0);

        // test the path exist
        DWORD ret = QueryDosDevice(port_name.c_str(), path, sizeof(path));
        if (ret != 0) {
            portList.push_back(i);
        }
    }

    return portList;
}

int win_serial::open(unsigned int port_num, baud_rate baud, parity parity, unsigned int data_bits,
    stop_bits stop_bits, flow_control flow_control)
{
    int ret = 0;
    DWORD out_thread_id, in_thread_id; /* required for Win9x */

    EnterCriticalSection(&win_serial::lock);

    if (!serial_init(port_num)) {
        ret = SERIAL_OPEN_FAILED;
        goto open_err;
    }

    ret = serial_config(baud, parity, data_bits, stop_bits, flow_control);
    if (ret < 0)
        goto open_err;

    // clear RX/TX buffer
    PurgeComm(port_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);

    output_thread_ctx.h_comm = port_handle;
    output_thread_ctx.wait_evt = CreateEvent(NULL, false, false, TEXT("out_thread_wait_evt")); // auto reset event
    output_thread_ctx.exit = false;
    output_thread_ctx.buf = &output_queue;
    CreateThread(NULL, 0, output_thread_handle, &output_thread_ctx, 0, &out_thread_id);

    input_thread_ctx.h_comm = port_handle;
    input_thread_ctx.wait_evt = CreateEvent(NULL, false, false, TEXT("in_thread_wait_evt")); // auto reset event
    input_thread_ctx.exit = false;
    input_thread_ctx.buf = NULL;
    CreateThread(NULL, 0, input_thread_handle, &input_thread_ctx, 0, &in_thread_id);

open_err:
    LeaveCriticalSection(&win_serial::lock);
    return ret;
}

void win_serial::close(void)
{
    EnterCriticalSection(&win_serial::lock);

    // stop thread
    output_thread_ctx.exit = true;
    SetEvent(output_thread_ctx.wait_evt);
    input_thread_ctx.exit = true;
    SetEvent(input_thread_ctx.wait_evt);

    // stop port
    if (port_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(port_handle);
        port_handle = INVALID_HANDLE_VALUE;
    }

    // clear queued output buffer
    while (!output_queue.empty()) {
        serial_data *data = output_queue.front();
        data->clear_self();
        output_queue.pop();
        delete data;
    }

    LeaveCriticalSection(&win_serial::lock);
}

bool win_serial::serial_init(unsigned int port_num)
{
    string port_name = "\\\\.\\COM" + to_string(port_num);

    port_handle = CreateFile(port_name.c_str(),
        GENERIC_READ | GENERIC_WRITE, // access mode
        0,                            // share mode, not share
        NULL,                         // security, usually NULL
        OPEN_EXISTING,                // means the serial port must exist, or create fail
        FILE_FLAG_OVERLAPPED,         // overlapped IO
        NULL);

    if (port_handle == INVALID_HANDLE_VALUE)
        return false;

    return true;
}

int win_serial::serial_config(unsigned int baud, parity parity, unsigned int data_bits, stop_bits stop_bits,
    flow_control flow_control)
{
    DCB dcb = { 0 };
    COMMTIMEOUTS timeout;

    if (!GetCommState(port_handle, &dcb))
        return SERIAL_GET_STATE_FAILED;

    // configure serial port
    dcb.BaudRate = baud;
    dcb.StopBits = stop_bits;
    dcb.Parity = parity;
    dcb.ByteSize = data_bits;
    dcb.fDsrSensitivity = FALSE;
    switch (flow_control) {
        case flow_control::no_flow_control:
            dcb.fOutxCtsFlow = FALSE;
            dcb.fOutxDsrFlow = FALSE;
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
            break;
        case flow_control::cts_rts_control:
            dcb.fOutxCtsFlow = TRUE;
            dcb.fOutxDsrFlow = FALSE;
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
            break;
        case flow_control::cts_dtr_control:
            dcb.fOutxCtsFlow = TRUE;
            dcb.fOutxDsrFlow = FALSE;
            dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
            break;
        case flow_control::dsr_rts_control:
            dcb.fOutxCtsFlow = FALSE;
            dcb.fOutxDsrFlow = TRUE;
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
            break;
        case flow_control::dsr_dtr_control:
            dcb.fOutxCtsFlow = FALSE;
            dcb.fOutxDsrFlow = TRUE;
            dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
            break;
        case flow_control::xon_xoff_control:
            dcb.fOutxCtsFlow = FALSE;
            dcb.fOutxDsrFlow = FALSE;
            dcb.fOutX = TRUE;
            dcb.fInX = TRUE;
            dcb.XonChar = 0x11;
            dcb.XoffChar = 0x13;
            dcb.XoffLim = 100;
            dcb.XonLim = 100;
            break;
        default:
            return SERIAL_ERR_FLOW_CONTROL;
    }

    if (!SetCommState(port_handle, &dcb))
        return SERIAL_CONFIG_FAILED;

    // set serial port expire time, all 0 means no expire
    timeout.ReadIntervalTimeout = 0;
    timeout.ReadTotalTimeoutMultiplier = 0;
    timeout.ReadTotalTimeoutConstant = 0;
    timeout.WriteTotalTimeoutMultiplier = 0;
    timeout.WriteTotalTimeoutConstant = 0;

    if (!SetCommTimeouts(port_handle, &timeout))
        return SERIAL_CONFIG_FAILED;

    return 0;
}

bool win_serial::write(unsigned char *buf, int len)
{
    if (port_handle == INVALID_HANDLE_VALUE)
        return false;

    if (len == 0)
        return true;

    serial_data *data = new serial_data(buf, len);
    output_queue.push(data);

    SetEvent(output_thread_ctx.wait_evt);

    return true;
}

void win_serial::register_data_cb(received_cb callback)
{
    input_thread_ctx.rcv_callback = callback;
}
