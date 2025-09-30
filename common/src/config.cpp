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

#include "config.h"
#include "common_util.h"

#include <fstream>
#include <iostream>
#include <algorithm>

//
// CConfig
//

CConfig :: CConfig( )
{

}

CConfig :: ~CConfig( )
{

}

void CConfig :: Read( const std::string& file )
{
	std::ifstream in;
	in.open( file.c_str( ) );

	if( in.fail( ) )
		std::cout << "[CONFIG] warning - unable to read file [" + file + "]" << std::endl;
	else
	{
	    	std::cout << "[CONFIG] loading file [" + file + "]" << std::endl;
		std::string Line;

		while( !in.eof( ) )
		{
			getline( in, Line );

			// ignore blank lines and comments

			if( Line.empty( ) || Line[0] == '#' )
				continue;

			// remove newlines and partial newlines to help fix issues with Windows formatted config files on Linux systems
			std::erase( Line, '\r' );
			std::erase( Line, '\n' );

			const std::string::size_type Split = Line.find( '=' );

			if( Split == string :: npos )
				continue;

			const std::string::size_type KeyStart = Line.find_first_not_of( ' ' );
			const std::string::size_type KeyEnd = Line.find( ' ', KeyStart );
			const std::string::size_type ValueStart = Line.find_first_not_of( ' ', Split + 1 );
			const std::string::size_type ValueEnd = Line.size( );

			if( ValueStart != string :: npos )
				m_CFG[Line.substr( KeyStart, KeyEnd - KeyStart )] = Line.substr( ValueStart, ValueEnd - ValueStart );
		}

		in.close( );
	}
}

bool CConfig :: Exists( const std::string& key )
{
	return m_CFG.contains( key );
}

int CConfig :: GetInt( const std::string& key, int x )
{
	if( !m_CFG.contains( key ) )
		return x;

	return std::stoi( m_CFG[key] );
}

bool CConfig :: GetBool( const std::string& key, bool x )
{
    if( !m_CFG.contains( key ) )
	return x;

    return GetInt(key, 0) == 1;
}

double CConfig :: GetDouble( const std::string& key, double x )
{
    if( !m_CFG.contains( key ) )
	return x;

    return std::stod(m_CFG[key]);
}

string CConfig :: GetString( const std::string& key, string x )
{
	if( !m_CFG.contains( key ) )
		return x;

	return m_CFG[key];
}

void CConfig :: Set( const std::string& key, string x )
{
	m_CFG[key] = x;
}
