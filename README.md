Библиотека SplitUdr
================================================

Компиляция проводилась Visual Studio 2019 Community.

## Пакет SPLIT_UTILS

Пакет SPLIT_UTILS содержит набор хранимых процедур, которые разбивают входной BLOB по
разделителю указангом во втором параметре.

Максимальный размер разделителя 255 байт.
Максимальный размер выходной строки 35535 байт.


```sql
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
```

Вы также можете объявить ещё несколько хранимых процедур с различными входными
и выходными типами данных, если они преобразуемы во внутренние типы данных, т.е.

* IN_TXT BLOB SUB_TYPE TEXT
* IN_SEPARATOR VARCHAR(255) 
* OUT_TXT VARCHAR(35535)

Для тестирования производительности, создадим процедуру генерации чисел.

```sql
CREATE OR ALTER PROCEDURE GEN_ROWS (
    MIN_VAL INTEGER,
    MAX_VAL INTEGER)
RETURNS (
    N INTEGER)
AS
DECLARE VARIABLE XMAX INTEGER;
begin
  n = minvalue(min_val, max_val);
  xMax = maxvalue(min_val, max_val);
  while (n <= xMax) DO
  begin
    suspend;
    n = n + 1;
  end
end
```

Теперь запускаем следующий запрос

```sql
WITH lst
AS (SELECT
        LIST(r.n, ',') AS b
    FROM gen_rows(1, 1000000) r)
SELECT
  MIN(s.out_int) AS min_int,
  MAX(s.out_int) AS max_int
FROM lst
LEFT JOIN SPLIT_UTILS.split_int(lst.b, ',') s ON TRUE
```

```
План
PLAN JOIN (LST R NATURAL, S NATURAL)

------ Информация о производительности ------
Время подготовки запроса = 16ms
Время выполнения запроса = 1s 906ms
Среднее время на получение одной записи = 1 906,00 ms
Current memory = 279 905 968
Max memory = 280 121 104
Memory buffers = 16 384
Reads from disk to cache = 1
Writes from cache to disk = 0
Чтений из кэша = 2 670
```

```sql
WITH lst
AS (SELECT
        LIST(r.n, ',') AS b
    FROM gen_rows(1, 1000000) r)
SELECT
  count(*)
FROM lst
```

```
План
PLAN (LST R NATURAL)

------ Информация о производительности ------
Время подготовки запроса = 16ms
Время выполнения запроса = 813ms
Среднее время на получение одной записи = 813,00 ms
Current memory = 279 880 448
Max memory = 280 145 104
Memory buffers = 16 384
Reads from disk to cache = 0
Writes from cache to disk = 667
Чтений из кэша = 1 998
```

Таким образом BLOB размером 6888895 = 6.8Мбайт был разобран за ~1,1 секунды.

## Процедуры Split_Words

SplitUDR также содержит процедуру разбиения текста на отдельные слова. Она объявлена следующим образом:

```sql
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
```

Параметры:

* IN_TXT - входной текст BLOB или VARCHAR(8191)
* IN_SEPARATORS - список разделителей (строка с символами разделителями), если
не указано, то используются разделители " \n\r\t,.?!:;/\\|<>[]{}()@#$%^&*-+='\"~`"

Пример использования:

```sql
  SELECT
    w.WORD
  FROM DOCS
  LEFT JOIN SPLIT_WORDS(DOCS.CONTENT) w
  WHERE DOCS.DOC_ID = 4
```

## Установка

1. Перенести в каталог plugins/udr файлы splitudr.ddl (windows) или libsplitudr.so (linux) из zip архива
2. Выполнить скрипт регистрации [split-udr.sql](https://github.com/sim1984/split_udr/tree/master/sql/split-udr.sql)

## Скачать

Скачать готовые сборки можно по ссылкам:
* [SplitUdr_Win_x64.zip](https://github.com/sim1984/split_udr/releases/download/1.1/SplitUdr_Win_x64.zip)
* [SplitUdr_Win_x86.zip](https://github.com/sim1984/split_udr/releases/download/1.1/SplitUdr_Win_x86.zip)
* [SplitUdr_Linux_x64.zip](https://github.com/sim1984/split_udr/releases/download/1.1/SplitUdr_Linux_x64.zip)
