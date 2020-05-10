REQUIREMENTS
	libkcs-zfw

FUNCTIONS
	Rules
		zfw_rule_add(mixed $rule);
			описание:
				добавить новое правило файервола
			параметры:
				$rule - правило/массив правил. правила начинаются с "add ...."
			возврат:
				true/false

		zfw_rule_delete(int $set, int rule_num);
			описание:
				удалить правило
			параметры:
				$set - сет к которой относится правило (0-30)
				$rule_num - номер правила
			возврат:
				true/false

		zfw_rule_move(int rule_num, int $set);
			описание:
				переместить правило в другой сет
			параметры:
				$rule_num - номер правила
				$set - сет к которой относится правило (0-30)
			возврат:
				true/false

		zfw_rules_list(int $set [, bool $show_stats]);
			описание:
				получить правила сета номер $set
			параметры:
				$set - номер сета
				$show_stats - включить в результат статистику прошедших пакетов
			возврат:
				false или
					array(1) {
					  [0]=>
					  array(4) {
					    ["set"]=>
					    int(10)
					    ["rule_num"]=>
					    int(4343)
					    ["disabled"]=>
					    bool(true)
					    ["rule"]=>
					    string(31) "allow ip from any to table(obj1)"
					  }
					}

		zfw_rules_flush(int $set [, bool $is_all]);
			описание:
				удалить правила из конкретного сета или из всех в зависимости от $is_all. Правило под номером 1 никогда не удаляется.
			параметры:
				$set - номер сета
				$is_all - не учитывать $set, и удалить правила из всех сетов
			возврат:
				true/false

		zfw_rules_rollback();
			описание:
				восстановить правила из бд.
			возврат:
				true/false

	Tables
		zfw_table_create(string $tablename);
			описание:
				создать таблицу файервола
			параметры:
				$tablename - название таблицы файервола
			возврат:
				true/false

		zfw_table_destroy(string $tablename);
			описание:
				уничтожить таблицу файервола
			параметры:
				$tablename - название таблицы файервола
			возврат:
				true/false

		zfw_table_destroy_all();
			описание:
				уничтожить все таблицы файервола
			возврат:
				true/false

		zfw_table_entry_add(string $tablename, string $entry);
			описание:
				добавить элемент в таблицу
			параметры:
				$tablename - название таблицы файервола
				$entry - элемент (ip-адрес)
			возврат:
				true/false

		zfw_table_entry_delete(string $tablename, string $entry);
			описание:
				удалить элемент из таблицы
			параметры:
				$tablename - название таблицы файервола
				$entry - элемент (ip-адрес)
			возврат:
				true/false

		zfw_table_flush(string $tablename);
			описание:
				удалить все эелементы из таблицы файервола
			параметры:
				$tablename - название таблицы файервола
			возврат:
				true/false

		zfw_table_list();
			описание:
				получить список таблиц и их элементов
			возврат:
				false или 
					array(3) {
					  ["lol"]=>
					  NULL			// без элементов
					  ["alia"]=>
					  array(3) {
					    [0]=>
					    string(10) "9.9.9.9/32"
					    [1]=>
					    string(14) "33.44.55.66/32"
					    [2]=>
					    string(14) "126.127.0.5/32"
					  }
					  ["test"]=>
					  array(2) {
					    [0]=>
					    string(10) "5.5.5.5/32"
					    [1]=>
					    string(15) "123.43.55.12/32"
					  }
					}

		zfw_table_get(string $tablename);
			описание:
				получить информацию о таблице файервола
			параметры:
				$tablename - название таблицы файервола
			возврат:
				false или 
					array(1) {
					  ["lol"]=>
					  array(2) {
					    [0]=>
					    string(16) "12.34.126.178/32"
					    [1]=>
					    string(15) "78.54.81.234/32"
					  }
					}

	Sets
		zfw_set_delete(int $num);
			описание:
				удалить правила в сете номер $num
			параметры:
				$num - номер сета
			возврат:
				true/false

		zfw_set_list();
			описание:
				получить список сетов.
			возврат:
				false или
					array(31) {
					  [0]=>
					  bool(true)
					  [1]=>
					  bool(true)
					  [2]=>
					  bool(false)
					  ...
					}, где true - enabled, false - disabled

		zfw_set_enable(int $num);
			описание:
				включить сет номер $num
			параметры:
				$num - номер сета
			возврат:
				true/false

		zfw_set_disable(int $num);
			описание:
				выключить сет номер $num
			параметры:
				$num - номер сета
			возврат:
				true/false

		zfw_set_move(int $from, int $to)
			описание:
				переместить правила из сета $from в сет $to
			параметры:
				$from - номер сета из которого перемещать правила
				$to - номер сета куда перемещать правила
			возврат:
				true/false

	NAT
		zfw_nat_delete(int $num);
			описание:
				удалить nat номер $num
			параметры:
				$num - номер nat
			возврат:
				true/false

		zfw_nat_config();
			описание:
				применить конфигурацию nat номер $num из бд
			параметры:
				$num - номер nat
			возврат:
				true/false

	Pipe/Queue
		zfw_pipe_delete(int $num);
			описание:
				удалить pipe номер $num
			параметры:
				$num - номер pipe
			возврат:
				true/false

		zfw_queue_delete(int $num);
			описание:
				удалить queue номер $num
			параметры:
				$num - номер queue
			возврат:
				true/false

		zfw_pipe_config();
			описание:
				применить конфигурацию pipe номер $num из бд
			параметры:
				$num - номер pipe
			возврат:
				true/false

		zfw_queue_config();
			описание:
				применить конфигурацию queue номер $num из бд
			параметры:
				$num - номер queue
			возврат:
				true/false
