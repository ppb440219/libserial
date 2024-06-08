#ifdef WIN32

#include <iostream>
#include <string>
#include "win_serial.h"

#define print_hex false
#define NUM_INPUT_HANDLES 2

using namespace std;

win_serial* win_serial::instance = NULL;
CRITICAL_SECTION win_serial::lock;

DWORD write_to_hal(HANDLE h_comm, const unsigned char* buf, DWORD len)
{
    OVERLAPPED osWrite = { 0 };
    DWORD bytes_write;
    DWORD ret;

    osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (osWrite.hEvent == NULL)
        return FALSE;

    if (!WriteFile(h_comm, buf, len, &bytes_write, &osWrite)) {
        if (GetLastError() != ERROR_IO_PENDING)
            ret = GetLastError();
        else {
            DWORD dwRes = WaitForSingleObject(osWrite.hEvent, INFINITE);
            switch (dwRes) {
                case WAIT_OBJECT_0:
                    if (!GetOverlappedResult(h_comm, &osWrite, &bytes_write, FALSE))
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

    CloseHandle(osWrite.hEvent);
    return ret;
}

DWORD WINAPI output_thread_handle(void* param)
{
    struct win_serial::thread_ctx* ctx = (struct win_serial::thread_ctx*)param;

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
                cout << "write err: " << ret << endl;
                //TODO: how to handle error happen
            }
        }
    }

    CloseHandle(ctx->wait_evt);
    cout << "write thread exit" << endl;

    return 0;
}

DWORD WINAPI input_thread_handle(void* param)
{
    struct win_serial::thread_ctx* ctx = (struct win_serial::thread_ctx*)param;
    DWORD bytes_read;
    unsigned char buf;
    HANDLE evt_array[NUM_INPUT_HANDLES];

    OVERLAPPED read_evt = { 0 };
    HANDLE oev = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (oev == NULL) {
        cout << "create input overlapped wait evt failed" << endl;
        return 0;
    }

    evt_array[0] = oev;
    evt_array[1] = ctx->wait_evt;

    while (1) {
        memset(&read_evt, 0, sizeof(read_evt));
        read_evt.hEvent = oev;

        bool readret = ReadFile(ctx->h_comm, &buf, 1, &bytes_read, &read_evt);
        if (!readret && GetLastError() != ERROR_IO_PENDING) {
            cout << "read error:" << GetLastError() << endl;
            break;
        }

        DWORD ret = WaitForMultipleObjects(NUM_INPUT_HANDLES, evt_array, FALSE, INFINITE);
        switch (ret) {
            case WAIT_OBJECT_0:
                readret = GetOverlappedResult(ctx->h_comm, &read_evt, &bytes_read, TRUE);

                if (bytes_read > 0) {
                    if (print_hex)
                        cout << hex << (int)buf << " ";
                    else
                        cout << buf;
                }
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
    cout << "read thread exit" << endl;

    return 0;
}

win_serial* win_serial::get_instance(void)
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

bool win_serial::open(unsigned int port_num, unsigned int baud, parity parity, unsigned int data_bits,
    stop_bits stop_bits, flow_control flow_control)
{
    bool ret = true;
    DWORD out_thread_id, in_thread_id; /* required for Win9x */

    EnterCriticalSection(&win_serial::lock);

    if (!serial_init(port_num)) {
        cout << "init serial port " << port_num << " failed" << endl;
        ret = false;
        goto open_err;
    }

    if (!serial_config(baud, parity, data_bits, stop_bits, flow_control)) {
        cout << "configure serial port " << port_num << " failed" << endl;
        ret = false;
        goto open_err;
    }

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
    string port_name = "COM" + to_string(port_num);

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

bool win_serial::serial_config(unsigned int baud, parity parity, unsigned int data_bits, stop_bits stop_bits,
    flow_control flow_control)
{
    DCB dcb = { 0 };
    COMMTIMEOUTS timeout;

    if (!GetCommState(port_handle, &dcb)) {
        cout << "config get state fail" << endl;
        return false;
    }

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
            cout << "not supported flow control" << endl;
            return false;
    }

    if (!SetCommState(port_handle, &dcb)) {
        cout << "config serial fail" << endl;
        return false;
    }

    // set serial port expire time, all 0 means no expire
    timeout.ReadIntervalTimeout = 0;
    timeout.ReadTotalTimeoutMultiplier = 0;
    timeout.ReadTotalTimeoutConstant = 0;
    timeout.WriteTotalTimeoutMultiplier = 0;
    timeout.WriteTotalTimeoutConstant = 0;

    if (!SetCommTimeouts(port_handle, &timeout)) {
        cout << "config timeout fail" << endl;
        return false;
    }

    return true;
}

bool win_serial::write(unsigned char* buf, int len)
{
    if (port_handle == INVALID_HANDLE_VALUE)
        return false;

    if (len == 0)
        return true;

    serial_data* data = new serial_data(buf, len);
    output_queue.push(data);

    SetEvent(output_thread_ctx.wait_evt);

    return true;
}

#endif //WIN32