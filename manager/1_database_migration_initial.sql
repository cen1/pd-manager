--
-- Table structure for table `new_award`
--
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_award` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `small_icon_ext` varchar(4) NOT NULL DEFAULT '',
  `large_icon_ext` varchar(4) NOT NULL DEFAULT '',
  `name` varchar(100) NOT NULL DEFAULT '',
  `description` varchar(256) NOT NULL DEFAULT '',
  `created_by_user_id` int(11) NOT NULL DEFAULT '0',
  `created_datetime` int(11) NOT NULL DEFAULT '0',
  `modified_by_user_id` int(11) NOT NULL DEFAULT '0',
  `modified_datetime` int(11) NOT NULL DEFAULT '0',
  `users` int(11) NOT NULL DEFAULT '0',
  `times_assigned_count` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=14 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_award_log`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_award_log` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `award_id` int(11) NOT NULL DEFAULT '0',
  `admin_user_id` int(11) NOT NULL DEFAULT '0',
  `datetime` int(11) NOT NULL DEFAULT '0',
  `type` int(11) NOT NULL DEFAULT '0',
  `victim_user_id` int(11) NOT NULL DEFAULT '0',
  `extra_info` varchar(100) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=64 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_award_to_user`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_award_to_user` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `award_id` int(11) NOT NULL DEFAULT '0',
  `user_id` int(11) NOT NULL DEFAULT '0',
  `datetime` int(11) NOT NULL DEFAULT '0',
  `order` int(11) NOT NULL DEFAULT '999',
  `public_description` text,
  PRIMARY KEY (`id`),
  KEY `award_id` (`award_id`),
  KEY `user_id` (`user_id`)
) ENGINE=InnoDB AUTO_INCREMENT=15 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_ban_group`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_ban_group` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `datetime` int(11) NOT NULL DEFAULT '0',
  `duration` int(11) NOT NULL DEFAULT '0',
  `last_warn_points_removed_datetime` int(11) NOT NULL DEFAULT '0',
  `last_warn_datetime` int(11) NOT NULL DEFAULT '0',
  `warn_points` int(11) NOT NULL DEFAULT '0',
  `last_ban_points_removed_datetime` int(11) NOT NULL DEFAULT '0',
  `last_ban_datetime` int(11) NOT NULL DEFAULT '0',
  `ban_points` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=580342 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_bot_membergroup`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_bot_membergroup` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(50) NOT NULL DEFAULT '',
  `description` text,
  `bot_permissiongroup_id` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `bot_permissiongroup_id` (`bot_permissiongroup_id`)
) ENGINE=InnoDB AUTO_INCREMENT=9002 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_bot_membergroup2`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_bot_membergroup2` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `category` int(11) NOT NULL DEFAULT '0',
  `ord` int(11) NOT NULL DEFAULT '0',
  `name` varchar(50) NOT NULL DEFAULT '',
  `description` text,
  `dotadiv2_voucher` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=24 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_bot_membergroup_log`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_bot_membergroup_log` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `datetime` int(11) NOT NULL DEFAULT '0',
  `bot_membergroup_id` int(11) NOT NULL DEFAULT '0',
  `player_id` int(11) NOT NULL DEFAULT '0',
  `admin_user_id` int(11) NOT NULL DEFAULT '0',
  `type` varchar(1) NOT NULL DEFAULT '',
  `description` text,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=995 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_bot_membergroup_to_player`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_bot_membergroup_to_player` (
  `bot_membergroup_id` int(11) NOT NULL DEFAULT '0',
  `player_id` int(11) NOT NULL DEFAULT '0',
  `category` int(11) NOT NULL DEFAULT '0',
  KEY `bot_membergroup_id` (`bot_membergroup_id`),
  KEY `player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_bot_permission`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_bot_permission` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `code` varchar(100) NOT NULL DEFAULT '',
  `description` text,
  `category` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `code` (`code`)
) ENGINE=InnoDB AUTO_INCREMENT=45 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_bot_permission_to_bot_membergroup`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_bot_permission_to_bot_membergroup` (
  `bot_permission_id` int(11) NOT NULL DEFAULT '0',
  `bot_membergroup_id` int(11) NOT NULL DEFAULT '0',
  KEY `bot_permission_id` (`bot_permission_id`),
  KEY `bot_membergroup_id` (`bot_membergroup_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_bot_permissiongroup`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_bot_permissiongroup` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(50) NOT NULL DEFAULT '',
  `description` text,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_contributor`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_contributor` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(15) NOT NULL DEFAULT '',
  `server_id` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `server_id` (`server_id`,`name`)
) ENGINE=InnoDB AUTO_INCREMENT=215 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_country`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_country` (
  `id` varchar(2) NOT NULL DEFAULT '??',
  `name` varchar(50) NOT NULL DEFAULT '??',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_customdotagame`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_customdotagame` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `game_id` int(11) NOT NULL DEFAULT '0',
  `winner` tinyint(4) NOT NULL DEFAULT '0',
  `creeps_spawned_time` int(11) NOT NULL DEFAULT '0',
  `stats_over_time` int(11) NOT NULL DEFAULT '0',
  `min` int(11) NOT NULL DEFAULT '0',
  `sec` int(11) NOT NULL DEFAULT '0',
  `mode1` varchar(50) DEFAULT NULL,
  `mode2` varchar(7) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `gameid` (`game_id`),
  KEY `winner` (`winner`)
) ENGINE=InnoDB AUTO_INCREMENT=41659 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_customdotaplayer`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_customdotaplayer` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `game_id` int(11) NOT NULL DEFAULT '0',
  `color` int(11) NOT NULL DEFAULT '0',
  `level` int(11) NOT NULL DEFAULT '0',
  `kills` int(11) NOT NULL DEFAULT '0',
  `deaths` int(11) NOT NULL DEFAULT '0',
  `creepkills` int(11) NOT NULL DEFAULT '0',
  `creepdenies` int(11) NOT NULL DEFAULT '0',
  `assists` int(11) NOT NULL DEFAULT '0',
  `gold` int(11) NOT NULL DEFAULT '0',
  `neutralkills` int(11) NOT NULL DEFAULT '0',
  `item1` char(4) NOT NULL DEFAULT '',
  `item2` char(4) NOT NULL DEFAULT '',
  `item3` char(4) NOT NULL DEFAULT '',
  `item4` char(4) NOT NULL DEFAULT '',
  `item5` char(4) NOT NULL DEFAULT '',
  `item6` char(4) NOT NULL DEFAULT '',
  `hero` char(4) NOT NULL DEFAULT '',
  `newcolor` int(11) NOT NULL DEFAULT '0',
  `towerkills` int(11) NOT NULL DEFAULT '0',
  `raxkills` int(11) NOT NULL DEFAULT '0',
  `courierkills` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `gameid` (`game_id`,`color`),
  KEY `gameid_2` (`game_id`),
  KEY `colour` (`color`),
  KEY `newcolour` (`newcolor`),
  KEY `hero` (`hero`),
  KEY `item1` (`item1`),
  KEY `item2` (`item2`),
  KEY `item3` (`item3`),
  KEY `item4` (`item4`),
  KEY `item5` (`item5`),
  KEY `item6` (`item6`)
) ENGINE=InnoDB AUTO_INCREMENT=139295 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_div1_stats_dota`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_div1_stats_dota` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `rating` double NOT NULL DEFAULT '1500',
  `highest_rating` double NOT NULL DEFAULT '1500',
  `last_played` int(10) NOT NULL DEFAULT '0',
  `last_played_scored` int(11) NOT NULL DEFAULT '0',
  `games` int(11) NOT NULL DEFAULT '0',
  `games_left` int(11) NOT NULL DEFAULT '0',
  `em_games` int(11) NOT NULL DEFAULT '0',
  `games_observed` int(11) NOT NULL DEFAULT '0',
  `wins` int(11) NOT NULL DEFAULT '0',
  `loses` int(11) NOT NULL DEFAULT '0',
  `draws` int(11) NOT NULL DEFAULT '0',
  `kills` int(11) NOT NULL DEFAULT '0',
  `deaths` int(11) NOT NULL DEFAULT '0',
  `assists` int(11) NOT NULL DEFAULT '0',
  `creepkills` int(11) NOT NULL DEFAULT '0',
  `creepdenies` int(11) NOT NULL DEFAULT '0',
  `neutralkills` int(11) NOT NULL DEFAULT '0',
  `towerkills` int(11) NOT NULL DEFAULT '0',
  `raxkills` int(11) NOT NULL DEFAULT '0',
  `courierkills` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `rating` (`rating`)
) ENGINE=InnoDB AUTO_INCREMENT=203438 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_div1_stats_dota_update_log`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_div1_stats_dota_update_log` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `admin_forum_id` int(11) NOT NULL DEFAULT '0',
  `victim_player_id` int(11) NOT NULL DEFAULT '0',
  `datetime` int(11) NOT NULL DEFAULT '0',
  `description` text COLLATE utf8_unicode_ci,
  `old_rating` int(11) NOT NULL DEFAULT '1500',
  `old_highest_rating` int(11) NOT NULL DEFAULT '1500',
  `old_games` int(11) NOT NULL DEFAULT '0',
  `old_games_left` int(11) NOT NULL DEFAULT '0',
  `old_em_games` int(11) NOT NULL DEFAULT '0',
  `old_games_observed` int(11) NOT NULL DEFAULT '0',
  `old_wins` int(11) NOT NULL DEFAULT '0',
  `old_loses` int(11) NOT NULL DEFAULT '0',
  `old_draws` int(11) NOT NULL DEFAULT '0',
  `old_kills` int(11) NOT NULL DEFAULT '0',
  `old_deaths` int(11) NOT NULL DEFAULT '0',
  `old_assists` int(11) NOT NULL DEFAULT '0',
  `old_creepkills` int(11) NOT NULL DEFAULT '0',
  `old_creepdenies` int(11) NOT NULL DEFAULT '0',
  `old_neutralkills` int(11) NOT NULL DEFAULT '0',
  `old_towerkills` int(11) NOT NULL DEFAULT '0',
  `old_raxkills` int(11) NOT NULL DEFAULT '0',
  `old_courierkills` int(11) NOT NULL DEFAULT '0',
  `new_rating` int(11) NOT NULL DEFAULT '1500',
  `new_highest_rating` int(11) NOT NULL DEFAULT '1500',
  `new_games` int(11) NOT NULL DEFAULT '0',
  `new_games_left` int(11) NOT NULL DEFAULT '0',
  `new_em_games` int(11) NOT NULL DEFAULT '0',
  `new_games_observed` int(11) NOT NULL DEFAULT '0',
  `new_wins` int(11) NOT NULL DEFAULT '0',
  `new_loses` int(11) NOT NULL DEFAULT '0',
  `new_draws` int(11) NOT NULL DEFAULT '0',
  `new_kills` int(11) NOT NULL DEFAULT '0',
  `new_deaths` int(11) NOT NULL DEFAULT '0',
  `new_assists` int(11) NOT NULL DEFAULT '0',
  `new_creepkills` int(11) NOT NULL DEFAULT '0',
  `new_creepdenies` int(11) NOT NULL DEFAULT '0',
  `new_neutralkills` int(11) NOT NULL DEFAULT '0',
  `new_towerkills` int(11) NOT NULL DEFAULT '0',
  `new_raxkills` int(11) NOT NULL DEFAULT '0',
  `new_courierkills` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `rating` (`old_rating`)
) ENGINE=InnoDB AUTO_INCREMENT=554 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_div1dotagame`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_div1dotagame` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `game_id` int(11) NOT NULL DEFAULT '0',
  `winner` int(11) NOT NULL DEFAULT '0',
  `creeps_spawned_time` int(11) NOT NULL DEFAULT '0',
  `stats_over_time` int(11) NOT NULL DEFAULT '0',
  `min` int(11) NOT NULL DEFAULT '0',
  `sec` int(11) NOT NULL DEFAULT '0',
  `mode1` varchar(50) NOT NULL DEFAULT '',
  `mode2` varchar(7) NOT NULL DEFAULT '',
  `ff` tinyint(1) NOT NULL DEFAULT '0',
  `scored` tinyint(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`),
  KEY `gameid` (`game_id`),
  KEY `winner` (`winner`)
) ENGINE=InnoDB AUTO_INCREMENT=4267378 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_div1dotaplayer`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_div1dotaplayer` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `rating_before` int(11) NOT NULL DEFAULT '0',
  `rating_after` int(11) NOT NULL DEFAULT '0',
  `botid` int(11) NOT NULL DEFAULT '0',
  `game_id` int(11) NOT NULL DEFAULT '0',
  `color` int(11) NOT NULL DEFAULT '0',
  `level` int(11) NOT NULL DEFAULT '0',
  `kills` int(11) NOT NULL DEFAULT '0',
  `deaths` int(11) NOT NULL DEFAULT '0',
  `creepkills` int(11) NOT NULL DEFAULT '0',
  `creepdenies` int(11) NOT NULL DEFAULT '0',
  `assists` int(11) NOT NULL DEFAULT '0',
  `gold` int(11) NOT NULL DEFAULT '0',
  `neutralkills` int(11) NOT NULL DEFAULT '0',
  `item1` char(4) NOT NULL DEFAULT '',
  `item2` char(4) NOT NULL DEFAULT '',
  `item3` char(4) NOT NULL DEFAULT '',
  `item4` char(4) NOT NULL DEFAULT '',
  `item5` char(4) NOT NULL DEFAULT '',
  `item6` char(4) NOT NULL DEFAULT '',
  `hero` char(4) NOT NULL DEFAULT '',
  `newcolor` int(11) NOT NULL DEFAULT '0',
  `towerkills` int(11) NOT NULL DEFAULT '0',
  `raxkills` int(11) NOT NULL DEFAULT '0',
  `courierkills` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `gameid` (`game_id`,`color`),
  KEY `gameid_2` (`game_id`),
  KEY `colour` (`color`),
  KEY `newcolour` (`newcolor`),
  KEY `hero` (`hero`),
  KEY `item1` (`item1`),
  KEY `item2` (`item2`),
  KEY `item3` (`item3`),
  KEY `item4` (`item4`),
  KEY `item5` (`item5`),
  KEY `item6` (`item6`)
) ENGINE=InnoDB AUTO_INCREMENT=42647592 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_dotadiv2_game`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_dotadiv2_game` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `game_id` int(11) NOT NULL DEFAULT '0',
  `winner` int(11) NOT NULL DEFAULT '0',
  `creeps_spawned_time` int(11) NOT NULL DEFAULT '0',
  `stats_over_time` int(11) NOT NULL DEFAULT '0',
  `min` int(11) NOT NULL DEFAULT '0',
  `sec` int(11) NOT NULL DEFAULT '0',
  `mode1` varchar(50) NOT NULL DEFAULT '',
  `mode2` varchar(7) NOT NULL DEFAULT '',
  `ff` tinyint(1) NOT NULL DEFAULT '0',
  `scored` tinyint(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`),
  KEY `gameid` (`game_id`),
  KEY `winner` (`winner`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_dotadiv2_gameplayer`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_dotadiv2_gameplayer` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `rating_before` int(11) NOT NULL DEFAULT '0',
  `rating_after` int(11) NOT NULL DEFAULT '0',
  `botid` int(11) NOT NULL DEFAULT '0',
  `game_id` int(11) NOT NULL DEFAULT '0',
  `color` int(11) NOT NULL DEFAULT '0',
  `level` int(11) NOT NULL DEFAULT '0',
  `kills` int(11) NOT NULL DEFAULT '0',
  `deaths` int(11) NOT NULL DEFAULT '0',
  `creepkills` int(11) NOT NULL DEFAULT '0',
  `creepdenies` int(11) NOT NULL DEFAULT '0',
  `assists` int(11) NOT NULL DEFAULT '0',
  `gold` int(11) NOT NULL DEFAULT '0',
  `neutralkills` int(11) NOT NULL DEFAULT '0',
  `item1` char(4) NOT NULL DEFAULT '',
  `item2` char(4) NOT NULL DEFAULT '',
  `item3` char(4) NOT NULL DEFAULT '',
  `item4` char(4) NOT NULL DEFAULT '',
  `item5` char(4) NOT NULL DEFAULT '',
  `item6` char(4) NOT NULL DEFAULT '',
  `hero` char(4) NOT NULL DEFAULT '',
  `newcolor` int(11) NOT NULL DEFAULT '0',
  `towerkills` int(11) NOT NULL DEFAULT '0',
  `raxkills` int(11) NOT NULL DEFAULT '0',
  `courierkills` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `gameid` (`game_id`,`color`),
  KEY `gameid_2` (`game_id`),
  KEY `colour` (`color`),
  KEY `newcolour` (`newcolor`),
  KEY `hero` (`hero`),
  KEY `item1` (`item1`),
  KEY `item2` (`item2`),
  KEY `item3` (`item3`),
  KEY `item4` (`item4`),
  KEY `item5` (`item5`),
  KEY `item6` (`item6`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_dotadiv2_playerinfo`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_dotadiv2_playerinfo` (
  `player_id` int(11) NOT NULL,
  `whisper` varchar(1) NOT NULL DEFAULT '1',
  `vouched` tinyint(1) NOT NULL DEFAULT '0',
  `last_vouched_datetime` int(11) NOT NULL DEFAULT '0',
  `last_vouched_voucher_user_id` int(11) NOT NULL DEFAULT '0',
  `last_unvouched_datetime` int(11) NOT NULL DEFAULT '0',
  `last_unvouched_voucher_user_id` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_dotadiv2_stats`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_dotadiv2_stats` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `rating` double NOT NULL DEFAULT '1500',
  `highest_rating` double NOT NULL DEFAULT '1500',
  `last_played` int(10) NOT NULL DEFAULT '0',
  `last_played_scored` int(11) NOT NULL DEFAULT '0',
  `games` int(11) NOT NULL DEFAULT '0',
  `games_left` int(11) NOT NULL DEFAULT '0',
  `em_games` int(11) NOT NULL DEFAULT '0',
  `games_observed` int(11) NOT NULL DEFAULT '0',
  `wins` int(11) NOT NULL DEFAULT '0',
  `loses` int(11) NOT NULL DEFAULT '0',
  `draws` int(11) NOT NULL DEFAULT '0',
  `kills` int(11) NOT NULL DEFAULT '0',
  `deaths` int(11) NOT NULL DEFAULT '0',
  `assists` int(11) NOT NULL DEFAULT '0',
  `creepkills` int(11) NOT NULL DEFAULT '0',
  `creepdenies` int(11) NOT NULL DEFAULT '0',
  `neutralkills` int(11) NOT NULL DEFAULT '0',
  `towerkills` int(11) NOT NULL DEFAULT '0',
  `raxkills` int(11) NOT NULL DEFAULT '0',
  `courierkills` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `rating` (`rating`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_dotadiv2_vouch_log`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_dotadiv2_vouch_log` (
  `bot_membergroup_log_id` int(11) NOT NULL AUTO_INCREMENT,
  `vouch_request_url` varchar(256) NOT NULL DEFAULT '',
  PRIMARY KEY (`bot_membergroup_log_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_game`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_game` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `map` varchar(100) NOT NULL DEFAULT '',
  `datetime` int(11) NOT NULL DEFAULT '0',
  `gamename` varchar(31) NOT NULL DEFAULT '',
  `duration` int(11) NOT NULL DEFAULT '0',
  `gamestate` int(11) NOT NULL DEFAULT '0',
  `creator_player_id` int(11) NOT NULL DEFAULT '0',
  `creator_server_id` int(11) NOT NULL DEFAULT '0',
  `rmk` tinyint(1) NOT NULL DEFAULT '0',
  `lobbyduration` int(11) NOT NULL DEFAULT '0',
  `game_in_progress` tinyint(1) NOT NULL DEFAULT '0',
  `game_type` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `map` (`map`),
  KEY `creator_player_id` (`creator_player_id`),
  KEY `game_type` (`game_type`),
  KEY `datetime` (`datetime`),
  KEY `game_type_2` (`game_type`,`datetime`)
) ENGINE=InnoDB AUTO_INCREMENT=5442784 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_gameplayer`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_gameplayer` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `game_id` int(11) NOT NULL DEFAULT '0',
  `player_id` int(11) NOT NULL DEFAULT '0',
  `pp_name` varchar(15) NOT NULL DEFAULT '',
  `ip` varchar(15) NOT NULL DEFAULT '',
  `reserved` tinyint(1) NOT NULL DEFAULT '0',
  `loadingtime` int(11) NOT NULL DEFAULT '0',
  `left` int(11) NOT NULL DEFAULT '0',
  `leftreason` varchar(100) NOT NULL DEFAULT '',
  `team` int(11) NOT NULL DEFAULT '0',
  `color` int(11) NOT NULL DEFAULT '0',
  `gproxy` tinyint(1) NOT NULL DEFAULT '0',
  `pp_server_id` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `gameid` (`game_id`),
  KEY `colour` (`color`),
  KEY `name` (`pp_name`),
  KEY `ip` (`ip`),
  KEY `player_id` (`player_id`)
) ENGINE=InnoDB AUTO_INCREMENT=48898479 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_hero`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_hero` (
  `heroid` varchar(4) NOT NULL DEFAULT '',
  `map` varchar(100) NOT NULL DEFAULT '',
  `name` varchar(50) NOT NULL DEFAULT '',
  `icon` varchar(100) NOT NULL DEFAULT '',
  `propernames` varchar(100) NOT NULL DEFAULT '',
  `ubertip` text,
  PRIMARY KEY (`heroid`,`map`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_hero_light`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_hero_light` (
  `heroid` varchar(4) NOT NULL DEFAULT '',
  `name` varchar(50) NOT NULL DEFAULT '',
  PRIMARY KEY (`heroid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_ip_to_country`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_ip_to_country` (
  `ip_from` int(10) unsigned zerofill NOT NULL DEFAULT '0000000000',
  `ip_to` int(10) unsigned zerofill NOT NULL DEFAULT '0000000000',
  `country_code2` char(2) NOT NULL DEFAULT '',
  `country_code3` char(3) NOT NULL DEFAULT '',
  `country_name` varchar(50) NOT NULL DEFAULT '',
  PRIMARY KEY (`ip_from`,`ip_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_item`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_item` (
  `itemid` varchar(4) NOT NULL DEFAULT '',
  `map` varchar(100) NOT NULL DEFAULT '',
  `name` varchar(50) NOT NULL DEFAULT '',
  `icon` varchar(100) NOT NULL DEFAULT '',
  PRIMARY KEY (`itemid`,`map`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_item_light`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_item_light` (
  `itemid` varchar(4) NOT NULL DEFAULT '',
  `name` varchar(50) NOT NULL DEFAULT '',
  PRIMARY KEY (`itemid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_log`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_log` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `ban_group_id` int(11) NOT NULL DEFAULT '0',
  `point_to_id` int(11) NOT NULL DEFAULT '0',
  `admin_user_id` int(11) NOT NULL DEFAULT '0',
  `admin_forum_id` int(11) DEFAULT '0',
  `victim_player_id` int(11) NOT NULL DEFAULT '0',
  `game_id` int(11) NOT NULL DEFAULT '0',
  `datetime` int(11) NOT NULL DEFAULT '0',
  `activates_datetime` int(11) NOT NULL DEFAULT '0',
  `type` int(11) NOT NULL DEFAULT '0',
  `active` tinyint(1) NOT NULL DEFAULT '1',
  `points` int(11) NOT NULL DEFAULT '0',
  `request_url` varchar(100) CHARACTER SET utf8 NOT NULL DEFAULT '',
  `public_description` text CHARACTER SET utf8,
  `private_description` text CHARACTER SET utf8,
  `duration` int(11) NOT NULL DEFAULT '0',
  `rules` varchar(50) CHARACTER SET utf8 NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `ban_group_id` (`ban_group_id`),
  KEY `point_to_id` (`point_to_id`),
  KEY `admin_user_id` (`admin_user_id`),
  KEY `victim_player_id` (`victim_player_id`),
  KEY `type` (`type`),
  KEY `active` (`active`),
  KEY `game_id` (`game_id`)
) ENGINE=InnoDB AUTO_INCREMENT=1790232 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_map`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_map` (
  `id` int(11) NOT NULL DEFAULT '0',
  `md5_checksum` varchar(16) NOT NULL DEFAULT '',
  `size_in_bytes` int(11) NOT NULL DEFAULT '0',
  `uploaded_datetime` int(11) NOT NULL DEFAULT '0',
  `uploaded_by_user_id` int(11) NOT NULL DEFAULT '0',
  `times_played_count` int(11) NOT NULL DEFAULT '0',
  `time_played_seconds` int(11) NOT NULL DEFAULT '0',
  `epicwar_map_page` varchar(255) NOT NULL DEFAULT '',
  `epicwar_download_link` varchar(255) NOT NULL DEFAULT '',
  `mapgnome_map_page` varchar(255) NOT NULL DEFAULT '',
  `mapgnome_download_link` varchar(255) NOT NULL DEFAULT '',
  `hive_map_page` varchar(255) NOT NULL DEFAULT '',
  `hive_download_link` varchar(255) NOT NULL DEFAULT '',
  `official_web_page` varchar(255) NOT NULL DEFAULT '',
  `official_map_page` varchar(255) NOT NULL DEFAULT '',
  `official_download_link` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `md5_checksum` (`md5_checksum`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_map_cfg`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_map_cfg` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `map_id` int(11) NOT NULL DEFAULT '0',
  `default` tinyint(1) NOT NULL DEFAULT '1',
  `times_played_count` int(11) NOT NULL DEFAULT '0',
  `time_played_seconds` int(11) NOT NULL DEFAULT '0',
  `map_path` varchar(255) NOT NULL DEFAULT '',
  `map_localpath` varchar(255) NOT NULL DEFAULT '',
  `map_type` varchar(50) NOT NULL DEFAULT '',
  `map_numplayers` tinyint(4) NOT NULL DEFAULT '0',
  `map_numteams` tinyint(4) NOT NULL DEFAULT '0',
  `map_slot1` varchar(50) NOT NULL DEFAULT '',
  `map_slot2` varchar(50) NOT NULL DEFAULT '',
  `map_slot3` varchar(50) NOT NULL DEFAULT '',
  `map_slot4` varchar(50) NOT NULL DEFAULT '',
  `map_slot5` varchar(50) NOT NULL DEFAULT '',
  `map_slot6` varchar(50) NOT NULL DEFAULT '',
  `map_slot7` varchar(50) NOT NULL DEFAULT '',
  `map_slot8` varchar(50) NOT NULL DEFAULT '',
  `map_slot9` varchar(50) NOT NULL DEFAULT '',
  `map_slot10` varchar(50) NOT NULL DEFAULT '',
  `map_slot11` varchar(50) NOT NULL DEFAULT '',
  `map_slot12` varchar(50) NOT NULL DEFAULT '',
  `map_options` tinyint(4) NOT NULL DEFAULT '0',
  `map_width` varchar(50) NOT NULL DEFAULT '',
  `map_height` varchar(50) NOT NULL DEFAULT '',
  `map_size` varchar(50) NOT NULL DEFAULT '',
  `map_info` varchar(50) NOT NULL DEFAULT '',
  `map_crc` varchar(50) NOT NULL DEFAULT '',
  `map_sha1` varchar(100) NOT NULL DEFAULT '',
  `map_speed` tinyint(4) NOT NULL DEFAULT '3',
  `map_visibility` tinyint(4) NOT NULL DEFAULT '4',
  `map_observers` tinyint(4) NOT NULL DEFAULT '1',
  `map_flags` tinyint(4) NOT NULL DEFAULT '3',
  `map_filter_maker` tinyint(4) NOT NULL DEFAULT '1',
  `map_filter_type` tinyint(4) NOT NULL DEFAULT '0',
  `map_filter_size` tinyint(4) NOT NULL DEFAULT '4',
  `map_filter_obs` tinyint(4) NOT NULL DEFAULT '4',
  PRIMARY KEY (`id`),
  KEY `map_id` (`map_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_player`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_player` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(15) NOT NULL DEFAULT '',
  `server_id` int(11) NOT NULL DEFAULT '0',
  `user_id` int(11) NOT NULL DEFAULT '0',
  `ban_group_id` int(11) NOT NULL DEFAULT '0',
  `div1_stats_dota_id` int(11) NOT NULL DEFAULT '0',
  `dota_div2_stats_id` int(11) NOT NULL DEFAULT '0',
  `stats_dota_main` tinyint(1) NOT NULL DEFAULT '1',
  `access_level` int(11) NOT NULL DEFAULT '1',
  `primary_account` tinyint(1) NOT NULL DEFAULT '1',
  `div1_games_count` int(11) NOT NULL DEFAULT '0',
  `dotadiv2_games_count` int(11) NOT NULL DEFAULT '0',
  `custom_games_count` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`,`server_id`),
  KEY `user_id` (`user_id`),
  KEY `access_level` (`access_level`),
  KEY `div1_stats_dota_id` (`div1_stats_dota_id`),
  KEY `dota_div2_stats_id` (`dota_div2_stats_id`)
) ENGINE=InnoDB AUTO_INCREMENT=1115287 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_rule`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_rule` (
  `id` decimal(4,2) NOT NULL DEFAULT '0.00',
  `category` varchar(50) NOT NULL DEFAULT '',
  `description` text,
  `ban_duration_from` int(11) NOT NULL DEFAULT '0',
  `ban_duration_to` int(11) NOT NULL DEFAULT '0',
  `ban_points` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_server`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_server` (
  `id` int(11) NOT NULL DEFAULT '0',
  `alias` varchar(50) NOT NULL DEFAULT '',
  `address` varchar(50) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_user`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_user` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `forum_id` int(11) NOT NULL DEFAULT '0',
  `country_id` varchar(2) NOT NULL DEFAULT '??',
  `access_level` int(11) NOT NULL DEFAULT '1',
  `name` varchar(15) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `access_level` (`access_level`),
  KEY `forum_id` (`forum_id`),
  KEY `country_id` (`country_id`),
  KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=83 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_user2`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_user2` (
  `id` int(11) NOT NULL DEFAULT '0',
  `name` varchar(15) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_web_membergroup`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_web_membergroup` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `ord` int(11) NOT NULL DEFAULT '0',
  `name` varchar(50) NOT NULL DEFAULT '',
  `description` text,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=9002 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_web_permission`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_web_permission` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `cat_id` int(11) NOT NULL DEFAULT '0',
  `ord` int(11) NOT NULL DEFAULT '0',
  `code` varchar(50) NOT NULL DEFAULT '',
  `description` text,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=17 DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_web_permission_to_membergroup`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_web_permission_to_membergroup` (
  `membergroup_id` int(11) NOT NULL DEFAULT '0',
  `permission_id` int(11) NOT NULL DEFAULT '0',
  KEY `membergroup_id` (`membergroup_id`),
  KEY `permission_id` (`permission_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `new_web_permission_to_web_membergroup`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `new_web_permission_to_web_membergroup` (
  `web_membergroup_id` int(11) NOT NULL DEFAULT '0',
  `web_permission_id` int(11) NOT NULL DEFAULT '0',
  KEY `web_membergroup_id` (`web_membergroup_id`),
  KEY `web_permission_id` (`web_permission_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;