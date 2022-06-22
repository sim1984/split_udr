SET TERM ^ ;

CREATE OR ALTER PACKAGE SPLIT_UTILS
AS
BEGIN
  PROCEDURE SPLIT_BOOLEAN (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_BOOLEAN BOOLEAN);

  PROCEDURE SPLIT_SMALLINT (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_INT SMALLINT);

  PROCEDURE SPLIT_INT (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_INT INT);

  PROCEDURE SPLIT_BIGINT (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_INT BIGINT);

  PROCEDURE SPLIT_STR (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_STR VARCHAR(8191));

  PROCEDURE SPLIT_DATE (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_DATE DATE);

  PROCEDURE SPLIT_TIME (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_TIME TIME);

  PROCEDURE SPLIT_TIMESTAMP (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_TIMESTAMP TIMESTAMP);

  PROCEDURE SPLIT_DOUBLE (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_DOUBLE DOUBLE PRECISION);
END^

RECREATE PACKAGE BODY SPLIT_UTILS
AS
BEGIN
  PROCEDURE SPLIT_BOOLEAN (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_BOOLEAN BOOLEAN)
  EXTERNAL NAME 'splitudr!split' ENGINE UDR;

  PROCEDURE SPLIT_SMALLINT (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_INT SMALLINT)
  EXTERNAL NAME 'splitudr!split' ENGINE UDR;

  PROCEDURE SPLIT_INT (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_INT INT)
  EXTERNAL NAME 'splitudr!split' ENGINE UDR;

  PROCEDURE SPLIT_BIGINT (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_INT BIGINT)
  EXTERNAL NAME 'splitudr!split' ENGINE UDR;

  PROCEDURE SPLIT_STR (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_STR VARCHAR(8191))
  EXTERNAL NAME 'splitudr!split' ENGINE UDR;

  PROCEDURE SPLIT_DATE (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_DATE DATE)
  EXTERNAL NAME 'splitudr!split' ENGINE UDR;

  PROCEDURE SPLIT_TIME (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_TIME TIME)
  EXTERNAL NAME 'splitudr!split' ENGINE UDR;

  PROCEDURE SPLIT_TIMESTAMP (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_TIMESTAMP TIMESTAMP)
  EXTERNAL NAME 'splitudr!split' ENGINE UDR;

  PROCEDURE SPLIT_DOUBLE (
      IN_TXT       BLOB SUB_TYPE TEXT,
      IN_SEPARATOR VARCHAR(31))
  RETURNS (
      OUT_DOUBLE DOUBLE PRECISION)
  EXTERNAL NAME 'splitudr!split' ENGINE UDR;
END
^

SET TERM ; ^

CREATE OR ALTER PROCEDURE SPLIT_WORDS (
    IN_TXT        BLOB SUB_TYPE TEXT CHARACTER SET UTF8,
    IN_SEPARATORS VARCHAR(50) CHARACTER SET UTF8 DEFAULT NULL)
RETURNS (
    WORD VARCHAR(8191) CHARACTER SET UTF8)
EXTERNAL NAME 'splitudr!strtok' ENGINE UDR;

CREATE OR ALTER PROCEDURE SPLIT_WORDS_S (
    IN_TXT        VARCHAR(8191) CHARACTER SET UTF8,
    IN_SEPARATORS VARCHAR(50) CHARACTER SET UTF8 DEFAULT NULL)
RETURNS (
    WORD VARCHAR(8191) CHARACTER SET UTF8)
EXTERNAL NAME 'splitudr!strtok_s' ENGINE UDR;