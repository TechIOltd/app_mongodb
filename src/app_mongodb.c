 /*
 * app_mongodb.c
 *
 * Author: Sokratis Galiatsis [sokratisg] <sokratis@techio.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*! \file
 *
 * \brief Mongodb based CallerID retriever
 *
 * \author Sokratis Galiatsis [sokratisg] <sokratis@techio.com>
 *
 * \arg none, callerid is retrieved automatically from channel
 * \ingroup addons
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1 $")

#include <sys/types.h>
#include "asterisk/config.h"
#include "asterisk/strings.h"
#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/translate.h"
#include "asterisk/image.h"
#include "asterisk/callerid.h"

#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <mongo.h>
#include <bson.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

static char *desc = "MongoDB Interface";
static char *app = "app_mongodb";
static char *config = "app_mongodb.conf";

static struct ast_str *hostname = NULL, *dbname = NULL, *dbcollection = NULL, *dbnamespace = NULL, *dbuser = NULL, *password = NULL;

static int dbport = 0;
static int connected = 0;

AST_MUTEX_DEFINE_STATIC(mongodb_lock);

struct unload_string {
  AST_LIST_ENTRY(unload_string) entry;
  struct ast_str *str;
};

static AST_LIST_HEAD_STATIC(unload_strings, unload_string);

static int _unload_module(int reload)
{
  return ast_unregister_application(app);
  return 0;
}

static int mongodb_callerid(struct ast_channel *chan, char *data)
{	
	const char * ns;
	mongo conn[1];
  char mongo_conn[255];

  int *caller_num;
  const char *cid;

	static int deprecated = 0;

	ast_debug(1, "mongodb: Starting mongodb_callerid.\n");

	mongo_init( &conn );
	if (mongo_client( &conn , ast_str_buffer(hostname), dbport ) != MONGO_OK){
		mongo_destroy( &conn );
		ast_log(LOG_ERROR, "Method: mongodb_log, MongoDB failed to connect.\n");
		connected = 0;
		return -1;
	}

	if (ast_str_strlen(dbuser) != 0 && (mongo_cmd_authenticate(&conn, ast_str_buffer(dbname), ast_str_buffer(dbuser), ast_str_buffer(password)) != MONGO_OK)) {
		mongo_destroy( &conn );
		ast_log(LOG_ERROR, "Method: mongodb_log, MongoDB failed to authenticate to do %s with username %s!\n", ast_str_buffer(dbname), ast_str_buffer(dbuser));
		connected = 0;
		return -1;
	}

	ast_debug(1, "mongodb: Locking mongodb_lock.\n");
	ast_mutex_lock(&mongodb_lock);

	ast_debug(1, "mongodb: Got connection.\n");

	ast_debug(1, "mongodb: Init BSON query.\n");
	bson query[1];
	mongo_cursor cursor[1];

  int callerr = atoi(chan->caller.id.number.str);
	bson_init( query );
	bson_append_int( query, "num", callerr );
	bson_finish( query );

 // TODO: handle error conditions
  strcpy(mongo_conn, ast_str_buffer(dbname));
  strcat(mongo_conn, ".");
  strcat(mongo_conn, ast_str_buffer(dbcollection));

	mongo_cursor_init( cursor, conn, mongo_conn );
	mongo_cursor_set_query( cursor, query );

	bson_iterator iterator[1];
	while( mongo_cursor_next( cursor ) == MONGO_OK ) {
		if ( bson_find( iterator, mongo_cursor_bson( cursor ), "cid" )) {
    		ast_debug(1, "cid: %s\n", bson_iterator_string( iterator ) );
    		ast_log(LOG_ERROR,"cid: %s\n", bson_iterator_string( iterator ) );
        cid = bson_iterator_string( iterator );
		}
	}
	bson_destroy( query );
	mongo_cursor_destroy( cursor );

	connected = 1;

	ast_debug(1, "Unlocking mongodb_lock.\n");
	ast_mutex_unlock(&mongodb_lock);

	/* Set the combined caller id. */
  chan->caller.id.name.valid = 1;
  ast_free(chan->caller.id.name.str);
  chan->caller.id.name.str = ast_strdup(cid);
/*
  chan->caller.id.number.valid = 1;
  ast_free(chan->caller.id.number.str);
  chan->caller.id.number.str = ast_strdup(cid_num);
*/
	return 0;
}

static int load_config_string(struct ast_config *cfg, const char *category, const char *variable, struct ast_str **field, const char *def)
{
	struct unload_string *us;
	const char *tmp;

	if (!(us = ast_calloc(1, sizeof(*us)))) {
		return -1;
	}

	if (!(*field = ast_str_create(16))) {
		ast_free(us);
		return -1;
	}

	tmp = ast_variable_retrieve(cfg, category, variable);

	ast_str_set(field, 0, "%s", tmp ? tmp : def);

	us->str = *field;

/*
	AST_LIST_LOCK(&unload_string);
	AST_LIST_INSERT_HEAD(&unload_string, us, entry);
	AST_LIST_UNLOCK(&unload_string);
*/

	return 0;
}

static int load_config_number(struct ast_config *cfg, const char *category, const char *variable, int *field, int def)
{
  const char *tmp;

  tmp = ast_variable_retrieve(cfg, category, variable);

  if (!tmp || sscanf(tmp, "%d", field) < 1) {
    *field = def;
  }

  return 0;
}

static int _load_module(int reload)
{
    int res;
	struct ast_config *cfg;
	struct ast_variable *var;
	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };

	mongo conn[1];
	bson b[1];

	ast_debug(1, "Starting mongodb_callerid module load.\n");
	ast_debug(1, "Loading mongodb_callerid Config.\n");

	if (!(cfg = ast_config_load(config, config_flags)) || cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_WARNING, "Unable to load config for mongodb CallerID Backend: %s\n", config);
		return AST_MODULE_LOAD_SUCCESS;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		return AST_MODULE_LOAD_SUCCESS;
	}


	if (reload) {
		_unload_module(1);
	}

	ast_debug(1, "Browsing mongodb Global.\n");
	var = ast_variable_browse(cfg, "global");
	if (!var) {
		return AST_MODULE_LOAD_SUCCESS;
	}

	res = 0;

	res |= load_config_string(cfg, "global", "hostname", &hostname, "localhost");
	res |= load_config_string(cfg, "global", "dbname", &dbname, "asterisk");
	res |= load_config_string(cfg, "global", "collection", &dbcollection, "callerid");
	res |= load_config_number(cfg, "global", "port", &dbport, 27017);
	res |= load_config_string(cfg, "global", "username", &dbuser, "");
	res |= load_config_string(cfg, "global", "password", &password, "");

	if (res < 0) {
		return AST_MODULE_LOAD_FAILURE;
	}

	ast_debug(1, "Got hostname of %s\n", ast_str_buffer(hostname));
	ast_debug(1, "Got port of %d\n", dbport);
	ast_debug(1, "Got dbname of %s\n", ast_str_buffer(dbname));
	ast_debug(1, "Got dbcollection of %s\n", ast_str_buffer(dbcollection));
	ast_debug(1, "Got user of %s\n", ast_str_buffer(dbuser));
	ast_debug(1, "Got password of %s\n", ast_str_buffer(password));
	
	
	dbnamespace = ast_str_create(255);
	ast_str_set(&dbnamespace, 0, "%s.%s", ast_str_buffer(dbname), ast_str_buffer(dbcollection));

	if (mongo_client(&conn , ast_str_buffer(hostname), dbport) != MONGO_OK) {
		ast_log(LOG_ERROR, "Method: _load_module, MongoDB failed to connect to %s:%d!\n", ast_str_buffer(hostname), dbport);
		res = -1;
	} else {
		if (ast_str_strlen(dbuser) != 0 && (mongo_cmd_authenticate(&conn, ast_str_buffer(dbname), ast_str_buffer(dbuser), ast_str_buffer(password)) != MONGO_OK)) {
			ast_log(LOG_ERROR, "Method: _load_module, MongoDB failed to authenticate to do %s with username %s!\n", ast_str_buffer(dbname), ast_str_buffer(dbuser));
			res = -1;
		} else {
			connected = 1;
		}
		
		mongo_destroy(&conn);
	}

	ast_config_destroy(cfg);

	res = ast_register_application(app, mongodb_callerid, NULL, NULL);
	if (res) {
		ast_log(LOG_ERROR, "Unable to register MongoDB CallerID Backend\n");
	}

	return res;
}

static int load_module(void)
{
	return _load_module(0);
}

static int unload_module(void)
{
	return _unload_module(0);
}

static int reload(void)
{
	return _load_module(1);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Get CallerID from MongoDB Application");
