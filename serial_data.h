#ifndef _SERIAL_DATA
#define _SERIAL_DATA

#include <cstdint>

class serial_data {
public:
	serial_data(const uint8_t *data, const uint32_t len)
	{
		buf = new unsigned char[len];
		memcpy(buf, data, len);
		length = len;
	}

	void clear_self(void)
	{
		length = 0;
		delete buf;
	}

	unsigned int length;
	unsigned char *buf;
};

#endif //_SERIAL_DATA