#ifndef _SERIAL_FACTORY_H_
#define _SERIAL_FACTORY_H_

#include "serial_intf.h"

#ifdef WIN32
#include "windows/win_serial.h"
#endif

class serial_factory {
public:
	static std::unique_ptr<serial_intf> create_serial(void) {
#ifdef WIN32
		return std::unique_ptr<serial_intf>((serial_intf*)new win_serial());
#else
		return nullptr;
#endif
	}
};

#endif //_SERIAL_FACTORY_H_