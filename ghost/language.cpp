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
#include "language.h"

//
// CLanguage
//

CLanguage :: CLanguage( string nCFGFile )
{
	m_CFG = new CConfig( );
	m_CFG->Read( nCFGFile );
}

CLanguage :: ~CLanguage( )
{
	delete m_CFG;
}

// patch
string CLanguage :: UnhostGameInLobbyBeforeHostingAgain( )
{
	return m_CFG->GetString( "lang_0401", "lang_0401" );
}

// patch
string CLanguage :: NoBotsAvailableAtTheMoment( )
{
	return m_CFG->GetString( "lang_0402", "lang_0402" );
}

// patch
string CLanguage :: ThereIsNoGamesInProgress( )
{
	return m_CFG->GetString( "lang_0403", "lang_0403" );
}

// patch
/*
string CLanguage :: ThereAreGamesInProgress( string games, string maxGames, string pubs, string privs, string lobbies, string maxLobbies )
{
	string Out = m_CFG->GetString( "lang_0404", "lang_0404" );
	UTIL_Replace( Out, "$CURRENT$", games );
	UTIL_Replace( Out, "$MAX$", maxGames );
	UTIL_Replace( Out, "$PUBLIC$", pubs );
	UTIL_Replace( Out, "$PRIVATE$", privs );
	UTIL_Replace( Out, "$LOBBY$", lobbies );
	UTIL_Replace( Out, "$MAXLOBBY$", maxLobbies );
	return Out;
}
*/

// patch
string CLanguage :: ThereAreGamesInProgress( string games, string maxGames, string ladderGames, string customGames, string lobbies, string maxLobbies )
{
	string Out = m_CFG->GetString( "lang_0404", "lang_0404" );
	UTIL_Replace( Out, "$CURRENT$", games );
	UTIL_Replace( Out, "$MAX$", maxGames );
	UTIL_Replace( Out, "$LADDER$", ladderGames );
	UTIL_Replace( Out, "$CUSTOM$", customGames );
	UTIL_Replace( Out, "$LOBBY$", lobbies );
	UTIL_Replace( Out, "$MAXLOBBY$", maxLobbies );
	return Out;
}

// patch
string CLanguage :: UserYouCantCreateGameBecauseYouAreBanned( string user )
{
	string Out = m_CFG->GetString( "lang_0405", "lang_0405" );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

// patch
string CLanguage :: UserWasBannedOnByBecauseExpiringIn( string victim, string date, string admin, string reason, string remainingTime )
{
	string Out = m_CFG->GetString( "lang_0406", "lang_0406" );
	UTIL_Replace( Out, "$VICTIM$", victim );
	UTIL_Replace( Out, "$DATE$", date );
	UTIL_Replace( Out, "$ADMIN$", admin );
	UTIL_Replace( Out, "$REASON$", reason );
	UTIL_Replace( Out, "$REMAININGTIME$", remainingTime );
	return Out;
}

// patch
string CLanguage :: RemoteGameNumberIs( string number, string description )
{
	string Out = m_CFG->GetString( "lang_0407", "lang_0407" );
	UTIL_Replace( Out, "$NUMBER$", number );
	UTIL_Replace( Out, "$DESCRIPTION$", description );
	return Out;
}

// patch
string CLanguage :: RemoteGameNumberDoesntExist( string number )
{
	string Out = m_CFG->GetString( "lang_0408", "lang_0408" );
	UTIL_Replace( Out, "$NUMBER$", number );
	return Out;
}

// patch
string CLanguage :: RemoteGameInLobbyNumberIs( string number, string description )
{
	string Out = m_CFG->GetString( "lang_0409", "lang_0409" );
	UTIL_Replace( Out, "$NUMBER$", number );
	UTIL_Replace( Out, "$DESCRIPTION$", description );
	return Out;
}

// patch
string CLanguage :: RemoteGameInLobbyNumberDoesntExist( string number )
{
	string Out = m_CFG->GetString( "lang_0410", "lang_0410" );
	UTIL_Replace( Out, "$NUMBER$", number );
	return Out;
}

// patch
string CLanguage :: CreatingPrivateGame( string gamename, string user, string creatorServerAlias, string creatorServer )
{
	string Out = m_CFG->GetString( "lang_0411", "lang_0411" );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	UTIL_Replace( Out, "$USER$", user );
	UTIL_Replace( Out, "$CREATORSERVERALIAS$", creatorServerAlias );
	UTIL_Replace( Out, "$CREATORSERVER$", creatorServer );
	return Out;
}

// patch
string CLanguage :: CreatingPublicGame( string gamename, string user, string creatorServerAlias, string creatorServer )
{
	string Out = m_CFG->GetString( "lang_0412", "lang_0412" );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	UTIL_Replace( Out, "$USER$", user );
	UTIL_Replace( Out, "$CREATORSERVERALIAS$", creatorServerAlias );
	UTIL_Replace( Out, "$CREATORSERVER$", creatorServer );
	return Out;
}

string CLanguage :: UnableToCreateGameTryAnotherName( string server, string gamename )
{
	string Out = m_CFG->GetString( "lang_0001", "lang_0001" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	return Out;
}

string CLanguage :: UserIsAlreadyAnAdmin( string server, string user )
{
	string Out = m_CFG->GetString( "lang_0002", "lang_0002" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: AddedUserToAdminDatabase( string server, string user )
{
	string Out = m_CFG->GetString( "lang_0003", "lang_0003" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: ErrorAddingUserToAdminDatabase( string server, string user )
{
	string Out = m_CFG->GetString( "lang_0004", "lang_0004" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: YouDontHaveAccessToThatCommand( )
{
	return m_CFG->GetString( "lang_0005", "lang_0005" );
}

string CLanguage :: UserIsAlreadyBanned( string server, string victim )
{
	string Out = m_CFG->GetString( "lang_0006", "lang_0006" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$VICTIM$", victim );
	return Out;
}

string CLanguage :: BannedUser( string server, string victim )
{
	string Out = m_CFG->GetString( "lang_0007", "lang_0007" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$VICTIM$", victim );
	return Out;
}

string CLanguage :: ErrorBanningUser( string server, string victim )
{
	string Out = m_CFG->GetString( "lang_0008", "lang_0008" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$VICTIM$", victim );
	return Out;
}

string CLanguage :: UserIsAnAdmin( string server, string user )
{
	string Out = m_CFG->GetString( "lang_0009", "lang_0009" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: UserIsNotAnAdmin( string server, string user )
{
	string Out = m_CFG->GetString( "lang_0010", "lang_0010" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: UserWasBannedOnByBecause( string server, string victim, string date, string admin, string reason )
{
	string Out = m_CFG->GetString( "lang_0011", "lang_0011" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$VICTIM$", victim );
	UTIL_Replace( Out, "$DATE$", date );
	UTIL_Replace( Out, "$ADMIN$", admin );
	UTIL_Replace( Out, "$REASON$", reason );
	return Out;
}

string CLanguage :: UserIsNotBanned( string server, string victim )
{
	string Out = m_CFG->GetString( "lang_0012", "lang_0012" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$VICTIM$", victim );
	return Out;
}

string CLanguage :: ThereAreNoAdmins( string server )
{
	string Out = m_CFG->GetString( "lang_0013", "lang_0013" );
	UTIL_Replace( Out, "$SERVER$", server );
	return Out;
}

string CLanguage :: ThereIsAdmin( string server )
{
	string Out = m_CFG->GetString( "lang_0014", "lang_0014" );
	UTIL_Replace( Out, "$SERVER$", server );
	return Out;
}

string CLanguage :: ThereAreAdmins( string server, string count )
{
	string Out = m_CFG->GetString( "lang_0015", "lang_0015" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$COUNT$", count );
	return Out;
}

string CLanguage :: ThereAreNoBannedUsers( string server )
{
	string Out = m_CFG->GetString( "lang_0016", "lang_0016" );
	UTIL_Replace( Out, "$SERVER$", server );
	return Out;
}

string CLanguage :: ThereIsBannedUser( string server )
{
	string Out = m_CFG->GetString( "lang_0017", "lang_0017" );
	UTIL_Replace( Out, "$SERVER$", server );
	return Out;
}

string CLanguage :: ThereAreBannedUsers( string server, string count )
{
	string Out = m_CFG->GetString( "lang_0018", "lang_0018" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$COUNT$", count );
	return Out;
}

string CLanguage :: YouCantDeleteTheRootAdmin( )
{
	return m_CFG->GetString( "lang_0019", "lang_0019" );
}

string CLanguage :: DeletedUserFromAdminDatabase( string server, string user )
{
	string Out = m_CFG->GetString( "lang_0020", "lang_0020" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: ErrorDeletingUserFromAdminDatabase( string server, string user )
{
	string Out = m_CFG->GetString( "lang_0021", "lang_0021" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

// patch, added server parameter
string CLanguage :: UnbannedUser( string server, string victim )
{
	string Out = m_CFG->GetString( "lang_0022", "lang_0022" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$VICTIM$", victim );
	return Out;
}

string CLanguage :: ErrorUnbanningUser( string victim )
{
	string Out = m_CFG->GetString( "lang_0023", "lang_0023" );
	UTIL_Replace( Out, "$VICTIM$", victim );
	return Out;
}

string CLanguage :: GameNumberIs( string number, string description )
{
	string Out = m_CFG->GetString( "lang_0024", "lang_0024" );
	UTIL_Replace( Out, "$NUMBER$", number );
	UTIL_Replace( Out, "$DESCRIPTION$", description );
	return Out;
}

string CLanguage :: GameNumberDoesntExist( string number )
{
	string Out = m_CFG->GetString( "lang_0025", "lang_0025" );
	UTIL_Replace( Out, "$NUMBER$", number );
	return Out;
}

string CLanguage :: GameIsInTheLobby( string description, string current, string max )
{
	string Out = m_CFG->GetString( "lang_0026", "lang_0026" );
	UTIL_Replace( Out, "$DESCRIPTION$", description );
	UTIL_Replace( Out, "$CURRENT$", current );
	UTIL_Replace( Out, "$MAX$", max );
	return Out;
}

string CLanguage :: ThereIsNoGameInTheLobby( string current, string max )
{
	string Out = m_CFG->GetString( "lang_0027", "lang_0027" );
	UTIL_Replace( Out, "$CURRENT$", current );
	UTIL_Replace( Out, "$MAX$", max );
	return Out;
}

string CLanguage :: UnableToLoadConfigFilesOutside( )
{
	return m_CFG->GetString( "lang_0028", "lang_0028" );
}

string CLanguage :: LoadingConfigFile( string file )
{
	string Out = m_CFG->GetString( "lang_0029", "lang_0029" );
	UTIL_Replace( Out, "$FILE$", file );
	return Out;
}

string CLanguage :: UnableToLoadConfigFileDoesntExist( string file )
{
	string Out = m_CFG->GetString( "lang_0030", "lang_0030" );
	UTIL_Replace( Out, "$FILE$", file );
	return Out;
}

string CLanguage :: CreatingPrivateGame( string gamename, string user )
{
	string Out = m_CFG->GetString( "lang_0031", "lang_0031" );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: CreatingPublicGame( string gamename, string user )
{
	string Out = m_CFG->GetString( "lang_0032", "lang_0032" );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: UnableToUnhostGameCountdownStarted( string description )
{
	string Out = m_CFG->GetString( "lang_0033", "lang_0033" );
	UTIL_Replace( Out, "$DESCRIPTION$", description );
	return Out;
}

string CLanguage :: UnhostingGame( string description )
{
	string Out = m_CFG->GetString( "lang_0034", "lang_0034" );
	UTIL_Replace( Out, "$DESCRIPTION$", description );
	return Out;
}

string CLanguage :: UnableToUnhostGameNoGameInLobby( )
{
	return m_CFG->GetString( "lang_0035", "lang_0035" );
}

// patch
string CLanguage :: Version( string version )
{
	string Out = m_CFG->GetString( "lang_0036", "lang_0036" );
	UTIL_Replace( Out, "$VERSION$", version );
	return Out;
}

string CLanguage :: UnableToCreateGameAnotherGameInLobby( string gamename, string description )
{
	string Out = m_CFG->GetString( "lang_0038", "lang_0038" );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	UTIL_Replace( Out, "$DESCRIPTION$", description );
	return Out;
}

string CLanguage :: UnableToCreateGameMaxGamesReached( string gamename, string max )
{
	string Out = m_CFG->GetString( "lang_0039", "lang_0039" );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	UTIL_Replace( Out, "$MAX$", max );
	return Out;
}

string CLanguage :: GameIsOver( string description )
{
	string Out = m_CFG->GetString( "lang_0040", "lang_0040" );
	UTIL_Replace( Out, "$DESCRIPTION$", description );
	return Out;
}

string CLanguage :: UnableToBanNoMatchesFound( string victim )
{
	string Out = m_CFG->GetString( "lang_0051", "lang_0051" );
	UTIL_Replace( Out, "$VICTIM$", victim );
	return Out;
}

string CLanguage :: PlayerWasBannedByPlayer( string server, string victim, string user )
{
	string Out = m_CFG->GetString( "lang_0052", "lang_0052" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$VICTIM$", victim );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: UnableToBanFoundMoreThanOneMatch( string victim )
{
	string Out = m_CFG->GetString( "lang_0053", "lang_0053" );
	UTIL_Replace( Out, "$VICTIM$", victim );
	return Out;
}

string CLanguage :: UnableToKickNoMatchesFound( string victim )
{
	string Out = m_CFG->GetString( "lang_0055", "lang_0055" );
	UTIL_Replace( Out, "$VICTIM$", victim );
	return Out;
}

string CLanguage :: UnableToKickFoundMoreThanOneMatch( string victim )
{
	string Out = m_CFG->GetString( "lang_0056", "lang_0056" );
	UTIL_Replace( Out, "$VICTIM$", victim );
	return Out;
}

string CLanguage :: HasPlayedGamesWithThisBot( string user, string firstgame, string lastgame, string totalgames, string avgloadingtime, string avgstay )
{
	string Out = m_CFG->GetString( "lang_0061", "lang_0061" );
	UTIL_Replace( Out, "$USER$", user );
	UTIL_Replace( Out, "$FIRSTGAME$", firstgame );
	UTIL_Replace( Out, "$LASTGAME$", lastgame );
	UTIL_Replace( Out, "$TOTALGAMES$", totalgames );
	UTIL_Replace( Out, "$AVGLOADINGTIME$", avgloadingtime );
	UTIL_Replace( Out, "$AVGSTAY$", avgstay );
	return Out;
}

string CLanguage :: HasntPlayedGamesWithThisBot( string user )
{
	string Out = m_CFG->GetString( "lang_0062", "lang_0062" );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: HasPlayedDotAGamesWithThisBot( string user, string totalscore, string totalgames, string totalwins, string totallosses, string totalkills, string totaldeaths, string totalcreepkills, string totalcreepdenies, string totalassists, string totalneutralkills, string avgkills, string avgdeaths, string avgcreepkills, string avgcreepdenies, string avgassists, string avgneutralkills )
{
	string Out = m_CFG->GetString( "lang_0074", "lang_0074" );
	UTIL_Replace( Out, "$USER$", user );
	UTIL_Replace( Out, "$TOTALGAMES$", totalgames );

	UTIL_Replace( Out, "$SCORE$", totalscore );

	UTIL_Replace( Out, "$TOTALWINS$", totalwins );
	UTIL_Replace( Out, "$TOTALLOSSES$", totallosses );
	UTIL_Replace( Out, "$TOTALKILLS$", totalkills );
	UTIL_Replace( Out, "$TOTALDEATHS$", totaldeaths );
	UTIL_Replace( Out, "$TOTALCREEPKILLS$", totalcreepkills );
	UTIL_Replace( Out, "$TOTALCREEPDENIES$", totalcreepdenies );
	UTIL_Replace( Out, "$TOTALASSISTS$", totalassists );
	UTIL_Replace( Out, "$TOTALNEUTRALKILLS$", totalneutralkills );
	/*
	UTIL_Replace( Out, "$TOTALTOWERKILLS$", totaltowerkills );
	UTIL_Replace( Out, "$TOTALRAXKILLS$", totalraxkills );
	UTIL_Replace( Out, "$TOTALCOURIERKILLS$", totalcourierkills );
	*/
	UTIL_Replace( Out, "$AVGKILLS$", avgkills );
	UTIL_Replace( Out, "$AVGDEATHS$", avgdeaths );
	UTIL_Replace( Out, "$AVGCREEPKILLS$", avgcreepkills );
	UTIL_Replace( Out, "$AVGCREEPDENIES$", avgcreepdenies );
	UTIL_Replace( Out, "$AVGASSISTS$", avgassists );
	UTIL_Replace( Out, "$AVGNEUTRALKILLS$", avgneutralkills );
	/*
	UTIL_Replace( Out, "$AVGTOWERKILLS$", avgtowerkills );
	UTIL_Replace( Out, "$AVGRAXKILLS$", avgraxkills );
	UTIL_Replace( Out, "$AVGCOURIERKILLS$", avgcourierkills );
	*/
	return Out;
}

string CLanguage :: HasntPlayedDotAGamesWithThisBot( string user )
{
	string Out = m_CFG->GetString( "lang_0075", "lang_0075" );
	UTIL_Replace( Out, "$USER$", user );
	return Out;
}

string CLanguage :: EndingGame( string description )
{
	string Out = m_CFG->GetString( "lang_0083", "lang_0083" );
	UTIL_Replace( Out, "$DESCRIPTION$", description );
	return Out;
}

string CLanguage :: UnableToLoadConfigFileGameInLobby( )
{
	return m_CFG->GetString( "lang_0088", "lang_0088" );
}

string CLanguage :: AtLeastOneGameActiveUseForceToShutdown( )
{
	return m_CFG->GetString( "lang_0092", "lang_0092" );
}

string CLanguage :: CurrentlyLoadedMapCFGIs( string mapcfg )
{
	string Out = m_CFG->GetString( "lang_0093", "lang_0093" );
	UTIL_Replace( Out, "$MAPCFG$", mapcfg );
	return Out;
}

string CLanguage :: ConnectingToBNET( string server )
{
	string Out = m_CFG->GetString( "lang_0105", "lang_0105" );
	UTIL_Replace( Out, "$SERVER$", server );
	return Out;
}

string CLanguage :: ConnectedToBNET( string server )
{
	string Out = m_CFG->GetString( "lang_0106", "lang_0106" );
	UTIL_Replace( Out, "$SERVER$", server );
	return Out;
}

string CLanguage :: DisconnectedFromBNET( string server )
{
	string Out = m_CFG->GetString( "lang_0107", "lang_0107" );
	UTIL_Replace( Out, "$SERVER$", server );
	return Out;
}

string CLanguage :: LoggedInToBNET( string server )
{
	string Out = m_CFG->GetString( "lang_0108", "lang_0108" );
	UTIL_Replace( Out, "$SERVER$", server );
	return Out;
}

string CLanguage :: BNETGameHostingSucceeded( string server )
{
	string Out = m_CFG->GetString( "lang_0109", "lang_0109" );
	UTIL_Replace( Out, "$SERVER$", server );
	return Out;
}

string CLanguage :: BNETGameHostingFailed( string server, string gamename )
{
	string Out = m_CFG->GetString( "lang_0110", "lang_0110" );
	UTIL_Replace( Out, "$SERVER$", server );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	return Out;
}

string CLanguage :: ConnectingToBNETTimedOut( string server )
{
	string Out = m_CFG->GetString( "lang_0111", "lang_0111" );
	UTIL_Replace( Out, "$SERVER$", server );
	return Out;
}

string CLanguage :: UnableToCreateGameDisabled( string reason )
{
	string Out;
	if( !reason.empty( ) )
	{
		Out = m_CFG->GetString( "lang_0414", "lang_0414" );
		UTIL_Replace( Out, "$REASON$", reason );
	}
	else
		Out = m_CFG->GetString( "lang_0413", "lang_0413" );

	return Out;
}

string CLanguage :: BotDisabled( )
{
	return m_CFG->GetString( "lang_0126", "lang_0126" );
}

string CLanguage :: BotEnabled( )
{
	return m_CFG->GetString( "lang_0127", "lang_0127" );
}

string CLanguage::BotLadderDisabled()
{
	return m_CFG->GetString("lang_0500", "lang_0500");
}

string CLanguage::BotLadderEnabled()
{
	return m_CFG->GetString("lang_0501", "lang_0501");
}

string CLanguage :: UnableToCreateGameInvalidMap( string gamename )
{
	string Out = m_CFG->GetString( "lang_0128", "lang_0128" );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	return Out;
}

string CLanguage :: AutoHostEnabled( )
{
	return m_CFG->GetString( "lang_0134", "lang_0134" );
}

string CLanguage :: AutoHostDisabled( )
{
	return m_CFG->GetString( "lang_0135", "lang_0135" );
}

string CLanguage :: UnableToLoadSaveGamesOutside( )
{
	return m_CFG->GetString( "lang_0136", "lang_0136" );
}

string CLanguage :: UnableToLoadSaveGameGameInLobby( )
{
	return m_CFG->GetString( "lang_0137", "lang_0137" );
}

string CLanguage :: LoadingSaveGame( string file )
{
	string Out = m_CFG->GetString( "lang_0138", "lang_0138" );
	UTIL_Replace( Out, "$FILE$", file );
	return Out;
}

string CLanguage :: UnableToLoadSaveGameDoesntExist( string file )
{
	string Out = m_CFG->GetString( "lang_0139", "lang_0139" );
	UTIL_Replace( Out, "$FILE$", file );
	return Out;
}

string CLanguage :: UnableToCreateGameInvalidSaveGame( string gamename )
{
	string Out = m_CFG->GetString( "lang_0140", "lang_0140" );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	return Out;
}

string CLanguage :: UnableToCreateGameSaveGameMapMismatch( string gamename )
{
	string Out = m_CFG->GetString( "lang_0141", "lang_0141" );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	return Out;
}

string CLanguage :: AutoSaveEnabled( )
{
	return m_CFG->GetString( "lang_0142", "lang_0142" );
}

string CLanguage :: AutoSaveDisabled( )
{
	return m_CFG->GetString( "lang_0143", "lang_0143" );
}

string CLanguage :: UpdatingClanList( )
{
	return m_CFG->GetString( "lang_0150", "lang_0150" );
}

string CLanguage :: UpdatingFriendsList( )
{
	return m_CFG->GetString( "lang_0151", "lang_0151" );
}

string CLanguage :: ErrorListingMaps( )
{
	return m_CFG->GetString( "lang_0171", "lang_0171" );
}

string CLanguage :: FoundMaps( string maps )
{
	string Out = m_CFG->GetString( "lang_0172", "lang_0172" );
	UTIL_Replace( Out, "$MAPS$", maps );
	return Out;
}

string CLanguage :: NoMapsFound( )
{
	return m_CFG->GetString( "lang_0173", "lang_0173" );
}

string CLanguage :: ErrorListingMapConfigs( )
{
	return m_CFG->GetString( "lang_0174", "lang_0174" );
}

string CLanguage :: FoundMapConfigs( string mapconfigs )
{
	string Out = m_CFG->GetString( "lang_0175", "lang_0175" );
	UTIL_Replace( Out, "$MAPCONFIGS$", mapconfigs );
	return Out;
}

string CLanguage :: NoMapConfigsFound( )
{
	return m_CFG->GetString( "lang_0176", "lang_0176" );
}

string CLanguage :: UnableToCreateGameNameTooLong( string gamename )
{
	string Out = m_CFG->GetString( "lang_0113", "lang_0113" );
	UTIL_Replace( Out, "$GAMENAME$", gamename );
	return Out;
}
