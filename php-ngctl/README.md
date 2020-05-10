FUNCTIONS
	ngctl_make_peer(string $peertype, string $peerhook, string $path, string $hook);
		описание:
			создать узел и соеднить.
		параметры:
			$peertype - тип создаваемого узла
			$peerhook - хук создаваемого узла
			$path - путь куда присоединить новый узел
			$hook - хук к оторому присоединить новый узел
		возврат:
			true/false

	ngctl_make_peer_rc(string $peertype, string $peerhook, string $path, string $hook);
		описание:
			создать узел и соеднить.
		параметры:
			$peertype - тип создаваемого узла
			$peerhook - хук создаваемого узла
			$path - путь куда присоединить новый узел
			$hook - хук к оторому присоединить новый узел
		возврат:
			true/имя нового узла

	ngctl_connect(string $peername, string $peerhook, string $path, string $hook);
		описание:
			соединить два узла.
		параметры:
			$peername - имя первого узла, должно быть имя узла а не путь
			$peerhook - хук первого узла
			$path - путь до второго узла
			$hook - хук второго узла
		возврат:
			true/false

	ngctl_node_name(string $path, string $name);
		описание:
			назвать узел
		параметры:
			$path - путь до узла
			$name - новое имя узла
		возврат:
			true/false

	ngctl_node_shutdown(string $path);
		описание:
			удалить узел (узел с типом ether нельзя удалить)
		параметры:
			$path - путь до узла
		возврат:
			true/false

	ngctl_node_msg(string $path, string $msg, string $params);
		описание:
			отправить сообщение не узел
		параметры:
			$path - путь до узла
			$msg - сообщение
			$params - дополнительные параметры
		возврат:
			true/false

	ngctl_hook_del(string $path, string $hook);
		описание:
			отсоединить хук у узла. у узлов типа vlan, bridge если все хуки отсоединены узел удаляется автоматически
		параметры:
			$path - путь до узла
			$hook - имя хука
		возврат:
			true/false

	ngctl_list();
		описание:
			получить список узлов
		параметры:
		возврат:
			false или 
				array(23) {
				  [0]=>
				  array(3) {
				    ["name"]=>
				    string(11) "lan_p3_v123"
				    ["type"]=>
				    string(6) "eiface"
				    ["hooks"]=>
				    array(1) {
				      [0]=>
				      array(4) {
				        ["ourhook"]=>
				        string(5) "ether"
				        ["peertype"]=>
				        string(4) "vlan"
				        ["peerhook"]=>
				        string(8) "trunk123"
				        ["peername"]=>
				        string(3) "vl3"
				      }
				    }
				  }
				  ....

	ngctl_node_info(string $path);
		описание:
			получить информацию об узле
		параметры:
			$path - путь до узла
		возврат:
			false или 
				  array(3) {
				    ["name"]=>
				    string(11) "lan_p3_v123"
				    ["type"]=>
				    string(6) "eiface"
				    ["hooks"]=>
				    array(1) {
				      [0]=>
				      array(4) {
				        ["ourhook"]=>
				        string(5) "ether"
				        ["peertype"]=>
				        string(4) "vlan"
				        ["peerhook"]=>
				        string(8) "trunk123"
				        ["peername"]=>
				        string(3) "vl3"
				      }
				    }
				  } ....

	ngctl_save(array $data);
		описание:
			сохранить конфигурацию netgraph. массив такого же формата как из функции ngctl_list
		параметры:
			$data - 
				array(23) {
				  [0]=>array(3) {
				    ["name"]=>
				    string(11) "lan_p3_v123"
				    ["type"]=>
				    string(6) "eiface"
				    ["hooks"]=>
				    array(1) {
				      [0]=>
				      array(4) {
				        ["ourhook"]=>
				        string(5) "ether"
				        ["peertype"]=>
				        string(4) "vlan"
				        ["peerhook"]=>
				        string(8) "trunk123"
				        ["peername"]=>
				        string(3) "vl3"
				      }
				    }
				  }
				 } ....
		возврат:
			true/false

	ngctl_rollback();
		описание:
			применить конфигурацию из БД
		возврат:
			true/false