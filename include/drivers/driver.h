#ifndef __MYOS__DRIVERS__DRIVER_H
#define __MYOS__DRIVERS__DRIVER_H

namespace myos {
    namespace drivers {
        class Driver {
        public:
            Driver();
            ~Driver();

            virtual void Activate();
            virtual int Reset();
            virtual void Deactivate();
        };

        class DriverManger {
        public:
            DriverManger();
            void AddDriver(Driver* );
            void ActivateAll();
        private:
            Driver* drivers[256];
            int numDrivers;
        };
    }
}


#endif