
#ifndef PROFILER_H
#define PROFILER_H

// turn profiling on/off => 1/0

//#define ENABLE_PROFILING 1
#define ENABLE_PROFILING 0

#ifdef WIN32
 #include "ms_stdint.h"
#else
 #include <stdint.h>
#endif

#include <time.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

class profile {
private:
	string m_name;
	string m_description;
	unsigned int m_start_ticks;
	unsigned int m_end_ticks;
	bool m_over;

public:
	profile( string name ) : m_name( name ), m_start_ticks( GetTicks( ) ), m_over( false ) { }
	~profile( );

	void add_description( string description ) { m_description = description; }
	void end();
};

inline void profile :: end()
{
	m_over = true;
	m_end_ticks = GetTicks( );

	// logging

	string ProfilerLogFile = "profiler91.log";

	ofstream Log;
	Log.open( ProfilerLogFile.c_str( ), ios :: app );

	if( !Log.fail( ) )
	{
		time_t Now = time( NULL );
		string Time = asctime( localtime( &Now ) );

		// erase the newline

		Time.erase( Time.size( ) - 1 );
		//Log << "[" << Time << "] " << m_start_ticks << " " << m_end_ticks << " " << ( m_end_ticks - m_start_ticks ) << " " << m_name << " " << m_description << endl;
		Log << "[" << Time << "] " << ( m_end_ticks - m_start_ticks ) << " " << m_name << " " << m_description << endl;
		Log.close( );
	}
}

inline profile :: ~profile()
{
	if( !m_over )
		end();
}

#if ENABLE_PROFILING == 0
 //#define PROFILE_SCOPE(profile_name);
 #define PROFILE_SCOPE(profile_name);

 //#define PROFILE(profile_name);
 #define PROFILE(profile_name);

 #define PROFILE_ADD_DESCRIPTION(profile_name, description);
 #define PROFILE_END(profile_name);
#else
 //#define PROFILE_SCOPE(profile_name) profile profile_name = profile("profile_name");
 #define PROFILE_SCOPE(profile_name) profile profile_name = profile(#profile_name);

 //#define PROFILE(profile_name) profile *profile_name = new profile("profile_name");
 #define PROFILE(profile_name) profile profile_name = profile(#profile_name);

 #define PROFILE_ADD_DESCRIPTION(profile_name, description) profile_name.add_description(description);
 #define PROFILE_END(profile_name) profile_name.end();
#endif

#endif
