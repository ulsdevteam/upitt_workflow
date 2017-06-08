<?php

function upitt_workflow_get_ftp_connection() {
  $ftp_user = variable_get('upitt_workflow_ftp_username');
  $ftp_pass = variable_get('upitt_workflow_ftp_password');
  $ftp_server = "ftp.box.com"; 
  $conn_id = ftp_ssl_connect($ftp_server) or die("Couldn't connect to $ftp_server");

  try {
    $conn_id = ftp_ssl_connect($ftp_server);
    if (false === $conn_id) {
        throw new Exception('Unable to connect');
    }

    $loggedIn = ftp_login($conn_id,  $ftp_user,  $ftp_pass);
    if (true === $loggedIn) {
        echo 'Success!';
    } else {
        throw new Exception('Unable to log in');
    }
    // specify port or enter passive mode
//    ftp_pasv($conn_id, true);

    ftp_close($conn_id);
  } catch (Exception $e) {
    $message = $e->getMessage(); 
    if ($message == 'Unable to log in') {
      echo "<br><b style='color:red'>Contact Admin <a href='mailto:digital-collections@pitt.edu?subject=Need to reset login for ftp account&body=Need to login to box.com with a CAPTCHA code for " . $ftp_user . ".'>digital-collections@pitt.edu</a> to reset the login for the FTP connection.</b>";
    } else {
      echo "Failure: " . $message;
    }
    // Close the connection if it is somehow still active.
    @ftp_close($conn_id);
    $conn_id = FALSE;
  }
  
  return $conn_id;
}

function upitt_workflow_test_ftp($current_path) {
  $current_path = str_replace("|", "/", $current_path);

  $conn_id = upitt_workflow_get_ftp_connection();
  if ($conn_id) {
    // What is the current FTP directory?
    $ftp_root_folder = ftp_pwd($conn_id) . '/';
    echo 'current directory ' . $ftp_root_folder ."<br>";

    echo  "current path : " . $current_path . "<hr>";
    if ($current_path && !ftp_chdir($conn_id, $current_path)) {
      die('could not change to ' . $current_path);
    }

    $contents = ftp_nlist($conn_id, ".");
    echo "<pre>" . print_r($contents, true);

    $files = $folders = array();
    echo "<pre style='color:#4b3'>";
    foreach ($contents as $ftp_file_or_folder) {
      //  Digital Collections Contributors
      if ($ftp_file_or_folder <> '.' && $ftp_file_or_folder <> '..') {
        $full_path_file_or_folder = $ftp_root_folder . $ftp_file_or_folder;
        echo 'chdir ' . $full_path_file_or_folder . "<br>";
        if (ftp_chdir($conn_id, $full_path_file_or_folder)) {
          $folders[$full_path_file_or_folder] = $ftp_file_or_folder;
        }
        else {
          $files[$full_path_file_or_folder] = $ftp_file_or_folder;
        }
      }
    }
    echo "</pre>";
    echo "<h3>Folders:</h3><pre>" . print_r($folders, true). "</pre>";
    echo "<h3>Files:</h3><pre>" . print_r($files, true). "</pre>";


    $command = 'LIST /Digital Collections Contributors/HeinzHistoryCenter'; // yields "450 Non-existing file"
//  $command = "LIST README\ -\ Contributor\ Setup.boxnote";
    if ($results = ftp_raw($conn_id, $command)) {
      echo "<pre>" . print_r($results, true)."</pre>";
    }

    ftp_close($conn_id);
  }
  // exit;
}
