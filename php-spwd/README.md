SETTINGS
	php.ini:
		kcs.spwd.start_position = начальный uid (по умолчанию 1000)
	Для включения файла /etc/master.passwd добавить -DNEED_MASTER_SPWD (или сколмпилировать с './configure SINGLE=yes')

FUNCTIONS
	spwd_get_entry($hash, $find);
		параметры:
			$find - string (username) или int (uid)
		возвтрат:
			1) false
			2) array(9) {
				  ["name"]=>
						  string(4) "alia"
				  ["uid"]=>
						  int(1002)
				  ["gid"]=>
						  int(1008)
				  ["change"]=>
						  int(0)
				  ["class"]=>
						  string(0) ""
				  ["desc"]=>
						  string(12) "Alia Akauova"
				  ["dir"]=>
						  string(12) "/home/lol123"
				  ["shell"]=>
						  string(7) "/bin/sh"
				  ["expire"]=>
						  int(0)
				}


	spwd_get_all($hash);
		возвтрат:
			1) false
			2) array(1) {
				  [0]=>
					array(9) {
					  ["name"]=>
							  string(4) "alia"
					  ["uid"]=>
							  int(1002)
					  ["gid"]=>
							  int(1008)
					  ["change"]=>
							  int(0)
					  ["class"]=>
							  string(0) ""
					  ["desc"]=>
							  string(12) "Alia Akauova"
					  ["dir"]=>
							  string(12) "/home/lol123"
					  ["shell"]=>
							  string(7) "/bin/sh"
					  ["expire"]=>
							  int(0)
					}
				}


	spwd_add_entry_full($hash, $pw_name, $pw_pwd, $pw_gid
					[, string $pw_class="" 
					[, string $pw_desc="" 
					[, string $pw_dir="/nonexistent" 
					[, string $pw_shell="/usr/sbin/nologin" 
					[, int $pw_change = 0 
					[, int $pw_expire = 0]]]]]]);
		возвтрат:
			true/false

	spwd_add_entry($hash, $pw_name, $pw_pwd, $pw_gid
					[, string $pw_class="" 
					[, string $pw_desc="" ]]
		возвтрат:
			true/false

	spwd_set_entry($hash, $pw_uid
					[, string $pw_name=NULL
					[, string $pw_pwd=NULL
					[, int $pw_gid=-1
					[, string $pw_class=NULL
					[, string $pw_desc=NULL
					[, string $pw_dir=NULL
					[, string $pw_shell=NULL
					[, int $pw_change=-1
					[, int $pw_expire=0]]]]]]]]]);
		возвтрат:
			true/false

	spwd_del_entry($hash, int $uid);
		возвтрат:
			true/false	
	spwd_verify(string $username, $string pwd);
		возвтрат:
			1) false
			2) string(64) hash

	spwd_get_entry_by_hash(string $hash);
		возвтрат:
			1) false
			2) array(9) {
				  ["name"]=>
						  string(4) "alia"
				  ["uid"]=>
						  int(1002)
				  ["gid"]=>
						  int(1008)
				  ["change"]=>
						  int(0)
				  ["class"]=>
						  string(0) ""
				  ["desc"]=>
						  string(12) "Alia Akauova"
				  ["dir"]=>
						  string(12) "/home/lol123"
				  ["shell"]=>
						  string(7) "/bin/sh"
				  ["expire"]=>
						  int(0)
				}

	spwd_class_list(string $hash);
		возвтрат:
			1) false
			2) array(1) {
				["kek_standard"]=>
				  array(1) {
				    ["alia"]=>
				    string(13) "ulululululkek"
				  }
				}

	spwd_class_del_entry(string $hash, string $class_name, string $entry);
		параметры:
			$hash - хэш авторизации
			$class_name - имя класса
			$entry - имя параметра в классе которое необходимо удалить
		возвтрат:
			true/false

	spwd_class_set_entry(string $hash, string $class_name, string $entry, string/int $value);
		параметры:
			$hash - хэш авторизации
			$class_name - имя класса
			$entry - имя параметра в классе которое необходимо обновить/добавить
			$value - значение параметра
		возвтрат:
			true/false

	spwd_srand();
		описание:
			Изменяет начальное число генератора псевдослучайных чисел