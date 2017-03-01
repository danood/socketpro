
from spa.clientside.asyncdbhandler import CAsyncDBHandler
from spa import BaseServiceID, tagBaseRequestID

class CSqlite(CAsyncDBHandler):
    sidSqlite = BaseServiceID.sidReserved + 0x6FFFFFF0 #asynchronous sqlite service id

    SQLITE_OK = 0 # Successful result 

    #beginning-of-error-codes
    SQLITE_ERROR = 1 # SQL error or missing database 
    SQLITE_INTERNAL = 2 # Internal logic error in SQLite 
    SQLITE_PERM = 3 # Access permission denied 
    SQLITE_ABORT = 4 # Callback routine requested an abort 
    SQLITE_BUSY = 5 # The database file is locked 
    SQLITE_LOCKED = 6 # A table in the database is locked 
    SQLITE_NOMEM = 7 # A malloc() failed 
    SQLITE_READONLY = 8 # Attempt to write a readonly database 
    SQLITE_INTERRUPT = 9 # Operation terminated by sqlite3_interrupt()
    SQLITE_IOERR = 10 # Some kind of disk I/O error occurred 
    SQLITE_CORRUPT = 11 # The database disk image is malformed 
    SQLITE_NOTFOUND = 12 # Unknown opcode in sqlite3_file_control() 
    SQLITE_FULL = 13 # Insertion failed because database is full 
    SQLITE_CANTOPEN = 14 # Unable to open the database file 
    SQLITE_PROTOCOL = 15 # Database lock protocol error 
    SQLITE_EMPTY = 16 # Database is empty 
    SQLITE_SCHEMA = 17 # The database schema changed 
    SQLITE_TOOBIG = 18 # String or BLOB exceeds size limit 
    SQLITE_CONSTRAINT = 19 # Abort due to constraint violation 
    SQLITE_MISMATCH = 20 # Data type mismatch 
    SQLITE_MISUSE = 21 # Library used incorrectly 
    SQLITE_NOLFS = 22 # Uses OS features not supported on host 
    SQLITE_AUTH = 23 # Authorization denied 
    SQLITE_FORMAT = 24 # Auxiliary database format error 
    SQLITE_RANGE = 25 # 2nd parameter to sqlite3_bind out of range 
    SQLITE_NOTADB = 26 # File opened that is not a database file 
    SQLITE_NOTICE = 27 # Notifications from sqlite3_log() 
    SQLITE_WARNING = 28 # Warnings from sqlite3_log() 
    SQLITE_ROW = 100 # sqlite3_step() has another row ready 
    SQLITE_DONE = 101 # sqlite3_step() has finished executing 
    
    #error codes from asynchronous sqlite server side implementation
    SQLITE_DB_NOT_OPENED_YET = 131
    SQLITE_BAD_END_TRANSTACTION_PLAN = 132
    SQLITE_NO_PARAMETER_SPECIFIED = 133
    SQLITE_BAD_PARAMETER_COLUMN_SIZE = 134
    SQLITE_BAD_PARAMETER_DATA_ARRAY_SIZE = 135
    SQLITE_DATA_TYPE_NOT_SUPPORTED = 136
    SQLITE_NO_DB_FILE_SPECIFIED = 137
    
    #sqlite extended error codes
    SQLITE_IOERR_READ = (SQLITE_IOERR | (1 << 8))
    SQLITE_IOERR_SHORT_READ = (SQLITE_IOERR | (2 << 8))
    SQLITE_IOERR_WRITE = (SQLITE_IOERR | (3 << 8))
    SQLITE_IOERR_FSYNC = (SQLITE_IOERR | (4 << 8))
    SQLITE_IOERR_DIR_FSYNC = (SQLITE_IOERR | (5 << 8))
    SQLITE_IOERR_TRUNCATE = (SQLITE_IOERR | (6 << 8))
    SQLITE_IOERR_FSTAT = (SQLITE_IOERR | (7 << 8))
    SQLITE_IOERR_UNLOCK = (SQLITE_IOERR | (8 << 8))
    SQLITE_IOERR_RDLOCK = (SQLITE_IOERR | (9 << 8))
    SQLITE_IOERR_DELETE = (SQLITE_IOERR | (10 << 8))
    SQLITE_IOERR_BLOCKED = (SQLITE_IOERR | (11 << 8))
    SQLITE_IOERR_NOMEM = (SQLITE_IOERR | (12 << 8))
    SQLITE_IOERR_ACCESS = (SQLITE_IOERR | (13 << 8))
    SQLITE_IOERR_CHECKRESERVEDLOCK = (SQLITE_IOERR | (14 << 8))
    SQLITE_IOERR_LOCK = (SQLITE_IOERR | (15 << 8))
    SQLITE_IOERR_CLOSE = (SQLITE_IOERR | (16 << 8))
    SQLITE_IOERR_DIR_CLOSE = (SQLITE_IOERR | (17 << 8))
    SQLITE_IOERR_SHMOPEN = (SQLITE_IOERR | (18 << 8))
    SQLITE_IOERR_SHMSIZE = (SQLITE_IOERR | (19 << 8))
    SQLITE_IOERR_SHMLOCK = (SQLITE_IOERR | (20 << 8))
    SQLITE_IOERR_SHMMAP = (SQLITE_IOERR | (21 << 8))
    SQLITE_IOERR_SEEK = (SQLITE_IOERR | (22 << 8))
    SQLITE_IOERR_DELETE_NOENT = (SQLITE_IOERR | (23 << 8))
    SQLITE_IOERR_MMAP = (SQLITE_IOERR | (24 << 8))
    SQLITE_IOERR_GETTEMPPATH = (SQLITE_IOERR | (25 << 8))
    SQLITE_IOERR_CONVPATH = (SQLITE_IOERR | (26 << 8))
    SQLITE_IOERR_VNODE = (SQLITE_IOERR | (27 << 8))
    SQLITE_IOERR_AUTH = (SQLITE_IOERR | (28 << 8))
    SQLITE_LOCKED_SHAREDCACHE = (SQLITE_LOCKED | (1 << 8))
    SQLITE_BUSY_RECOVERY = (SQLITE_BUSY | (1 << 8))
    SQLITE_BUSY_SNAPSHOT = (SQLITE_BUSY | (2 << 8))
    SQLITE_CANTOPEN_NOTEMPDIR = (SQLITE_CANTOPEN | (1 << 8))
    SQLITE_CANTOPEN_ISDIR = (SQLITE_CANTOPEN | (2 << 8))
    SQLITE_CANTOPEN_FULLPATH = (SQLITE_CANTOPEN | (3 << 8))
    SQLITE_CANTOPEN_CONVPATH = (SQLITE_CANTOPEN | (4 << 8))
    SQLITE_CORRUPT_VTAB = (SQLITE_CORRUPT | (1 << 8))
    SQLITE_READONLY_RECOVERY = (SQLITE_READONLY | (1 << 8))
    SQLITE_READONLY_CANTLOCK = (SQLITE_READONLY | (2 << 8))
    SQLITE_READONLY_ROLLBACK = (SQLITE_READONLY | (3 << 8))
    SQLITE_READONLY_DBMOVED = (SQLITE_READONLY | (4 << 8))
    SQLITE_ABORT_ROLLBACK = (SQLITE_ABORT | (2 << 8))
    SQLITE_CONSTRAINT_CHECK = (SQLITE_CONSTRAINT | (1 << 8))
    SQLITE_CONSTRAINT_COMMITHOOK = (SQLITE_CONSTRAINT | (2 << 8))
    SQLITE_CONSTRAINT_FOREIGNKEY = (SQLITE_CONSTRAINT | (3 << 8))
    SQLITE_CONSTRAINT_FUNCTION = (SQLITE_CONSTRAINT | (4 << 8))
    SQLITE_CONSTRAINT_NOTNULL = (SQLITE_CONSTRAINT | (5 << 8))
    SQLITE_CONSTRAINT_PRIMARYKEY = (SQLITE_CONSTRAINT | (6 << 8))
    SQLITE_CONSTRAINT_TRIGGER = (SQLITE_CONSTRAINT | (7 << 8))
    SQLITE_CONSTRAINT_UNIQUE = (SQLITE_CONSTRAINT | (8 << 8))
    SQLITE_CONSTRAINT_VTAB = (SQLITE_CONSTRAINT | (9 << 8))
    SQLITE_CONSTRAINT_ROWID = (SQLITE_CONSTRAINT | (10 << 8))
    SQLITE_NOTICE_RECOVER_WAL = (SQLITE_NOTICE | (1 << 8))
    SQLITE_NOTICE_RECOVER_ROLLBACK = (SQLITE_NOTICE | (2 << 8))
    SQLITE_WARNING_AUTOINDEX = (SQLITE_WARNING | (1 << 8))
    SQLITE_AUTH_USER = (SQLITE_AUTH | (1 << 8))

    def __init__(self):
        super(CSqlite, self).__init__(CSqlite.sidSqlite)
