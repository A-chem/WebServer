<?php
$body = file_get_contents("php://input");
?>
<!doctype html>
<html>
<head><title>PHP CGI Test</title></head>
<body>
    <h1>PHP CGI OK</h1>
    <p>Method: <?php echo htmlspecialchars($_SERVER["REQUEST_METHOD"]); ?></p>
    <p>Query: <?php echo htmlspecialchars($_SERVER["QUERY_STRING"]); ?></p>
    <p>Script: <?php echo htmlspecialchars($_SERVER["SCRIPT_NAME"]); ?></p>
    <pre><?php echo htmlspecialchars($body); ?></pre>
</body>
</html>