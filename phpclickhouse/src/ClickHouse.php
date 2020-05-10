<?php
class ClickHouse
{
    private $qurl;

    public function __construct(String $protocol="http",String $host="127.0.0.1",int $port=8123, $user=null,$password=null)
    {
        if($user!=null) $host = $user.":".$password."@".$host;
        $this->qurl = $protocol."://".$host.":".$port;
    }

    public function query(String $query): ClickHouseData
    {
        $query = trim($query);
        $query .= " FORMAT JSON";
        $query = str_replace(" ","%20",$query);
        $data = file_get_contents($this->qurl."/?query=".$query);
        return new ClickHouseData($query,$data);
    }

    public function prepare(String $query): ClickHouseData
    {
        return new ClickHouseData($query,null);
    }

    public static function escape(String $string): String
    {
        $string = str_replace("\n","\\n",$string);
        $string = str_replace("\r","\\r",$string);
        $string = str_replace("\\","\\\\",$string);
        $string = str_replace("'","\'",$string);
        $string = str_replace("\"",'\"',$string);
        return $string;
    }
}

class ClickHouseData
{
    private $query;
    private $result;

    public function __construct(String $query,$result)
    {
        $this->query = $query;
        $this->result = json_decode($result);
    }

    public function execute(Array $args): ClickHouseData
    {
        $i=0;
        $element = 0;
        while(false !== ($element = strpos($this->query,"?",((isset($args[$i-1]))?$element+strlen($args[$i-1]):$element)))){
            if(!isset($args[$i]))break;
            if(is_string($args[$i])) $args[$i] = '\''.ClickHouse::escape($args[$i]).'\'';
            $this->query = substr_replace($this->query,"",$element,1);
            $this->query = substr_replace($this->query,$args[$i],$element,0);
            $i++;
        }
        return $this;
    }

    public function fetch()
    {
        return $this->query;
    }

    public function fetchAll(): Array
    {
        return $this->result->data;
    }

    public function getCountRows(): int
    {
        return $this->result->rows;
    }


}
