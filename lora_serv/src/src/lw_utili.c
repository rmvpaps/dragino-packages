/*
  ____  ____      _    ____ ___ _   _  ___  
  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 

Description:
    lw_uliti control the sqlite3 database for lorawan configure

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: skerlan

*/

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include "lw_utili.h"

extern char *optarg;  
extern int optind, opterr, optopt; 

#define OPT_ADDGW                         (1)
#define OPT_LISTGW                        (2)
#define OPT_ADDPF                         (3)
#define OPT_LISTPF                        (4)
#define OPT_ADDAPP                        (5)
#define OPT_LISTAPP                       (6)
#define OPT_ADDDEV                        (7)
#define OPT_LISTDEV                       (8)
#define OPT_RX2DR                         (9)
#define OPT_RX2FREQ                       (10)
#define OPT_APPNAME                       (11)
#define OPT_PFNAME                        (12)
#define OPT_PFID                          (13)
#define OPT_LISTMSG                       (14)
#define OPT_DEVADDR                       (15)
#define OPT_DEVEUI                        (16)
#define OPT_APPEUI                        (18)
#define OPT_GWEUI                         (17)

#define CMD_ADDGW                           1
#define CMD_LISTGW                          2
#define CMD_ADDPF                           3
#define CMD_LISTPF                          4
#define CMD_ADDAPP                          5
#define CMD_LISTAPP                         6
#define CMD_ADDDEV                          7
#define CMD_LISTDEV                         8
#define CMD_LISTMSG                         9
#define CMD_UPGW                            10

#define DEBUG_SQL           0
#define DEBUG_ERROR         1

#define DEBUG_STMT(stmt) if (DEBUG_SQL) { sql_debug(stmt);} 

static void show_help();

static void sql_debug(sqlite3_stmt* stmt);

static bool db_step(sqlite3_stmt* stmt, void (*rowcallback)(sqlite3_stmt* stmt, void* data), void* data);

static int app_getopt(struct lw_t *cntx, int argc, char **argv);

static void listgw(sqlite3_stmt *stmt, void* data); 
static void listpf(sqlite3_stmt *stmt, void* data); 
static void listapp(sqlite3_stmt *stmt, void* data); 
static void listdev(sqlite3_stmt *stmt, void* data); 
static void listmsg(sqlite3_stmt *stmt, void* data);

static char *genkey(char *key, uint8_t len);
static char *invert(char *dst, const char *src, uint8_t len);

struct option app_long_options[] = {
    {"help",        no_argument,            0,      'h'},
    {"version",     no_argument,            0,      'v'},

    {"addgw",       required_argument,      0,      OPT_ADDGW},
    {"listgw",            no_argument,      0,      OPT_LISTGW},
    {"upgw",        required_argument,      0,      OPT_UPGW},
    {"pfname",      required_argument,      0,      OPT_PFNAME},
    {"pfid",        required_argument,      0,      OPT_PFID},
    {"addapp",            no_argument,      0,      OPT_ADDAPP},
    {"listapp",           no_argument,      0,      OPT_LISTAPP},
    {"appname",     required_argument,      0,      OPT_APPNAME},
    {"addpf",             no_argument,      0,      OPT_ADDPF},
    {"listpf",            no_argument,      0,      OPT_LISTPF},
    {"rx2dr",       required_argument,      0,      OPT_RX2DR},
    {"rx2freq",     required_argument,      0,      OPT_RX2FREQ},
    {"adddev",            no_argument,      0,      OPT_ADDDEV},
    {"listdev",           no_argument,      0,      OPT_LISTDEV},
    {"listmsg",           no_argument,      0,      OPT_LISTMSG},
    {"devaddr",     required_argument,      0,      OPT_DEVADDR},
    {"deveui",      required_argument,      0,      OPT_DEVEUI},
    {"gweui",       required_argument,      0,      OPT_GWEUI},
    {"appeui",      required_argument,      0,      OPT_APPEUI},
    {0,             0,                      0,      0},
};

bool db_init(const char* dbpath, struct lw_t* cntx) {
    int i, ret;

	ret = sqlite3_open(dbpath, &cntx->db);
	if (ret) {
        MSG("ERROR: Can't open database: %s\n", sqlite3_errmsg(cntx->db));
	    sqlite3_close(cntx->db);
		return false;
	}
#ifdef LG08_LG02
	ret = sqlite3_open(MSGDB, &cntx->msgdb);
	if (ret) {
        MSG("ERROR: Can't open database: %s\n", sqlite3_errmsg(cntx->msgdb));
	    sqlite3_close(cntx->msgdb);
		return false;
	}
#endif
	return true;
}

void db_destroy(struct lw_t* cntx) {
		sqlite3_close(cntx->db);
#ifdef LG08_LG02
		sqlite3_close(cntx->msgdb);
#endif
}

static bool db_step(sqlite3_stmt* stmt, void (*rowcallback)(sqlite3_stmt* stmt, void* data), void* data) {
	int ret;
	while (1) {
		ret = sqlite3_step(stmt);
		if (ret == SQLITE_DONE) {
			return true;
        } else if (ret == SQLITE_ROW) {
			if (rowcallback != NULL)
				rowcallback(stmt, data);
		} else {
			MSG_DEBUG(DEBUG_ERROR, "ERROR~ %s\n", sqlite3_errstr(ret));
			return false;
		}
	}
}

static void sql_debug(sqlite3_stmt* stmt) { 
    char *sql;
    sql = sqlite3_expanded_sql(stmt);
    MSG_DEBUG(DEBUG_SQL, "\nDEBUG~ SQL=(%s)\n", sql);
    sqlite3_free(sql);
}

static void show_help() {
    MSG("\n--------------------------------------------------------------------------------\n");
    MSG("Usage: lw_utili [OPTIONS]\n");
    MSG("\n");
    MSG("-v, --version         show this app version\n");
    MSG("-h, --help            show this help\n");
    MSG("\n--------------------------------------------------------------------------------\n");
    MSG("--addgw    <hex>      add a gweui\n");
    MSG("--listgw              list all gateways status and info, or the gw index by gweui\n");
    MSG("--upgw     <hex>      update the gateway profile\n");
    MSG("--pfname   <string>   profile name use for add or list profile\n");
    MSG("--pfid     <int>      profile id use for add gateway\n");
    MSG("\n");
    MSG("e.g. add a gateway:   lw_utili --addgw A840411B7C5C4150 --pfname dragino\n");
    MSG("                      PFNAME index which profile gateway in use\n");
    MSG("     list gateways:   lw_utili --listgw, this command will print all gateways status and info\n");
    MSG("\n--------------------------------------------------------------------------------\n");
    MSG("--addpf               add profile for gateway\n");
    MSG("--listpf   <string>   list all profiles info, or the profile by profile name\n");
    MSG("--rx2dr    <int>      datarate: 0(SF12BW125)/1(SF11BW125)/2(SF10BW125)/3(SF9BW125)/4(SF8BW125)/5(SF7BW125)\n");
    MSG("--rx2freq  <float>    rx2freque use for join accept downlink\n");
    MSG("\n");
    MSG("e.g. add a profile:   lw_utili --addpf --pfname dragino --rx2dr 5 --rx2freq 8689.525\n");
    MSG("\n--------------------------------------------------------------------------------\n");
    MSG("--addapp   <string>   add a applicate for device register\n");
    MSG("--listapp  <string>   list all applicates info, or the app index by app name\n");
    MSG("--appname  <string>   application name use for add or list application\n");
    MSG("\n");
    MSG("e.g. add a application: lw_utili --addapp --appname dragino\n");
    MSG("\n--------------------------------------------------------------------------------\n");
    MSG("--adddev              add a device return DEVEUI APPEUI and APPKEY\n");
    MSG("--listdev             list all devices info, or the device index by deveui\n");
    MSG("\n");
    MSG("e.g. add a device:    lw_utili --adddev --appname dragino\n");
    MSG("\n--------------------------------------------------------------------------------\n");
    MSG("--listmsg             list all message by devaddr|deveui|gweui|appeui\n");
    MSG("--devaddr  <hex>      devaddr\n"     );
    MSG("--deveui   <hex>      devaddr\n"     );
    MSG("--appeui   <hex>      appaddr\n"     );
    MSG("--gweui    <hex>      gwaddr\n"     );
    MSG("\n");
    MSG("e.g. listmsg by devaddr: lw_utili --listmsg --devaddr aabbccdd\n");
    MSG("\n--------------------------------------------------------------------------------\n");
}

static int app_getopt(struct lw_t *cntx, int argc, char **argv) {
    int ret, index;
    opterr = 0;
    index = 0;

    while (1) {
        ret = getopt_long(argc, argv, ":hv", app_long_options, &index);
        if(ret == -1){
            break;
        }
        switch (ret) {
        case 'v':
            MSG("%s Version: 1.0.0\n", argv[0]);
            return 0;
        case 'h':
            show_help();
            return 0;
        case OPT_ADDGW:
            MSG("ADDGW CMD>>>>>>>>>>>>>>>>>>>>>\n");
            if(optarg != NULL) { 
                if (optarg[0] == '-') {
                    optind--;
                } else {  
                    strncpy(cntx->gweui, optarg, sizeof(cntx->gweui));
                    cntx->cmd = CMD_ADDGW;
                }        
            } else {
                MSG("WARNING~ Need GWEUI input\n");
                return -1;
            }
            break;
        case OPT_LISTGW:
            MSG("LISTGW CMD>>>>>>>>>>>>>>>>>>>>>>\n");
            cntx->cmd = CMD_LISTGW;
            break;
        case OPT_UPGW:
            MSG("UPGW CMD>>>>>>>>>>>>>>>>>>>>>>\n");
            cntx->cmd = CMD_UPGW;
            break;
        case OPT_ADDPF:
            MSG("ADDPF CMD>>>>>>>>>>>>>>>>>>>>>>\n");
            cntx->cmd = CMD_ADDPF;
            break;
        case OPT_LISTPF:
            MSG("LISTPF CMD>>>>>>>>>>>>>>>>>>>>>>\n");
            cntx->cmd = CMD_LISTPF;
            break;
        case OPT_ADDAPP:
            MSG("ADDAPP CMD>>>>>>>>>>>>>>>>>>>>>>\n");
            cntx->cmd = CMD_ADDAPP;
            break;
        case OPT_LISTAPP:
            MSG("LISTAPP CMD>>>>>>>>>>>>>>>>>>>>>>\n");
            cntx->cmd = CMD_LISTAPP;
            break; 
        case OPT_ADDDEV:
            MSG("ADDDEV CMD>>>>>>>>>>>>>>>>>>>>>>\n");
            cntx->cmd = CMD_ADDDEV;
            break; 
        case OPT_LISTDEV:
            MSG("LISTDEV CMD>>>>>>>>>>>>>>>>>>>>>\n");
            cntx->cmd = CMD_LISTDEV;
            break; 
        case OPT_LISTMSG:
            MSG("LISTMSG CMD>>>>>>>>>>>>>>>>>>>>>\n");
            cntx->cmd = CMD_LISTMSG;
            break; 
        case OPT_PFNAME:
            if(optarg != NULL) { 
                if (optarg[0] == '-') {
                    optind--;
                } else {  
                    strncpy(cntx->pfname, optarg, sizeof(cntx->pfname));
                }        
            } else {
                MSG("WARNING~ input profile name!");
                return -1;
            }       
            break;
        case OPT_PFID:
            if(optarg != NULL) { 
                if (optarg[0] == '-') {
                    optind--;
                } else {  
                    cntx->pfid = atoi(optarg);
                }        
            } else {
                MSG("WARNING~ input profile id!");
                return -1;
            }       
            break;
        case OPT_APPNAME:
            if(optarg != NULL) { 
                if (optarg[0] == '-') {
                    optind--;
                } else {  
                    strncpy(cntx->appname, optarg, sizeof(cntx->appname));
                }        
            } else {
                MSG("WARNING~ input appname!");
                return -1;
            }       
            break;
        case OPT_RX2DR:
            if(optarg != NULL) { 
                if (optarg[0] == '-') {
                    optind--;
                } else {  
                    cntx->rx2dr = atoi(optarg);
                }        
            } else {
                MSG("WARNING~ input rx2dr!");
                return -1;
            }       
            break;
        case OPT_RX2FREQ:
            if(optarg != NULL) { 
                if (optarg[0] == '-') {
                    optind--;
                } else {  
                    cntx->rx2freq = atof(optarg);
                }        
            } else {
                MSG("WARNING~ input rx2freq!");
                return -1;
            }       
            break;
        case OPT_DEVADDR:
            if(optarg != NULL) { 
                if (optarg[0] == '-') {
                    optind--;
                } else {  
                    strncpy(cntx->devaddr, optarg, sizeof(cntx->devaddr));
                }        
            } else {
                MSG("WARNING~ need input devaddr\n");
                return -1;
            }       
            break;
        case OPT_GWEUI:
            if(optarg != NULL) { 
                if (optarg[0] == '-') {
                    optind--;
                } else {  
                    strncpy(cntx->gweui, optarg, sizeof(cntx->gweui));
                }        
            } else {
                MSG("WARNING~ Input the GWEUI!\n");
                return -1;
            }       
            break;
        case OPT_DEVEUI:
            if(optarg != NULL) { 
                if (optarg[0] == '-') {
                    optind--;
                } else {  
                    strncpy(cntx->deveui, optarg, sizeof(cntx->deveui));
                }        
            } else {
                MSG("WARNING~ Input the DEVEUI!\n");
                return -1;
            }       
            break;
        case OPT_APPEUI:
            if(optarg != NULL) { 
                if (optarg[0] == '-') {
                    optind--;
                } else {  
                    strncpy(cntx->appeui, optarg, sizeof(cntx->appeui));
                }        
            } else {
                MSG("WARNING~ Input the APPEUI!\n");
                return -1;
            }       
            break;
        case '?':
            MSG("Unknown options\n");
            return -1;
        default:
            show_help();
            return 0;
        }
    }
    return 0;
}

void main(int argc, char *argv[]) {
    char sql[512] = {'\0'};
    struct lw_t cntx = {'\0'};

    db_init(DBPATH, &cntx);

    if (app_getopt(&cntx, argc, argv))
            return;

    switch(cntx.cmd) {
        case CMD_ADDGW:
            if (strlen(cntx.gweui) < 8) {
                MSG("WARNING~ Wrong GWEUI! gweui=%s\n", cntx.gweui);
                goto out;
            }
            
            if (cntx.pfid > 0) {
                snprintf(sql, sizeof(sql), "select id from gwprofile where id = %d", cntx.pfid);
                INITSTMT(sql, cntx.stmt);
                DEBUG_STMT(cntx.stmt);
                int ret = sqlite3_step(cntx.stmt);
                if (ret != SQLITE_ROW) {
                    MSG("\nid:%d not a valid profileid, use --listpf to get a valid profileid\n", cntx.pfid);
                    goto out;
                } 
                snprintf(sql, sizeof(sql), "INSERT OR IGNORE INTO gws (gweui, profileid) VALUES ('%s', %d)", cntx.gweui, cntx.pfid);
                sqlite3_finalize(cntx.stmt);
            } else
                snprintf(sql, sizeof(sql), "INSERT OR IGNORE INTO gws (gweui) VALUES ('%s')", cntx.gweui);

            INITSTMT(sql, cntx.stmt);
            DEBUG_STMT(cntx.stmt);
            db_step(cntx.stmt, NULL, NULL);
            MSG("Add GW complete! GWEUI: %s\n", cntx.gweui);
            break;

        case CMD_LISTGW:
            snprintf(sql, sizeof(sql), "SELECT gweui, profileid, last_seen_at FROM gws ORDER BY profileid");
            INITSTMT(sql, cntx.stmt);
            DEBUG_STMT(cntx.stmt);
            MSG("\n|     GWEUI     |  profileID  |    Last-seen    | \n");
            if (db_step(cntx.stmt, listgw, NULL))
                goto out;
            break;

        case CMD_ADDPF:
            if ((strlen(cntx.pfname) < 1) || cntx.rx2dr < -1 || cntx.rx2dr > 6) {
                MSG("WARNING! Wrong option: pfname=%s, rx2dr=%u, rx2freq=%f\n", cntx.pfname, cntx.rx2dr, cntx.rx2freq);
                goto out;
            }
            snprintf(sql, sizeof(sql), "INSERT OR IGNORE INTO gwprofile (name, rx2datarate, rx2freq) VALUES ('%s', %d, %f)",
                    cntx.pfname, cntx.rx2dr, cntx.rx2freq);
            INITSTMT(sql, cntx.stmt);
            DEBUG_STMT(cntx.stmt);
            db_step(cntx.stmt, NULL, NULL);
            MSG("Add profile complete! name:%s, rx2dr:%u, r2xfreq:%f\n", cntx.pfname, cntx.rx2dr, cntx.rx2freq);
            break;

        case CMD_UPGW:
            if ((strlen(cntx.gweui) > 0) && cntx.pfid > 0) { 
                snprintf(sql, sizeof(sql), "SELECT name FROM gwprofile where id = %d", cntx.pfid);
                INITSTMT(sql, cntx.stmt);
                int ret = sqlite3_step(cntx.stmt);
                if (ret != SQLITE_ROW) {
                    MSG("WARNING~ not a valid profile id , check by lw_utili --listpf\n");
                    goto out;
                }
                sqlite3_finalize(cntx.stmt);
                snprintf(sql, sizeof(sql), "UPDATE OR IGNORE gws SET profileid = %d WHERE gweui = '%s'", cntx.pfid, cntx.gweui);
                INITSTMT(sql, cntx.stmt);
                DEBUG_STMT(cntx.stmt);
                if (db_step(cntx.stmt, NULL, NULL)) 
                    MSG("Update gateway's profile sucessful!\n");
            }

            break;

        case CMD_LISTPF:
            snprintf(sql, sizeof(sql), "SELECT name, rx1delay, rx2datarate, rx2freq FROM gwprofile ORDER BY id");
            INITSTMT(sql, cntx.stmt);
            DEBUG_STMT(cntx.stmt);
            MSG("\n|  name  |  rx1delay  |  r2xdr  |  rx2freq  |\n");
            if (db_step(cntx.stmt, listpf, NULL))
                goto out;
            break;

        case CMD_ADDAPP:
            if (strlen(cntx.appname) < 1) {
                show_help();
                goto out;
            }
            MSG("appname: %s\n", cntx.appname);
            char appeui[17] = {'\0'};
            char appeui_r[17] = {'\0'};
            char appkey[33] = {'\0'};
            
            genkey(appeui, 16);
            genkey(appkey, 32);

            snprintf(sql, sizeof(sql), "INSERT OR IGNORE INTO apps (name, appeui, appkey) VALUES ('%s', '%s', '%s')",
                    cntx.appname, appeui, appkey);
            INITSTMT(sql, cntx.stmt);
            DEBUG_STMT(cntx.stmt);
            if (!db_step(cntx.stmt, NULL, NULL))
                goto out;
            else {
                MSG("\n Add application complete!\n Appname:%s\n APPEUI(lsb):%s\n", cntx.appname, appeui);
                MSG(" APPEUI(msb):%s\n APPKEY:%s\n", invert(appeui_r, appeui, 16), appkey);
            }
            break;

        case CMD_LISTAPP:
            snprintf(sql, sizeof(sql), "SELECT name, appeui, appkey FROM apps");
            INITSTMT(sql, cntx.stmt);
            DEBUG_STMT(cntx.stmt);
            if (db_step(cntx.stmt, listapp, NULL))
                goto out;
            break;

        case CMD_ADDDEV:
            if (strlen(cntx.appname) < 1) {
                show_help();
                goto out;
            }
            snprintf(sql, sizeof(sql), "select appeui, appkey from apps where name = '%s'", cntx.appname);
            INITSTMT(sql, cntx.stmt);
            DEBUG_STMT(cntx.stmt);
            int ret = sqlite3_step(cntx.stmt);
            if (ret != SQLITE_ROW) {
                MSG("\nappname:%s not a valid appname, use --listapp to get a valid appname\n", cntx.appname);
                goto out;
            } else if ( ret == SQLITE_ROW ) {
                char appeui[17] = {'\0'};
                char appeui_r[17] = {'\0'};
                char appkey[33] = {'\0'};
                char deveui[17] = {'\0'};
                char deveui_r[17] = {'\0'};

                strncpy(appeui, sqlite3_column_text(cntx.stmt, 0), sizeof(appeui));
                strncpy(appkey, sqlite3_column_text(cntx.stmt, 1), sizeof(appkey));
                genkey(deveui, 16);

                sqlite3_finalize(cntx.stmt);

                snprintf(sql, sizeof(sql), "insert or ignore into devs (deveui, appid) values ('%s', '%s')", deveui, appeui);
                INITSTMT(sql, cntx.stmt);
                DEBUG_STMT(cntx.stmt);
                db_step(cntx.stmt, NULL, NULL);
                MSG("\n Add device complete!\n DEVEUI(lsb):%s\n APPEUI(lsb):%s\n", deveui, appeui);
                MSG(" DEVEUI(msb):%s\n APPEUI(msb):%s\n", invert(deveui_r, deveui, 16), invert(appeui_r, appeui, 16));
                MSG(" APPKEY:%s\n", appkey);
            } 
                
            break;

        case CMD_LISTDEV:
            snprintf(sql, sizeof(sql), "SELECT deveui, appid, appkey, devaddr FROM devs INNER JOIN apps on devs.appid = apps.appeui");
            INITSTMT(sql, cntx.stmt);
            DEBUG_STMT(cntx.stmt);
            if (db_step(cntx.stmt, listdev, NULL))
                goto out;
            break;

        case CMD_LISTMSG:

            MSG("---------------------------------------------------------------\n");
            char msgcon[17] = {'\0'};
            char colum[16] = {'\0'};

            if (strlen(cntx.devaddr) > 0) { 
                strcpy(msgcon, cntx.devaddr);
                strcpy(colum, "devaddr");
            } else if (strlen(cntx.deveui) > 0) { 
                strcpy(msgcon, cntx.deveui);
                strcpy(colum, "deveui");
            } else if (strlen(cntx.appeui) > 0) {
                strcpy(msgcon, cntx.appeui);
                strcpy(colum, "appeui");
            } else if (strlen(cntx.gweui) > 0) {
                strcpy(msgcon, cntx.gweui);
                strcpy(colum, "gweui");
            } else {
                MSG("WARNING~ NO listmsg condition! Need devaddr or deveui or gweui or appeui!\n");
                goto out;
            }

            snprintf(sql, sizeof(sql), "SELECT fcntup, recvtime, freq, datarate, payload FROM upmsg WHERE '%s' = '%s' ORDER BY fcntup", colum, msgcon);
            INITSTMT(sql, cntx.stmt);
            DEBUG_STMT(cntx.stmt);
            MSG("| id | time | freq | dr | payload |\n");
            db_step(cntx.stmt, listmsg, NULL);
            break;

        default:
            goto out;
    }

out:
    sqlite3_finalize(cntx.stmt);
    db_destroy(&cntx);
    return;

}

static void listgw(sqlite3_stmt *stmt, void* data) {
    char gweui[17] = {'\0'};
    int profileid = 0;
    uint32_t tmst = 0;

    strncpy(gweui, sqlite3_column_text(stmt, 0), sizeof(gweui));
    profileid = sqlite3_column_int(stmt, 1);
    tmst = (uint32_t)sqlite3_column_int(stmt, 2);

    MSG("| %s |  %d  | %lu |\n", gweui, profileid, tmst);

    return;
}

static void listpf(sqlite3_stmt *stmt, void* data) {
    char name[32] = {'\0'};
    int rx1delay = 0;
    uint8_t rx2dr = 0;
    float rx2freq = 0;

    strncpy(name, sqlite3_column_text(stmt, 0), sizeof(name));
    rx1delay = sqlite3_column_int(stmt, 1);
    rx2dr = (uint8_t)sqlite3_column_int(stmt, 2);
    rx2freq = sqlite3_column_double(stmt, 3);

    MSG("|  %s  |  %d  |  %u  |  %f  |\n", name, rx1delay, rx2dr, rx2freq);

    return;
}

static void listapp(sqlite3_stmt *stmt, void* data) {
    char name[32] = {'\0'};
    char appeui[17] = {'\0'};
    char appeui_r[17] = {'\0'};
    char appkey[33] = {'\0'};

    strncpy(name, sqlite3_column_text(stmt, 0), sizeof(name));
    strncpy(appeui, sqlite3_column_text(stmt, 1), sizeof(appeui));
    strncpy(appkey, sqlite3_column_text(stmt, 2), sizeof(appkey));

    MSG("\n------------------------------------------------------------------------------------------------\n");
    MSG(" name :  %s  \n appeui(lsb): %s  \n appeui(msb): %s \n appkey: %s \n", name, appeui, invert(appeui_r, appeui, 16), appkey);
    MSG("------------------------------------------------------------------------------------------------\n");

    return;
}

static void listdev(sqlite3_stmt *stmt, void* data) {
    char deveui[17] = {'\0'};
    char deveui_r[17] = {'\0'};
    char appeui[17] = {'\0'};
    char appeui_r[17] = {'\0'};
    char appkey[33] = {'\0'};
    //char devaddr[9] = {'\0'};

    strncpy(deveui, sqlite3_column_text(stmt, 0), sizeof(deveui));
    strncpy(appeui, sqlite3_column_text(stmt, 1), sizeof(appeui));
    strncpy(appkey, sqlite3_column_text(stmt, 2), sizeof(appkey));

    MSG("\n------------------------------------------------------------------------------------------------\n");
    MSG(" deveui(lsb): %s\n deveui(msb): %s\n appeui(lsb): %s\n appeui(msb): %s\n appkey: %s\n", deveui, invert(deveui_r, deveui, 16), appeui, invert(appeui_r, appeui, 16), appkey);
    MSG("------------------------------------------------------------------------------------------------\n");

    return;
}

static void listmsg(sqlite3_stmt *stmt, void* data) {
    uint32_t fcntup;
    float freq;
    uint8_t dr;
    char time[32] = {'\0'};
    char payload[256] = {'\0'};

    fcntup = (uint32_t)sqlite3_column_int(stmt, 0);
    strncpy(time, sqlite3_column_text(stmt, 1), sizeof(time));
    freq = sqlite3_column_double(stmt, 2);
    dr = (uint8_t)sqlite3_column_int(stmt, 3);
    strncpy(payload, sqlite3_column_text(stmt, 4), sizeof(payload));

    MSG("------------------------------------------------------------------------------------------------\n");
    MSG("| %u | %s | %f | %u | %s |\n", fcntup, time, freq, dr, payload);
    MSG("------------------------------------------------------------------------------------------------\n");

    return;
}

static char *genkey(char *key, uint8_t len) {
    int i;
    static unsigned long next = 1;
    char alpha[16] = {'A', 'B', 'C', 'E', 'F', 'D',
                      '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
    for (i = 0; i < len; i++) {
        next = next * 1103515245 + 12345;
        srand((unsigned int)(time(NULL) + next));
        key[i] = alpha[rand() % 16];
    }
    
    return key;
}

static char *invert(char *dst, const char *src, uint8_t len) {
    int i;
    for (i = 0; i < len; i++) {
        if (i % 2)
            dst[i] = src[len - i];
        else
            dst[i] = src[len - i - 2];
    }

    return dst;

}
