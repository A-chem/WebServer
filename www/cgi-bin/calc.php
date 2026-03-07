#!/usr/bin/php
<?php
// PHP CGI script for testing GET and POST calculations

// 1. Headers (Darori bach l-browser i-fhem chno hadchi)
echo "Content-Type: text/html\r\n\r\n";

// 2. Grab data from Environment (GET) or STDIN (POST)
$method = getenv('REQUEST_METHOD');
$result = null;
$a = 0;
$b = 0;

if ($method === 'POST') {
    // Read from stdin for POST data
    parse_str(file_get_contents("php://input"), $params);
    $a = isset($params['a']) ? intval($params['a']) : 0;
    $b = isset($params['b']) ? intval($params['b']) : 0;
    $result = $a + $b;
} elseif ($method === 'GET') {
    // Read from QUERY_STRING for GET data
    parse_str(getenv('QUERY_STRING'), $params);
    $a = isset($params['a']) ? intval($params['a']) : 0;
    $b = isset($params['b']) ? intval($params['b']) : 0;
    $result = $a + $b;
}

?>
<!DOCTYPE html>
<html>
<head>
    <title>PHP Calculator CGI</title>
    <style>
        body { font-family: sans-serif; text-align: center; padding: 50px; background: #f4f7f6; }
        .calc-box { background: white; padding: 20px; border-radius: 10px; display: inline-block; shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .res { font-size: 24px; color: #007bff; font-weight: bold; }
    </style>
</head>
<body>
    <div class="calc-box">
        <h1>➕ CGI Calculator</h1>
        <p>Method used: <b><?php echo $method; ?></b></p>
        
        <form method="POST" action="/cgi-bin/calc.php">
            <input type="number" name="a" value="<?php echo $a; ?>"> + 
            <input type="number" name="b" value="<?php echo $b; ?>">
            <button type="submit">Calculate</button>
        </form>

        <?php if ($result !== null): ?>
            <p class="res">Result: <?php echo $a . " + " . $b . " = " . $result; ?></p>
        <?php endif; ?>
        
        <br>
        <a href="/">Back to Home</a>
    </div>
</body>
</html>