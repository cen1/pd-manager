/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#include "ghost.h"
#include "util.h"
#include "config.h"
#include "didyouknow.h"

#include <signal.h>
#include <stdlib.h>

#ifdef WIN32
  #include <windows.h>
  #include <winsock.h>
#endif

#include <time.h>

#ifndef WIN32
  #include <sys/time.h>
#endif

#ifdef __APPLE__
  #include <mach/mach_time.h>
#endif

string gCFGFile;
CGHost *gGHost = NULL;

void SignalCatcher2(int s)
{
	CONSOLE_Print("[!!!] caught signal " + UTIL_ToString(s) + ", exiting NOW");

	if (gGHost)
	{
		if (gGHost->m_Exiting)
			exit(1);
		else
			gGHost->m_Exiting = true;
	}
	else
		exit(1);
}

void SignalCatcher(int s)
{
	// signal( SIGABRT, SignalCatcher2 );
	signal(SIGINT, SignalCatcher2);

	CONSOLE_Print("[!!!] caught signal " + UTIL_ToString(s) + ", exiting nicely");

	if (gGHost)
		gGHost->m_ExitingNice = true;
	else
		exit(1);
}

//
// main
//
int main(int argc, char **argv) {
	srand( time( NULL ) );

	gCFGFile = "ghost.cfg";

	if( argc > 1 && argv[1] )
		gCFGFile = argv[1];

	// read config file

	CConfig CFG;
	CFG.Read( "default.cfg" );
	CFG.Read( gCFGFile );
	gLogFile = CFG.GetString( "bot_log", string( ) );
	gLogMethod = CFG.GetInt( "bot_logmethod", 1 );

	if(!gLogFile.empty()) {
		if (gLogMethod == 0)
		{
			// log method 0: logging disabled
		}
		else if (gLogMethod == 1) {
			// log method 1: open, append, and close the log for every message
			// this works well on Linux but poorly on Windows, particularly as the log file grows in size
			// the log file can be edited/moved/deleted while GHost++ is running
		}
		else if (gLogMethod == 2) {
			// log method 2: open the log on startup, flush the log for every message, close the log on shutdown
			// the log file CANNOT be edited/moved/deleted while GHost++ is running

			gLog = new ofstream( );
			gLog->open( gLogFile.c_str( ), ios :: app );
		}
	}

	CONSOLE_Print("[GHOST] starting up");

	if (!gLogFile.empty()) {
		if (gLogMethod == 1) {
			CONSOLE_Print("[GHOST] using log method 1, logging is enabled and [" + gLogFile + "] will not be locked");
		}
		else if (gLogMethod == 2) {
			if (gLog->fail()) {
				CONSOLE_Print("[GHOST] using log method 2 but unable to open [" + gLogFile + "] for appending, logging is disabled");
			}
			else {
				CONSOLE_Print("[GHOST] using log method 2, logging is enabled and [" + gLogFile + "] is now locked");
			}
		}
	}
	else {
		CONSOLE_Print("[GHOST] no log file specified, logging is disabled");
	}

	// catch SIGABRT and SIGINT

	// signal( SIGABRT, SignalCatcher );
	signal( SIGINT, SignalCatcher );

#ifndef WIN32
	// disable SIGPIPE since some systems like OS X don't define MSG_NOSIGNAL

	signal( SIGPIPE, SIG_IGN );
#endif

#ifdef WIN32
	// initialize timer resolution
	// attempt to set the resolution as low as possible from 1ms to 5ms

	unsigned int TimerResolution = 0;

    for (unsigned int i = 1; i <= 5; ++i) {
		if ( timeBeginPeriod( i ) == TIMERR_NOERROR ) {
			TimerResolution = i;
			break;
		}
		else if (i < 5) {
			CONSOLE_Print("[GHOST] error setting Windows timer resolution to " + UTIL_ToString(i) + " milliseconds, trying a higher resolution");
		}
		else {
			CONSOLE_Print("[GHOST] error setting Windows timer resolution");
			return 1;
		}
	}

	CONSOLE_Print( "[GHOST] using Windows timer with resolution " + UTIL_ToString( TimerResolution ) + " milliseconds" );
#elif __APPLE__
	// not sure how to get the resolution
#else
	// print the timer resolution

	struct timespec Resolution;

	if (clock_getres(CLOCK_MONOTONIC, &Resolution) == -1) {
		CONSOLE_Print("[GHOST] error getting monotonic timer resolution");
	}
	else {
		CONSOLE_Print("[GHOST] using monotonic timer with resolution " + UTIL_ToString((double)(Resolution.tv_nsec / 1000), 2) + " microseconds");
	}
#endif

#ifdef WIN32
	// initialize winsock

	CONSOLE_Print( "[GHOST] starting winsock" );
	WSADATA wsadata;

	if (WSAStartup(MAKEWORD( 2, 2 ), &wsadata ) != 0) {
		CONSOLE_Print( "[GHOST] error starting winsock" );
		return 1;
	}

	//Increase process priority
	CONSOLE_Print( "[GHOST] setting process priority to \"above normal\"" );
	SetPriorityClass( GetCurrentProcess( ), ABOVE_NORMAL_PRIORITY_CLASS );
#endif

	// initialize ghost
	gGHost = new CGHost(&CFG, gCFGFile);

	while (1) {
		// block for 50ms on all sockets - if you intend to perform any timed actions more frequently you should change this
		// that said it's likely we'll loop more often than this due to there being data waiting on one of the sockets but there aren't any guarantees

		if (gGHost->Update(50000)) {
			break;
		}
	}

	// shutdown ghost
	CONSOLE_Print("[GHOST] shutting down");
	delete gGHost;
	gGHost = NULL;

#ifdef WIN32
	// shutdown winsock
	CONSOLE_Print("[GHOST] shutting down winsock");
	WSACleanup( );

	// shutdown timer
	timeEndPeriod(TimerResolution);
#endif

	if (gLog) {
		if (!gLog->fail()) {
			gLog->close();
		}
		delete gLog;
	}

	return 0;
}
