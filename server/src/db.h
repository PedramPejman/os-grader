#ifndef DB_H
#define DB_H

#include <sqlite3.h> 

#define MAX_SQL_RESPONSE_LEN  1024
#define MAX_SQL_COMMAND_LEN   512
#define MAX_ASSNUM_DIGITS     5
#define MAX_ACK_LEN           8
#define DB_PATH               "db/testing.db"

#define DB_TABLE_NAME         "SUBMISSIONS"
#define DB_COL_USER           "USER"
#define DB_COL_ASSNUM         "ASS_NUM"
#define DB_COL_RESULT         "RESULT"

#define SQL_COL_DELIM         "*"
#define SQL_LINE_DELIM        "\n"
#define NULL_STR              "NULL"


/* Acknowledgment messages */
#define CMP_AOK   "CMP_AOK" /* Compilation succeeded        */
#define CMP_ERR   "CMP_ERR" /* Compilation failed     FATAL */
#define RUN_AOK   "RUN_AOK" /* A run succeeded              */
#define RUN_ERR   "RUN_ERR" /* A run failed           FATAL */
#define TIM_OUT   "TIM_OUT" /* A run timed out        FATAL */
#define CHK_AOK   "CHK_AOK" /* A diff succeeded             */
#define CHK_ERR   "CHK_ERR" /* A diff failed          FATAL */

#define JDG_ERR   "JDG_ERR" /* Solution not accepted        */
#define JDG_AOK   "JDG_AOK" /* Solution accepted            */

#define RCV_AOK   "RCV_AOK" /* Client request received      */
#define DEBUG 1


/*
** To be used as the callback to sqlite3_exec() for SELECT statements.
** Assumes the first argument is a string, which the response is written to, with following format:
** [<USER><SQL_COL_DELIM><ASSIGNMENT NUMBER><SQL_COL_DELIM><STATUS CODE><SQL_LINE_DELIM>]*
** If no rows are returned, it will leave passed_buffer untouched.
** Non-zero return value would cause a SQLITE_ABORT return value for the responsible sqlite3_exec() call. 
*/
static int record_retrieval_callback(void *passed_buffer, int argc, char **argv, char **column_names);

/*
** Opens connection to the supplied database.
** Creates db if it does not exist.
** Returns 1 if an error occured, 0 otherwise.
*/
int open_db(sqlite3 **db, char *db_path);

/*
** Creates an empty submissions table
** Returns 1 if an error occured, 0 otherwise.
*/
int create_table(sqlite3 *db);

/*
** Assuming a valid db connection, insert_record inserts a row
** in DB_TABLE_NAME with the provided values. All arguments must be
** non-NULL. Returns 1 on error and 0 otherwise.
*/
int insert_record(sqlite3 *db, char *user, char *ass_num, char *result);

/*
** Assuming a valid db connection, lookup_user looks up all entries for a user
** in the database and writes the response to response_buffer with following format:
** [<USER><SQL_COL_DELIM><ASSIGNMENT NUMBER><SQL_COL_DELIM><RESULT CODE><SQL_LINE_DELIM>]*
** Returns 1 if any errors occur, otherwise 0.
*/
int lookup_user(sqlite3 *db, char *user, char *response_buffer);

/*
** Assuming a valid db connection, lookup_assignment looks up all entries for an assignment
** in the database and writes the response to response_buffer with following format:
** [<USER><SQL_COL_DELIM><ASSIGNMENT NUMBER><SQL_COL_DELIM><STATUS CODE><SQL_LINE_DELIM>]*
** Returns 1 if any errors occur, otherwise 0.
*/
int lookup_assignment(sqlite3 *db, char *ass_num, char *response_buffer);

/*
** Closes the connection to the database.
** If there are unfinalized prepared statements, the handle
** will become a zombie and 1 is returned. Otherwise 0 is returned.
*/
int close_db(sqlite3 *db);

#endif
