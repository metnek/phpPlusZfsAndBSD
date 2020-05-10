### required
libkvm
libutil

system_info_uptime();
	возврат:
		array(2) {
		  ["time"]=>						время работы системы
		  array(4) {
		    ["days"]=>
		    int(3)
		    ["hours"]=>
		    int(0)
		    ["mins"]=>
		    int(16)
		    ["secs"]=>
		    int(49)
		  }
		  ["load_avg"]=>					средняя загрузка	
		  array(3) {
		    [0]=>							за последнюю минуту
		    float(0.1787109375)
		    [1]=>							за последние 5 минут
		    float(0.2763671875)
		    [2]=>							за последние 15 минут
		    float(0.28515625)
		  }
		}

system_info_users();
	возврат:
		array(2) {
		  [0]=>
		  array(6) {
		    ["user"]=>
		    string(4) "root"
		    ["tty"]=>
		    string(5) "pts/0"
		    ["ip"]=>
		    string(8) "6.6.6.66"
		    ["login_date"]=>		когда пользователь авторизовался
		    int(1562557591)
		    ["time"]=>				сколько авторизован пользователь
		    array(4) {
		      ["days"]=>
		      int(2)
		      ["hours"]=>
		      int(23)
		      ["mins"]=>
		      int(55)
		      ["secs"]=>
		      int(54)
		    }
		    ["cmd"]=>
		    string(10) "-csh (csh)"
		  }
		  [1]=>
		  array(6) {
		    ["user"]=>
		    string(4) "root"
		    ["tty"]=>
		    string(5) "pts/1"
		    ["ip"]=>
		    string(8) "6.6.6.66"
		    ["login_date"]=>
		    int(1562557960)
		    ["time"]=>
		    array(4) {
		      ["days"]=>
		      int(0)
		      ["hours"]=>
		      int(0)
		      ["mins"]=>
		      int(5)
		      ["secs"]=>
		      int(26)
		    }
		    ["cmd"]=>
		    string(10) "-csh (csh)"
		  }



system_info_ram();
	возврат:
		значения в Кб
		array(15) {
		  ["mem_wire"]=>
		  int(3839388)
		  ["mem_active"]=>
		  int(3412)
		  ["mem_inactive"]=>
		  int(75596)
		  ["mem_cache"]=>
		  int(0)
		  ["mem_free"]=>
		  int(99720)
		  ["mem_gap_vm"]=>
		  int(2920)
		  ["mem_all"]=>
		  int(4021036)
		  ["mem_gap_sys"]=>
		  int(133952)
		  ["mem_phys"]=>
		  int(4154988)
		  ["mem_gap_hw"]=>
		  int(39316)
		  ["mem_hw"]=>
		  int(4194304)
		  ["mem_used"]=>
		  int(4018988)
		  ["mem_avail"]=>
		  int(175316)
		  ["mem_total"]=>
		  int(4194304)
		  ["total_used"]=>
		  int(179600)
		}



system_info_swap();
	возврат:
		значения в Кб
		array(1) {
		  [0]=>
		  array(4) {
		    ["dev"]=>
		    string(7) "vtbd0p2"
		    ["total"]=>
		    int(2097644)
		    ["used"]=>
		    int(35048)
		    ["avail"]=>
		    int(2062596)
		  }
		}

system_info_cpu_use();
	возврат:
		значение в %
		array(1) {
		  ["cpu_use"]=>
		  float(21.014492753623)
		}

system_info_ifaces(array $ifaces);
	параметры:
		$ifaces = array('vtnet0', 'tap0', 'vtnet1');
	возврат:
		0 - найден но не активен
		1 - найден и активен
		2 - не найден (disabled)
		array(3) {
		  ["vtnet0"]=>
		  int(1)
		  ["vtnet1"]=>
		  int(1)
		  ["tap0"]=>
		  int(0)
		}



