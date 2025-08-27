#include <iostream>
#include "serial_intf.h"
#include "serial_factory.h"

void serial_received(uint8_t data) {
	cout << data;
}

int main()
{
	int ret = 0;
	int port_id;
	//unsigned char test_data[] = { 0x03, 0x85, 0x04, 0x00, 0x00, 0x00, '\r', '\n' };
	unsigned char test_data[] = "hello libserial\r\n";
	std::unique_ptr<serial_intf> serial;
	
	cout << "== libserial test ==" << endl << endl;

	serial = serial_factory::create_serial();
	if (serial == nullptr) {
		cout << "get serial instance failed" << endl;
		return 0;
	}

	// select port
	list<int> ports = serial->get_available_port();
	if (ports.size() <= 0) {
		cout << "no available serial port" << endl;
		return 0;
	}

	cout << "available serial port:" << endl;
	for (auto it = ports.begin(); it != ports.end(); it++)
		cout << "COM" << *it << endl;

	cout << endl << "select serial port:";
	cin >> port_id;

	// open port
	ret = serial->open(port_id, baud_rate::BR_115200, parity::no_parity, 8, stop_bits::one_stop_bits, flow_control::cts_rts_control);
	if (ret < 0) {
		cout << "serial port " << port_id << " fail !  err " << ret << endl;
		return 0;
	}
	cout << "serial port: " << port_id << " opend" << endl;

	// register received callback
	serial->register_data_cb(serial_received);

	for (int i = 0; i < 30; i++) {
		serial->write(test_data, sizeof(test_data));
		Sleep(500);
	};

	serial->close();
	cout << "serial port " << port_id << " closed" << endl;

	system("pause");
	return 0;
}
