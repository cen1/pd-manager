
#ifndef CONFIGURATION_H
#define CONFIGURATION_H

// output & logging options

// set to 1 if this is a test bot
#define PD_TEST_VERSION 0

// set to 1 if you log from multiple threads (e.g. if you log from MySQL thread, it will use mutex when logging)
#define PD_THREADED_LOGGING 0

#endif
