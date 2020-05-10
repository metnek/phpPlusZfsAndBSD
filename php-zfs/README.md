REQUIREMENTS
	/usr/src
	libzfs
	libzfs_core
	libnvpair
	libuutil
	libjail
	libgeom
	libintl (devel/gettext-runtime)
	libkzfs (libkcs-kzfs-1.0.0)

DESCRIPTION
	zfs.so - расширение PHP для работы с zfs

SETTINGS
	php.ini
		kcs.zfs.snap.rules.cmd = "скрипт для создания и удаления автоматических снимков (по умолчанию /etc/zfs/autosnap)"
		kcs.zfs.snap.rules.file = "файл планировщика где будут храниться настройки автоматичсеких снимков (по умолчанию /etc/crontab)"
	требования к скрипту kcs.zfs.snap.rules.cmd:
		1) скрипт должен принимать 2 параметра
			create/delete dataset_name
		2) информацию об интервале удаления должен брать из файла kcs.zfs.snap.rules.file в формате #dataset_name:$interval
	       	$interval:
	       		Y:$year - хранить $year лет
	       		m:$month - хранить $month месяцев
	       		w:$weeks - хранить $weeks недель
	       		d:$days - хранить $days дней
	    пример записи в файле планировщика:
	    	#zroot/data:Y:1
			*/30 6-21 * * 1-5  root /etc/zfs/autosnap create zroot/data
			*/30 * * * *  root /etc/zfs/autosnap delete zroot/data
		описание примера:
			скрипт с параметром создания снимка будет запускаться с Пн по Пт, с 6:00 утра до 21:00 вечера каждые 30 минут
			скрипт с удалением будет запускаться каждые 30 минут каждый день для проверки снимков выходящих за интервал хранения в 1 год (Y:1)
FUNCTIONS
	zfs_ds_props(string $zname);
		описание:
			получить пользовательские параметры из датасета
		параметры:
			$zname - имя датасета
		возврат:
			false/ массив {'propname1' => 'value1', 'propname2' => 'value2' ...}

	zfs_ds_props_remove(string $zname, array propnames);
		описание:
			удалить пользовательские параметры из датасета
		параметры:
			$zname - имя датасета
			$propnames - массив из параметров ['propname1', 'propname2' ...]
		возврат:
			true/false

	zfs_ds_create(string $zname, array $params);
		описание:
			создание датасета
		параметры:
			$zname - имя датасета
			$params - 
				array(
					["config" => array(key=>value),]
					["ext" => array(key=>value)]
				)
				"config" - системные параметры датасета, для вывода всех возможных zfs_all_props();
				"ext" - пользовательские параметры датасета, в имени должно быть ':'
		возврат:
			true/false

	zfs_all_props()
		описание:
			получить все доступные параметры для zfs
		возврат:
			array(
				array(3) {
				    ["name"]=>
				    string(5) "vscan"
				    ["readonly"]=>
				    bool(false)
				    ["values"]=>
				    string(8) "on | off"
				  }
				........
			)

	zfs_ds_destroy(string $zname [, bool $recursive = false]);
		описание:
			удаление датасета
		параметры:
			$zname - имя датасета
			$recursive - рекурсивное удаление, необходимо если есть снимки или дочерние датасеты
		возврат:
			true/false

	zfs_ds_list(array $params);
		описание:
			список датасетов
		параметры:
			$params - параметры для каждого датасета
				array('prop1', 'prop2' ...)
		возврат:
		var_dump(zfs_ds_list(array('creation', 'used')));
			array(42) {
			  ["lolushka"]=>
			  array(2) {
			    ["creation"]=>
			    int(1562702578)
			    ["used"]=>
			    string(4) "304K"
			  }
			  ["zroot"]=>
			  array(2) {
			    ["creation"]=>
			    int(1540965642)
			    ["used"]=>
			    string(5) "35.1G"
			  }
			  .......
			}

	zfs_ds_update(string $zname, array $params);
		описание:
			обновление паратров датасета
		параметры:
			$zname - имя датасета
			$params - 
				array(
					"config" => array(key=>value),
					"ext" => array(key=>value)
				)
				"config" - системные параметры датасета, для вывода всех возможных zfs_all_props();, только те у которых 'readonly'=>true
				"ext" - пользовательские параметры датасета, в имени должно быть ':'
		возврат:
			true/false

	zfs_ds_rename(string $zname, string $new_zname [, array $params]);
		описание:
			переименование датасета
		параметры:
			$zname - имя датасета
			$new_zname - новое имя датасета
			$params - 
				array(
					['force'=>true/false,]		принудительное переименование
					['nounmount'=>true/false,]	не делать перемонтивароние датасета
					['parents'=>true/false,]		создать недостающие родительские датасеты в $new_zname
				)
		возврат:
			true/false

	zfs_snap_create(string $zname, string $snapname, string $desc [, bool $recursive]);
		описание:
			создание снимка для датасета, если $recursive true, то снимок создается и для дочерних датасетов
		параметры:
			$zname - имя датасета
			$snapname - имя снимка
			$desc - описание снимка
			$recursive - рекурсивное создание снимка для дочерних датасетов
		возврат:
			true/false


	zfs_snap_destroy(string $zname, string $snapname [, bool $recursive]);
		описание:
			удаление снимка для датасета, если $recursive true, то снимок удаляется и у дочерних датасетов
		параметры:
			$zname - имя датасета
			$snapname - имя снимка
			$recursive - рекурсивное удаление снимка для дочерних датасетов
		возврат:
			true/false

	zfs_snap_update(string $zname, string $snapname, string $desc);
		описание:
			обновление описания снимка
		параметры:
			$zname - имя датасета
			$snapname - имя снимка
			$desc - описание снимка
		возврат:
			true/false

	zfs_snap_rollback(string $zname, string $snapname);
		описание:
			восстановление снимка, все снимки созданные после восстанавливаемого будут удалены, поэтому необходимо выводить предупреждение в интерфейсе
		параметры:
			$zname - имя датасета
			$snapname - имя снимка
		возврат:
			true/false

	zfs_snap_rename(string $zname, string $snapname, string $new_snapname [, array $params]);
		описание:
			переименование снимка
		параметры:
			$zname - имя датасета
			$snapname - имя снимка
			$new_snapname - новое имя снимка
			$params - 
				array(
					['force'=>true/false,]		принудительное переименование
					['recursive'=>true/false,]	рекурсивное переименование для дочерних датасетов
				)
		возврат:
			true/false

	zfs_snap_list(string $zname);
		описание:
			список снимков для датасета
		параметры:
			$zname - имя датасета
		возврат:
			array(2) {
			  ["alia"]=>	имя снимка
			  array(3) {
			    ["used"]=>
			    string(4) "112K"
			    ["creation"]=>
			    int(1562539020)
			    ["desc"]=>
			    string(0) ""
			  }
			  ["kekeke"]=>
			  array(3) {
			    ["used"]=>
			    string(4) "112K"
			    ["creation"]=>
			    int(1561846999)
			    ["desc"]=>
			    string(0) ""
			  }
			}

	zfs_snap_rule_set(string $zname, array $sched);
		описание:
			добавить или обновить расписание создания снимков для датасета
		параметры:
			$zname - имя датасета
			$sched - расписание в формате cron
			array(
				'interval'=>string $interval
			    'create'=>array(
	                    'min'=>'*/30',
	                    'hour'=>'*',
	                    'mday'=>'*',
	                    'month'=>'*',
	                    'wday'=>'6,7'
		            ),
			    'delete'=>array(
	                    'min'=>'*/30',
	                    'hour'=>'*',
	                    'mday'=>'*',
	                    'month'=>'*',
	                    'wday'=>'1,2,3,4'
		            )
	        );
	       	$interval:
	       		Y:$year - хранить $year лет
	       		m:$month - хранить $month месяцев
	       		w:$weeks - хранить $weeks недель
	       		d:$days - хранить $days дней
		возврат:
			true/false

	zfs_snap_rule_get(string $zname);
		описание:
			получить расписание создания снимков для датасета
		параметры:
			$zname - имя датасета
		возврат:
			false или
				array(3) {
				  ['interval']=>
				  	string() "m:6"
				  ["create"]=>
				  array(5) {
				    ["min"]=>
				    string(4) "*/30"
				    ["hour"]=>
				    string(1) "*"
				    ["mday"]=>
				    string(1) "*"
				    ["month"]=>
				    string(1) "*"
				    ["wday"]=>
				    string(3) "6,7"
				  }
				  ["delete"]=>
				  array(5) {
				    ["min"]=>
				    string(4) "*/30"
				    ["hour"]=>
				    string(1) "*"
				    ["mday"]=>
				    string(1) "*"
				    ["month"]=>
				    string(1) "*"
				    ["wday"]=>
				    string(7) "1,2,3,4"
				  }
				}
	       	"interval":
	       		Y:$year - хранить $year лет
	       		m:$month - хранить $month месяцев
	       		w:$weeks - хранить $weeks недель
	       		d:$days - хранить $days дней

	zfs_snap_rule_delete(string $zname);
		описание:
			удалить расписание создания снимков для датасета
		параметры:
			$zname - имя датасета
		возврат:
			true/false

	zfs_send(string $zname, array $params [, bool $move = false]);
		описание:
			отправить датасет на другой хост, принимающий хост сначала должен запустить функцию zfs_recv()
		параметры:
			$zname - имя датасета
			$params - параметры удаленного хоста
					array('ip'=> string ipv4, 'port'=> int port);
			$move - если false то датасет копируется на другой хост, иначе перемещается
		возврат:
			true/false

	zfs_recv(string $zname, array $params);
		описание:
			принять датасет с другого хоста
		параметры:
			$zname - имя датасета
			$params - параметры данного хоста
					array('ip'=> string ipv4, 'port'=> int port);
		возврат:
			true/false

	zfs_mount(string $zname [, array $mntopts]);
		описание:
			примонтировать датасет
		параметры:
			$zname - имя датасета
			$mntopts - параметры монтирования
				atime/noatime
				exec/noexec
				ro/rw
				suid/nosuid
				array('ro', 'noatime'...);
		возврат:
			true/false

	zfs_mount_list();
		описание:
			получить список примонтиваронных датасетов и точки монтирования
		возврат:
			false или
				array(5) {
				  ["zroot/ROOT/default"]=>
				  string(1) "/"
				  ["zroot/data"]=>
				  string(5) "/data"
				  ["zroot/data/storage"]=>
				  string(13) "/data/storage"
				  ["zroot/tmp"]=>
				  string(4) "/tmp"
				  ["zroot/usr/home"]=>
				  string(9) "/usr/home"
				}

	zfs_mount_all([array $mntopts]);
		описание:
			примонтировать все датасеты
		параметры:
			$mntopts - параметры монтирования
				atime/noatime
				exec/noexec
				ro/rw
				suid/nosuid
				array('ro', 'noatime'...);
		возврат:
			true/false

	zfs_unmount(string $entry [, bool $force = false]);
		описание:
			отмонтировать датасет
		параметры:
			$entry - имя датасета или точка монтирования
			$force - принудительное отмонтирование
		возврат:
			true/false

	zfs_unmount_all([bool $force = false]);
		описание:
			отмонтировать все датасеты, функция не тестировалась
		параметры:
			$force - принудительное отмонтирование
		возврат:
			true/false

	zpool_destroy(string $zpool_name);
		описание:
			удалить зпул
		параметры:
			$zpool_name - имя зпула
		возврат:
			true/false

	zpool_list([, array $params]);
		описание:
			получить список зпулов
		параметры:
			$params - параметры для каждого зпула
		возврат:
			array(2) {
			  ["9237116510943290981"]=>		id зпула
			  array(1) {
			    ["name"]=>
			    string(8) "lolushka"
			  }
			  [2893572901409731148]=>
			  array(1) {
			    ["name"]=>
			    string(5) "zroot"
			  }
			}

	zpool_create(string $zpool_name, array $params);
		описание:
			создать зпул
			не все параметры зпула тестировались
		параметры:
			$zpool_name - имя зпула
			$params - array(
				'devs' - обязательный параметр,
						array(
							array(
								'type'=> string mirror/raidz/raidz[1-3]/stripe
								'children'=> array('md1', 'md2')
								'is_log'=>bool
							)
							array(
								'type'=> string mirror/raidz/raidz[1-3]/stripe
								'children'=> array('md3', 'md4')
								'is_log'=>bool
							)
							......
						),
				['force'=>bool,] принудительное создание
				['no_feat'=>bool,] отключить дополнительные плюшки
				['root'=>string,] альтернативный корневой каталог, параметр cachefile автоматически ставится в 'none'
				['mounpoint'=>string] точка монтирования
				['props'=>array('prop_name'=>'value')] все параметры можно получить из функции zpool_all_props();
				['fs_props'=>array('fs_prop_name'=>'value')] все параметры можно получить из функции zfs_all_props();
				['temp_name'=>string] временное имя зпула, параметр cachefile автоматически ставится в 'none'
			)
		возврат:
			true/false

	zpool_all_props();
		описание:
			получить все доступные параметры для zpool
		возврат:
			array(25) {
			  [0]=>
			  array(3) {
			    ["name"]=>
			    string(9) "allocated"
			    ["readonly"]=>
			    bool(true)
			    ["values"]=>
			    string(6) "<size>"
			  }
			  ......

	zpool_update(string $zpool_name, array $params);
		описание:
			обновить параметры зпула
		параметры:
			$zpool_name - имя зпула
			$params - array('prop_name'=>'value'), все параметры можно получить из функции zpool_all_props();
		возврат:
			true/false

	zpool_add(string $zpool_name, array $devs);
		описание:
			добавить устройства в зпул
		параметры:
			$zpool_name - имя зпула
			$devs - array(
						array(
							'type'=> string mirror/raidz/raidz[1-3]/stripe
							'children'=> array('md1', 'md2')
							'is_log'=>bool
						)
						array(
							'type'=> string mirror/raidz/raidz[1-3]/stripe
							'children'=> array('md4', 'md3')
							'is_log'=>bool
						)
						......
					)
		возврат:
			true/false

	zpool_remove();					// wait !!! DANGER !!!
	zpool_split(string $zpool_name, string $new_zpool_name [, array $params]);
		описание:
			создать новый зпул из дисков старого зпула. старый зпул должен состоять только из mirror устройств,
			из каждого mirror устройства берется один диск (по умолчанию последний) и из этих дисков создается зпул.
		параметры:
			$zpool_name - имя зпула
			$new_zpool_name - имя нового зпула
			$params - array(
					['devs'=>array('md1', 'md4')]
					['root'=>string] альтернативный корневой каталог
					['mntopts'=>array('ro', 'noatime'...)] параметры монтирования
					['props'=>array('prop_name'=>'value')] все параметры можно получить из функции zpool_all_props();
			)
			'mntopts' - параметры монтирования
				atime/noatime
				exec/noexec
				ro/rw
				suid/nosuid
		возврат:
			true/false

	zpool_import(string $zpool_name [, array $params]);
		описание:
			импортировать зпул
		параметры:
			$zpool_name - имя зпула
			$params - array(
				['newname'=>string [, 'temp_name'=>bool]] импортировать с новым именем, если 'temp_name'=>true, то новое имя будет использоваться до следующего экспорта
				['force'=>bool] принудительное импортирование
				['rewind'=>bool] восстановление испорченного зпула
				['missing_log'=>bool] разрешить импортирование без log устройства
				['no_mount'=>bool] импортировать без монтирования 
				['mntopts'=>array('ro', 'noatime'...)] параметры монтирования
				['props'=>array('prop_name'=>'value')] все параметры можно получить из функции zpool_all_props();
				['root'=>string,] альтернативный корневой каталог, параметр cachefile автоматически ставится в 'none'
				['cachefile'=>string | 'searchdirs'=>array ('dir1', 'dir2' ...)] исключают друг друга
				['destroyed'=>bool] импортировать только уничтоженные зпулы, также необходим параметр 'force'=>true
			)
			'mntopts' - параметры монтирования
				atime/noatime
				exec/noexec
				ro/rw
				suid/nosuid
			'cachefile' загрузить информацию из кэш файла
			'searchdirs' искать устройства в данных папках
		возврат:
			true/false

	zpool_import_all([array $params]);
		описание:
			импортировать все зпулы
		параметры:
			$params - array(
				['force'=>bool] принудительное импортирование
				['rewind'=>bool] восстановление испорченного зпула
				['missing_log'=>bool] разрешить импортирование без log устройства
				['no_mount'=>bool] импортировать без монтирования 
				['mntopts'=>array('ro', 'noatime'...)] параметры монтирования
				['props'=>array('prop_name'=>'value')] все параметры можно получить из функции zpool_all_props();
				['root'=>string,] альтернативный корневой каталог, параметр cachefile автоматически ставится в 'none'
				['cachefile'=>string | 'searchdirs'=>array ('dir1', 'dir2' ...)] исключают друг друга
				['destroyed'=>bool] импортировать только уничтоженные зпулы, также необходим параметр 'force'=>true
			)
			'mntopts' - параметры монтирования
				atime/noatime
				exec/noexec
				ro/rw
				suid/nosuid
			'cachefile' загрузить информацию из кэш файла
			'searchdirs' искать устройства в данных папках
		возврат:
			true/false

	zpool_import_list([array $params]);
		описание:
			получить список зпулов для импорта
		параметры:
			$params - 
				array(
					['cachefile'=>string | 'searchdirs'=>array ('dir1', 'dir2' ...),] исключают друг друга
					['destroyed'=>bool]
				);
				'cachefile' загрузить информацию из кэш файла
				'searchdirs' искать устройства в данных папках
				'destroyed' показать только уничтоженные зпулы
		возврат:
			false если нет зпулов для импорта или:
			array(2) {
			  ["9237116510943290981"]=>
			  array(5) {
			    ["name"]=>
			    string(8) "lolushka"
			    ["state"]=>
			    string(6) "ONLINE"
			    ["destroyed"]=>
			    bool(false)
			    ["action"]=>
			    string(62) "The pool can be imported using its name or numeric identifier."
			    ["config"]=>
			    array(2) {
			      [0]=>
			      array(2) {
			        ["name"]=>
			        string(5) "md111"
			        ["state"]=>
			        string(6) "ONLINE"
			      }
			      [1]=>
			      array(2) {
			        ["name"]=>
			        string(5) "md333"
			        ["state"]=>
			        string(6) "ONLINE"
			      }
			    }
			  }
			  ["10711593606292361566"]=>
			  array(5) {
			    ["name"]=>
			    string(3) "kek"
			    ["state"]=>
			    string(6) "ONLINE"
			    ["destroyed"]=>
			    bool(false)
			    ["action"]=>
			    string(62) "The pool can be imported using its name or numeric identifier."
			    ["config"]=>
			    array(1) {
			      [0]=>
			      array(3) {
			        ["name"]=>
			        string(8) "mirror-0"
			        ["state"]=>
			        string(6) "ONLINE"
			        ["config"]=>
			        array(2) {
			          [0]=>
			          array(2) {
			            ["name"]=>
			            string(5) "md222"
			            ["state"]=>
			            string(6) "ONLINE"
			          }
			          [1]=>
			          array(2) {
			            ["name"]=>
			            string(5) "md444"
			            ["state"]=>
			            string(6) "ONLINE"
			          }
			        }
			      }
			    }
			  }
			}

	zpool_export(string $zpool_name [, bool $force = false]);
		описание:
			экспортировать зпул
		параметры:
			$zpool_name - имя зпула
			$force - принудительное экспортирование
		возврат:
			true/false

	zpool_status([string $zpool_name]);
		описание:
			показать статус зпулов/зпула
		параметры:
			$zpool_name - имя зпула
		возврат:
			false или:
			array(5) {
			  ["name"]=>
			  string(3) "kek"
			  ["state"]=>
			  string(6) "ONLINE"
			  ["scan"]=>
			  string(14) "none requested"
			  ["config"]=>		если не mirror/raid то диски сразу в этом массиве
			  array(1) {
			    [0]=>
			    array(6) {
			      ["name"]=>
			      string(8) "mirror-0"
			      ["state"]=>
			      string(6) "ONLINE"
			      ["read"]=>
			      string(1) "0"
			      ["write"]=>
			      string(1) "0"
			      ["cksum"]=>
			      string(1) "0"
			      ["config"]=>
			      array(2) {
			        [0]=>
			        array(5) {
			          ["name"]=>
			          string(5) "md222"
			          ["state"]=>
			          string(6) "ONLINE"
			          ["read"]=>
			          string(1) "0"
			          ["write"]=>
			          string(1) "0"
			          ["cksum"]=>
			          string(1) "0"
			        }
			        [1]=>
			        array(5) {
			          ["name"]=>
			          string(5) "md444"
			          ["state"]=>
			          string(6) "ONLINE"
			          ["read"]=>
			          string(1) "0"
			          ["write"]=>
			          string(1) "0"
			          ["cksum"]=>
			          string(1) "0"
			        }
			      }
			    }
			  }
			  ["err"]=>
			  string(20) "No known data errors"
			}


	zpool_attach(string $zpool_name, string $dev, string $new_dev [, bool $force = false]);
		описание:
			присоединить диск к диску или к raid или к mirror
			функция не тестировалась
		параметры:
			$zpool_name - имя зпула
			$dev - имя устройства к которому необходимо присоединить 
			$new_dev - имя устройства которое необходимо присоединить 
			$force - принудительное присоединение
		возврат:
			true/false

	zpool_detach(string $zpool_name, string $dev);
		описание:
			отсоединить диск от зпула
			функция не тестировалась
		параметры:
			$zpool_name - имя зпула
			$dev - имя устройства которое необходимо отсоединить 
		возврат:
			true/false

	zpool_online(string $zpool_name, array $devs [, bool $expand=false]);
		описание:
			перевести устройство в режим онлайн
			функция не тестировалась
		параметры:
			$zpool_name - имя зпула
			$devs - массив устройств 
					array('/dev/md1', '/dev/md2' ...);
			$expand - расширить устройство
		возврат:
			true/false

	zpool_offline(string $zpool_name, array $devs [, bool $temp=false]);
		описание:
			перевести устройство в режим офлайн
			функция не тестировалась
		параметры:
			$zpool_name - имя зпула
			$devs - массив устройств 
					array('/dev/md1', '/dev/md2' ...);
			$temp - временный перевод в режим офлайн, после перезагрузки хоста устройствавернутся в прежний режим
		возврат:
			true/false

	zpool_reguid(string $zpool_name);
		описание:
			создать новый id для зпула
		параметры:
			$zpool_name - имя зпула
		возврат:
			true/false

	zpool_reopen(string $zpool_name);
		описание:
			переоткрыть все устройства зпула
		параметры:
			$zpool_name - имя зпула
		возврат:
			true/false

	zpool_labelclear(string $dev [, bool $force=false]);
		описание:
			очистить устройство от меток zfs
			функция не тестировалась
		параметры:
			$dev - имя устройства
			$force - принудительная очистка
		возврат:
			true/false

	zpool_scrub(array $zpool_list [, int $cmd=0]);
		описание:
			сканирование зпулов
			функция не тестировалась
		параметры:
			$zpool_list - массив имен зпулов
				array('zpool1', 'zpool2' ...);
			$cmd - 
				0 - старт сканирования (start)
				1 - приостановить сканирование (pause)
				2 - остановить сканирование (stop)
		возврат:
			true/false

	zpool_clear(string $zpool_name [, bool $do_rewind=false [, string $dev]]);
		описание:
			очистить зпул/устройство зпула от ошибок
		параметры:
			$zpool_name - имя зпула
			$do_rewind - инициировать восстановление испорченного зпула/устройства зпула
			$dev - имя устройства
		возврат:
			true/false

	zpool_replace(string $zpool_name, string $dev [, string $new_dev=NULL [, bool $force=false]]);
		описание:
			заменить устройство в зпуле
		параметры:
			$zpool_name - имя зпула
			$dev - имя устройства
			$new_dev - имя нового устройства, NULL когда устройство вышло из строя и на его место добавляют новое, то есть имя осталось прежним
			$force - принудительная замена
		возврат:
			true/false

	zpool_root_mount_from();
		описание:
			получить информацию о системном зпуле
		возврат:
			false или string,
			следующего вида "zfs:zroot/ROOT/default"

	zpool_get_devs(string $zpool_name);
		описание:
			получить список устройств зпула (1 уровень)
		параметры:
			$zpool_name - имя зпула
		возврат:
			false или:
			array(3) {
			  [0]=>
			  string(5) "md333"
			  [1]=>
			  string(8) "mirror-1"
			  [2]=>
			  string(5) "md444"
			}

	zfs_settings_merge(string $from, string $to);
		описание:
			заменить пользовательские параметры в $to на параметры с $from
		параметры:
			$from - имя датасета или полное имя снимка (dataset@snapname) откуда взять параметры
			$to - имя датасета или полное имя снимка (dataset@snapname) где перезаписать параметры
		возврат:
			true/false
