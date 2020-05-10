### required
libbdb

для указания общей директории бд:
    kenv kcs.bdb.path="путь до директории"

bdb_open(string $dbname [, string $dir]);
	описание:
		открыть подключение к базе данных
	параметры:
		$dbname - имя БД
		$dir - папка где лежит бд
	возврат:
		false/resource

bdb_close(resource $db);
	описание:
		закрыть подключение к базе данных
	параметры:
		$db - подключение к базе данных
	возврат:
		true/false

bdb_get(resource $db, int key);
	описание:
		получить значение из бд
	параметры:
		$db - подключение к базе данных
		$key - ключ
	возврат:
		false или массив данных array(
			'column' => 'value'
			...
		)

bdb_set(resource $db, int key, array $data);
	описание:
		добавить/изменить значение в бд
	параметры:
		$db - подключение к базе данных
		$key - ключ
		$data - array(
			'column1' => 'value',
			'column2' => array('type'=>'S/D/L', value1, value2, ...)
		)
		'S' - строки
		'D' - числа с плавающей точкой
		'L' - целые числа
	возврат:
		true/false

bdb_del(resource $db, int key);
	описание:
		удалить значение из бд
	параметры:
		$db - подключение к базе данных
		$key - ключ
	возврат:
		true/false

bdb_get_next(resource $db);
	описание:
		получить следующее значение из бд
	параметры:
		$db - подключение к базе данных
	возврат:
		false или массив данных array(
			'column' => 'value'
			...
		)

bdb_get_prev(resource $db);
	описание:
		получить предыдущее значение из бд
	параметры:
		$db - подключение к базе данных
	возврат:
		false или массив данных array(
			'column' => 'value'
			...
		)

bdb_get_cursor(resource $db, int $key);
	описание:
		получить запись из бд и установить курсор на эту запись
	параметры:
		$db - подключение к базе данных
	возврат:
		false или массив данных array(
			'column' => 'value'
			...
		)

bdb_get_last(resource $db);
	описание:
		получить последнее значение из бд
	параметры:
		$db - подключение к базе данных
	возврат:
		false или массив данных array(
			'column' => 'value'
			...
		)

bdb_flush(resource $db);
	описание:
		очистить бд
	параметры:
		$db - подключение к базе данных
	возврат:
		true/false