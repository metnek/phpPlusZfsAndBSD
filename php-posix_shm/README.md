FUNCTIONS

	posix_shm_attach(string $name, int $size [, int $mode]);
		описание:
			создать или открыть область общей памяти
		параметры:
			$name - имя области
			$size - размер области
			$mode - режим создания/открытия (по умолчанию 0660)
		возврат:
			false или ресурс POSIX shared memory

	posix_shm_write(resource $rsrc_shm, string $data);
		описание:
			записать данные в общую память
		параметры:
			$rsrc_shm - ресурс POSIX shared memory
			$data - данные для записи
		возврат:
			true/false

	posix_shm_read(resource $rsrc_shm);
		описание:
			получить данные из общей памяти
		параметры:
			$rsrc_shm - ресурс POSIX shared memory
		возврат:
			false или string

	posix_shm_close(resource $rsrc_shm);
		описание:
			закрыть и освободить общую память
		параметры:
			$rsrc_shm - ресурс POSIX shared memory
		возврат:
			false/NULL