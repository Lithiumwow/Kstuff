#ifndef SQLITE3_H
#define SQLITE3_H

#define SQLITE_OK 0
#define SQLITE_ROW 100
#define SQLITE_DONE 101
#define SQLITE_OPEN_READWRITE 0x00000002

typedef void sqlite3;
typedef void sqlite3_stmt;

/* Function stubs */
#define sqlite3_prepare_v2(...) SQLITE_OK
#define sqlite3_open_v2(...) SQLITE_OK
#define sqlite3_step(stmt) SQLITE_DONE
#define sqlite3_column_text(stmt, col) ((const unsigned char*)"")
#define sqlite3_finalize(stmt) SQLITE_OK

#endif
