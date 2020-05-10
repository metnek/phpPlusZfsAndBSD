xls2array(string $filename [, string $sheetname]);
	описание:
		получить данные из xls файла
	параметры:
		$filename - полный путь до xls файла
		$sheet_name - открыть конкретный лист 
	возврат:
		false или массив всех данных файла
	дополнительно:
		устарела (возможна нехватка памяти при больших файлах)

xls_get_sheets(string $filename);
	описание:
		получить список листов XLS файла
	параметры:
		$filename - полный путь до xls файла
	возврат:
		false или список листов XLS файла

xls2array_open(string $xls_path [, string $sheet_name]);
	описание:
		открыть xls файл
	параметры:
		$xls_path - полный путь до xls файла
		$sheet_name - открыть конкретный лист 
	возврат:
		false/resource

xls2array_close(resource $xls_rsrc);
	описание:
		закрыть xls файл
	параметры:
		$xls_rsrc - ресурс типа "XLS File"
	возврат:
		true/false

xls2array_read(resource $xls_rsrc);
	описание:
		прочитать следующую строку в xls файле
	параметры:
		$xls_rsrc - ресурс типа "XLS File"
	возврат:
		false или массив из значений колонок строки


