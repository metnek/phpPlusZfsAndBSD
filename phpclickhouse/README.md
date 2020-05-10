# PHPClickHouse Client
Простая версия клиента clickhouse (с использованием интерфейса HTTP).

## Методы и объекты

### ClickHouse
```php
public __constuct ( [ String $protocol="http" [, String $host="127.0.0.1" [, int $port=8123 [, $user=null [, $pass=null ]]]]] ) : ClickHouse
```
Задаёт параметры для подключения к базе данных.


- $protocol - Протокол подключение к базе (http или https)
- $host - Хост базы данных
- $port - Порт базы данных
- $user - Имя пользователя базы данных
- $pass - Пароль пользователя базы данных

```php
public query(String $query): ClickHouseData
```
Выполняет SQL-запрос и возвращает результирующий набор в виде объекта ClickHouseData.


### ClickHouseData
```php
public execute( [ $args ] ) : ClickHouseData
```
Запускает подготовленный запрос. Если запрос содержит маркеры параметров (псевдопеременные), вы должны передать массив значений только на вход.

- $args - Массив значений, содержащий столько элементов, сколько параметров заявлено в SQL-запросе.
