<?php

//Authentication information needed to connect with your database
$servername = "##########";
$username = "##########";
$password = "##########";

// Create a connection to your database
try {
    $pdo = new PDO("mysql:host=$servername;dbname=$username", $username, $password);
} catch (PDOException $e) {
    $message = ['error' => 'unable to connect to db'];
    exit(json_encode($message));
}

// Check if the request method is GET or POST
$request_method = $_SERVER['REQUEST_METHOD'];

if ($request_method == 'GET') {
    // Handle the GET request
    $query = array();
    parse_str($_SERVER['QUERY_STRING'], $query);

    $sql = 'INSERT INTO logs (TAG, date_time, info) VALUES (:TAG, :date_time, :info);';
    $statement = $pdo->prepare($sql);
    $statement->execute([':TAG' => $query['TAG'], ':date_time' => $query['date_time'], ':info' => $query['info']]);

    $message = ['result' => 'log inserted'];
    echo json_encode($message);
} elseif ($request_method == 'POST') {
    // Handle the POST request
    $logs = json_decode(file_get_contents("php://input"), true);

    $sql = 'INSERT INTO logs (TAG, date_time, info) VALUES (:TAG, :date_time, :info);';
    $statement = $pdo->prepare($sql);

    foreach ($logs as $log) {
        $statement->execute([':TAG' => $log['TAG'], ':date_time' => $log['date_time'], ':info' => $log['info']]);
    }

    $message = ['result' => 'logs inserted'];
    echo json_encode($message);
} else {
    // Unsupported request method
    $message = ['error' => 'unsupported request method'];
    echo json_encode($message);
}

