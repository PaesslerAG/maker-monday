#ifndef __SDS_ABSTRACT_SERIAL_H__
#define __SDS_ABSTRACT_SERIAL_H__

#include <HardwareSerial.h>

namespace Serials {

  struct AbstractSerial {
    virtual void begin(int baudRate) ;
    virtual Stream *getStream();
    virtual void release() {
      // do nothing by default
    }
  };

  struct Hardware: public AbstractSerial {
    Hardware(HardwareSerial &serial): serial(serial) {}

    void begin(int baudRate) {
      serial.begin(baudRate);
    }

    Stream *getStream() {
      return &serial;
    }

    HardwareSerial &serial;
  };

}

#endif // __SDS_ABSTRACT_SERIAL_H__
