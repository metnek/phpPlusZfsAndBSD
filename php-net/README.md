REQUIREMENTS
	Если настройки будут храниться в zfs, то необходим libkcs-kzfs-2.0.0 и libkcs-base, при компиляции необходимо добавить -DZFS_ENABLE
SETTINGS
	php.ini
		"kcs.net.conf" = "путь до файла конфигурации сети (по умолчанию /etc/kinit.conf)"
		"kcs.dhcp.pid_path" = "путь до папки где будут храниться pid файлы dhcp клиента (по умолчанию /var/run)"
		"kcs.dhcp.pid_format" = "формат пути до pid файла, должен принимать две строки: путь до папки и имя интерфейса. по умолчанию %s/dhcpcd-%s.pid"
		"kcs.net.ifaces" = "список сетевых карт, например 'eth0 eth1', значения по умолчанию нет"
		"kcs.net.prefix.enable" = 0/1 (по умолчанию 0), если 1 то kcs.net.ifaces список префиксов, например 'lan wan bridge'
    	"kcs.dhcpd.cmd" = "путь до программы-клиента dhcp (по умолчанию '/sbin/dhclient')"
    	"kcs.dhcpd.flags" = "флаги для программы клиета dhcp (по умолчанию '-b')"
    	"kcs.dhcpd.name" = "имя программы, т.е. arg0 (по умолчанию 'dhclient')"
    conf settings
    	если ZFS_ENABLE то у всех параметров добавляется приставка 'net:...', значения хранятся в датасете с названием значения из kenv "kcs.kzfs.conf" 
    	общие
    		hostname - имя хоста
    	ip-адреса
	    	ifconfig_$ifacename_$type_$param_$aliasnum - установка ip-адреса.
	    	ifconfig_$ifacename_$type_aliases - список номеров алиасов через запятую (0,1,2...)
	    		$ifacename - имя сетевой карты
	    		$type - тип (ip4/ip6)
	    		$aliasnum - номер алиаса
	    		$param - что именно записано (ip-адрес 'alias', маска подсети 'netmask', префикс 'prefix')
	    dns
	    	dns - только если DHCP. записывается 'DHCP'
	    route
	    	route_static_$name_gtw - адрес шлюза
	    	route_static_$name_dest - адрес назначения
	    	route_static_$name_type - тип адреса назначения (0/1 сеть/хост)
	    	route_static_$name_status - статус роута (0/1/2 включен/выключен/включен-но-не-работает)
	    		$name - имя роута
	    	route_list - список имен статических роутов через запятую (route1,route2,route3...)
	    	route_default - шлюз по умолчанию

FUNCTIONS
	netif_up(string $ifname);
		описание:
			поставить флаг UP сетевому интерфейсу
		аргументы:
			$ifname - имя сетевого интерфейса
		возвращаемое значение:
			true/false

	zcfg_save(string $script_path);
		описание:
			сохранить все настройки
		аргументы:
			$script_path - путь до скрипта
		возвращаемое значение:
			true/false

	get_hostname();
		описание:
			получить hostname
		возвращаемое значение:
			string

	set_hostname(string $new_hostname);
		описание:
			поменять hostname
		возвращаемое значение:
			true/false

	ip4_add(string $ifname, string $ip, string $netmask, int $alias_num, bool $enabled, int $set_type);
		описание:
			добавить ipv4-адрес
		аргументы:
			$ifname - имя сетевой карты
			$ip - ip-адрес в формате ipv4
			$netmask - маска подсети в формате xxx.xxx.xxx.xxx
			$alias_num - номер алиаса, для основного номер 0, остальные ++
			$enabled - если true и не set_type == PHP_NET_IP_CONF то применяется в системе
			$set_type - работа с системой или конфигурационными данными (zfs dataset/ conf file)
				значения:
					PHP_IP_NET_AUTO - выполнить в системе и изменить конфигурацию
					PHP_IP_NET_CONF - изменить только конфигурацию сохраненных данных
					PHP_IP_NET_SYSTEM - изменить только в системе
		возвращаемое значение:
			true/false

	ip4_del(string $ifname, int $alias_num, int $set_type);
		описание:
			удалить ipv4-адрес
		аргументы:
			$ifname - имя сетевой карты
			$alias_num - номер алиаса, для основного номер 0, остальные ++
			$set_type - работа с системой или конфигурационными данными (zfs dataset/ conf file)
				значения:
					PHP_IP_NET_AUTO - выполнить в системе и изменить конфигурацию
					PHP_IP_NET_CONF - изменить только конфигурацию сохраненных данных
					PHP_IP_NET_SYSTEM - изменить только в системе
		возвращаемое значение:
			true/false

	ip6_add(string $ifname, string $ip6, int $prefix, int $alias_num, bool $enabled, int $set_type);
		описание:
			добавить ipv6-адрес
		аргументы:
			$ifname - имя сетевой карты
			$ip6 - ip-адрес в формате ipv6
			$prefix - 0-128
			$alias_num - номер алиаса, для основного номер 0, остальные ++
			$enabled - если true и не set_type == PHP_NET_IP_CONF то применяется в системе
			$set_type - работа с системой или конфигурационными данными (zfs dataset/ conf file)
				значения:
					PHP_IP_NET_AUTO - выполнить в системе и изменить конфигурацию
					PHP_IP_NET_CONF - изменить только конфигурацию сохраненных данных
					PHP_IP_NET_SYSTEM - изменить только в системе
		возвращаемое значение:
			true/false

	ip6_del(string $ifname, int $alias_num, int $set_type);
		описание:
			удалить ipv6-адрес
		аргументы:
			$ifname - имя сетевой карты
			$alias_num - номер алиаса, для основного номер 0, остальные ++
			$set_type - работа с системой или конфигурационными данными (zfs dataset/ conf file)
				значения:
					PHP_IP_NET_AUTO - выполнить в системе и изменить конфигурацию
					PHP_IP_NET_CONF - изменить только конфигурацию сохраненных данных
					PHP_IP_NET_SYSTEM - изменить только в системе
		возвращаемое значение:
			true/false

	ips_get([string $ifname]);
		описание:
			получить ip-адреса все/только для сетевой карты
		аргументы:
			$ifname - имя сетевой карты
		возвращаемое значение:
			если сетевая карта не найдена то:
			["iface"=>NULL] иначе:
			[
				"ifname" => [
								"mac" => string "mac-адрес",
								[
									"type" => int 0,						//   ipv4
									"ip" => string "ip",
									"netmask" => string "маска подсети",	// для type=0
									"alias" => int номер алиаса
								],
								[
									"type" => int 1,						//   ipv6
									"ip" => string "ip",
									"prefix" => int префикс,				// для type=1
									"alias" => int номер алиаса
								],
								[
									"type" => int 2,						//   dhcp
									"ip" => NULL,
									"netmask" => NULL,						// для type=2
									"alias" => NULL
								]
							]
			]


	ips_set(string $ifname, array $ips, bool $enabled, int $set_type);
		описание:
			установить ip-адреса для сетевой карты. все имеющиеся ip-адреса карты будут удалены
		аргументы:
			$ifname - имя сетевой карты
			$ips - ip-адреса в следующем формате:
				[
					[
						'type' => int 0,						//   ipv4
						'ip' => string "ip",,
						'netmask' => string "маска подсети",	// для type=0
						'alias' => int номер алиаса
					],
					[
						'type' => int 1,						//   ipv6
						'ip' => string "ip",
						"prefix" => int префикс,				// для type=1
						'alias' => int номер алиаса
					]
				]
			$enabled - если true и не set_type == PHP_NET_IP_CONF то применяется в системе
			$set_type - работа с системой или конфигурационными данными (zfs dataset/ conf file)
				значения:
					PHP_IP_NET_AUTO - выполнить в системе и изменить конфигурацию
					PHP_IP_NET_CONF - изменить только конфигурацию сохраненных данных
					PHP_IP_NET_SYSTEM - изменить только в системе
		возвращаемое значение:
			true/false

	dhcp_set_ip(string $ifname, bool $enabled, int $set_type);
		описание:
			включить dhcp для сетевой карты. все имеющиеся ip-адреса карты будут удалены
		аргументы:
			$ifname - имя сетевой карты
			$enabled - если true и не set_type == PHP_NET_IP_CONF то применяется в системе
			$set_type - работа с системой или конфигурационными данными (zfs dataset/ conf file)
				значения:
					PHP_IP_NET_AUTO - выполнить в системе и изменить конфигурацию
					PHP_IP_NET_CONF - изменить только конфигурацию сохраненных данных
					PHP_IP_NET_SYSTEM - изменить только в системе
		возвращаемое значение:
			true/false

	dhcp_del_ip(string $ifname, int $set_type);
		описание:
			выключить dhcp для сетевой карты. все имеющиеся ip-адреса карты будут удалены
		аргументы:
			$ifname - имя сетевой карты
			$set_type - работа с системой или конфигурационными данными (zfs dataset/ conf file)
				значения:
					PHP_IP_NET_AUTO - выполнить в системе и изменить конфигурацию
					PHP_IP_NET_CONF - изменить только конфигурацию сохраненных данных
					PHP_IP_NET_SYSTEM - изменить только в системе
		возвращаемое значение:
			true/false

	dns_get();
		описание:
			получить настройки dns
		возвращаемое значение:
			если установлено получение по dhcp:
				string "DHCP"
			если вручную:
				[
					"domain" => string "поиск в домене",
					"dns1" => string "основной dns-сервер",
					"dns2" => string "дополнительный dns-сервер" может быть NULL
				]

	dns_set(string $domain, string $dns1 [, string $dns2]);
		описание:
			установить настройки dns вручную
		аргументы:
			$domain - поиск в домене
			$dns1 - основной dns-сервер
			$dns2 - дополнительный dns-сервер
		возвращаемое значение:
			true/false

	dns_set_dhcp();
		описание:
			установить получение настроек dns по dhcp
		возвращаемое значение:
			true/false

	route_add(int $type, string $name, string $dest, string $gtw, int $status);
		описание:
			добавить статичный шлюз
		аргументы:
			$type - тип 0/1, сеть/хост соответственно
			$name - имя шлюза
			$dest - ip-адрес назначения
					для типа сеть в формате xxx.xxx.xxx.xxx/xx
					для типа хост в формате xxx.xxx.xxx.xxx
			$gtw - ip-адрес шлюза в формате xxx.xxx.xxx.xxx
			$status - 0/1 выключен/включен соответственно
		возвращаемое значение:
			true/false

	route_del(string $name);
		описание:
			удалить статичный шлюз
		аргументы:
			$name - имя шлюза
		возвращаемое значение:
			true/false

	route_set_default(string $gtw);
		описание:
			установить шлюз по умолчанию
		аргументы:
			$gtw - ip-адрес шлюза в формате xxx.xxx.xxx.xxx
		возвращаемое значение:
			true/false

	route_del_default();
		описание:
			удалить шлюз по умолчанию
		возвращаемое значение:
			true/false

	routes_get();
		описание:
			получить информацию о шлюзах. type 0/1 сеть/хост соответственно
		возвращаемое значение:
		[
			"name1" => [
				"type" => int 0 сеть,
				"dest" => string ip-адрес назначения в формате xxx.xxx.xxx.xxx/xx,
				"gtw" => string ip-адрес шлюза в формате xxx.xxx.xxx.xxx,
				"status" => int 0/1/2 выключен/включен/включен-но-не-работает соответственно
			],
			"name2" => [
				"type" => int 1 хост,
				"dest" => string ip-адрес назначения в формате xxx.xxx.xxx.xxx,
				"gtw" => string ip-адрес шлюза в формате xxx.xxx.xxx.xxx,
				"status" => int 0/1/2 выключен/включен/включен-но-не-работает соответственно
			],
			"default" => string "шлюз по умолчанию" / NULL
		]

	dns_unset();
		описание:
			удалить настройки DNS
		возвращаемое значение:
			true/false

	route_get(string $route_name);
		описание:
			получить информацию о шлюзе. type 0/1 сеть/хост соответственно
		аргументы:
			$name - имя шлюза
		возвращаемое значение:
		[
			"name1" => [
				"type" => int 0 сеть,
				"dest" => string ip-адрес назначения в формате xxx.xxx.xxx.xxx/xx,
				"gtw" => string ip-адрес шлюза в формате xxx.xxx.xxx.xxx,
				"status" => int 0/1/2 выключен/включен/включен-но-не-работает соответственно
			]
		]