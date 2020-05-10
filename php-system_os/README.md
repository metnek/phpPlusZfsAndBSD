SETTINGS
	php.ini
		kcs.reboot.cmd = "pre reboot cmd";
		kcs.php.mount.enable = 0/1 (запретить/разрешить)
			запрет/разрешение на использование функций system_os_mount, system_os_unmount, system_os_mount_list, system_os_chroot_cmd
			если 0, то функции будут возвращать false

FUNCTIONS
	system_os_datetime_set(array $datetime);
		описание:
			установить системное время и дату
		параметры:
			$datetime - array(
						"year" => int год				больше чем 1900
						"mon" => int месяц				[0-11]
						"mday" => int день месяца		[1-31]	
						"hour" => int час				[0-23]
						"min" => int минуты				[0-59]
						"sec" => int секунды			[0-59]
					)
		возврат:
			true/false

	system_os_reboot();
		описание:
			перезагрузить хост
		возврат:
			false или перезагрузка

	system_os_extract(string $filename, string $path);
		описание:
			распаковать архив txz
		параметры:
			$filename - полный путь до архива
			$path - путь куда распаковать архив
		возврат:
			true/false

	system_os_ncpu()
		описание:
			получить количество ядер
		возврат:
			false/int $num

	system_os_cpu_set(int $pid, mixed $proc);
		описание:
			установить ядро для процесса
		параметры:
			$pid - идентификатор процесса
			$proc - номер ядра/ядер. может быть массивом чисел или просто числом. если -1, то все ядра
		возврат:
			false/true

	system_os_mount(string $type, string $mountpoint [, string $source]);
		описание:
			примонтировать файловую систему
		параметры:
			$type - тип файловой системы (tmpfs, nullfs, devfs, procfs, fdescfs)
			$mountpoint - точка монтирования
			$source - что монтировать (только для nullfs)
		возврат:
			false/true

	system_os_unmount(string $mountpoint);
		описание:
			отмонтировать файловую систему
		параметры:
			$mountpoint - точка монтирования
		возврат:
			false/true

	system_os_mount_list();
		описание:
			получить список примонтированных файловых систем
		возврат:
			false или 
				array(19) {
				  [0]=>
				  array(3) {
				    ["type"]=>
				    string(3) "zfs"
				    ["mountpoint"]=>
				    string(1) "/"
				    ["source"]=>
				    string(18) "zroot/ROOT/default"
				  }
				  [1]=>
				  array(3) {
				    ["type"]=>
				    string(5) "devfs"
				    ["mountpoint"]=>
				    string(4) "/dev"
				    ["source"]=>
				    string(5) "devfs"
				  } ....

	system_os_chroot_cmd(string $dir, string $cmd [, bool $is_fork]);
		описание:
			сменить корневую папку и выполнить команду
		параметры:
			$dir - на какую папку сменить
			$cmd - команда
			$is_fork - выполнить команду в дочернем процессе
		возврат:
			false или ничего

	system_os_kenv_get(string $key);
		описание:
			получить значение из kernel environment
		параметры:
			$key - ключ по которому получить значение
		возврат:
			false или значение