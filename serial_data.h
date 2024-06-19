#ifndef _SERIAL_DATA
#define _SERIAL_DATA

class serial_data {
public:
	serial_data(const unsigned char *data, unsigned int len)
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