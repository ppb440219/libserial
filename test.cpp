#include <iostream>
#include "serial_intf.h"
#include "serial_factory.h"

using namespace std;

void serial_received(uint8_t data) {
	cout << (int)data;
}

int main()
{
	cout << "== Serial port tester ==" << endl << endl;

	serial_intf* serial = serial_factory::create_serial();
	int port_id;
	unsigned char test_data[] = { 0x03, 0x85, 0x04, 0x00, 0x00, 0x00, '\r', '\n' };
	//unsigned char test_data[] = "write something!!";

	if (serial == NULL)
		cout << "get serial instance failed" << endl;

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
	if (!serial->open(port_id, baud_rate::BR_115200, parity::no_parity, 8, stop_bits::one_stop_bits, flow_control::cts_rts_control)) {
		cout << "serial port " << port_id << " fail !" << endl;
		return 0;
	}
	cout << "serial port: " << port_id << " opend" << endl;

	// register received callback
	serial->register_data_cb(serial_received);

	/*for (int i = 0; i < 5; i++) {
		test_data[5] = i;
		serial->write(test_data, sizeof(test_data));
		cout << "write data" << endl;
		Sleep(1500);
	}*/
	int idx = 0;
	while (1) {
		test_data[5] = idx++;
		serial->write(test_data, sizeof(test_data));
		Sleep(100);
	};

	serial->close();

	system("pause");
	return 0;
}

