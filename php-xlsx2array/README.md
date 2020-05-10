### required
libexpat
libzip

xlsx2array(string $filename [, string $sheetname]);
	описание:
		получить данные из xlsx файла
	параметры:
		$filename - полный путь до xlsx файла
		$sheet_name - открыть конкретный лист 
	возврат:
		false или массив всех данных файла
	дополнительно:
		устарела (возможна нехватка памяти при больших файлах)

xlsx_get_sheets(string $filename);
	описание:
		получить список листов XLSX файла
	параметры:
		$filename - полный путь до xlsx файла
	возврат:
		false или список листов XLSX файла

xlsx2array_open(string $xlsx_path [, string $sheet_name]);
	описание:
		открыть xlsx файл
	параметры:
		$xlsx_path - полный путь до xlsx файла
		$sheet_name - открыть конкретный лист 
	возврат:
		false/resource

xlsx2array_close(resource $xlsx_rsrc);
	описание:
		закрыть xlsx файл
	параметры:
		$xlsx_rsrc - ресурс типа "XLSX File"
	возврат:
		true/false

xlsx2array_read(resource $xlsx_rsrc);
	описание:
		прочитать следующую строку в xlsx файле
	параметры:
		$xlsx_rsrc - ресурс типа "XLSX File"
	возврат:
		false или массив из значений колонок строки

