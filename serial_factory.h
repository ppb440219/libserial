#ifndef _SERIAL_FACTORY_H_
#define _SERIAL_FACTORY_H_

#include "serial_intf.h"

#ifdef WIN32
#include "windows/win_serial.h"
#endif

class serial_factory {
public:
	static serial_intf* create_serial(void) {
#ifdef WIN32
		return (serial_intf*)win_serial::get_instance();
#else
		return NULL;
#endif
	}
};

#endif //_SERIAL_FACTORY_H_