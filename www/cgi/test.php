#!/usr/bin/php-cgi
<?php
$body = file_get_contents('php://input');
header('Content-Type: text/plain');
echo "method=" . (isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] : '') . "\n";
echo "query=" . (isset($_SERVER['QUERY_STRING']) ? $_SERVER['QUERY_STRING'] : '') . "\n";
echo "body=" . $body . "\n";
?>
