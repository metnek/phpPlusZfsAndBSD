<?php
print "->Start<-".PHP_EOL;
include "../src/ClickHouse.php";

$ClickHouse = new ClickHouse();
$query = [
    "prepare-1"=>"SELECT * FROM `users` WHERE `login`=? and `password`=?"
];
print "PrepareQuery:".$query['prepare-1'].PHP_EOL;

$prepare = $ClickHouse->prepare($query['prepare-1']);
$q=$prepare->execute([1,'pwd']);
print $q->fetch().PHP_EOL;


print "->End<-".PHP_EOL;
