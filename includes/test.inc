<?php

function upitt_workflow_get_ftp_connection() {
  $ftp_user = variable_get('upitt_workflow_ftp_username');
  $ftp_pass = variable_get('upitt_workflow_ftp_password');
  $ftp_server = "ftp.box.com"; 

  try {
    $conn_id = ftp_ssl_connect($ftp_server);
    if (false === $conn_id) {
      throw new Exception('Unable to connect');
    }

    $loggedIn = ftp_login($conn_id,  $ftp_user,  $ftp_pass);
    if (true === $loggedIn) {
      error_log("upitt_workflow_get_ftp_connection Success!");
    } else {
      throw new Exception('Unable to log in');
    }
    // specify port or enter passive mode
    ftp_pasv($conn_id, true);
  } catch (Exception $e) {
    $message = $e->getMessage(); 
    if ($message == 'Unable to log in') {
      echo "<br><b style='color:red'>Contact Admin <a href='mailto:digital-collections@pitt.edu?subject=Need to " .
           "reset login for ftp account&body=Need to login to box.com with a CAPTCHA code for " . $ftp_user . 
           ".'>digital-collections@pitt.edu</a> to reset the login for the FTP connection.</b>";
    } else {
      echo "Failure: " . $message;
    }
    // Close the connection if it is somehow still active.
    @ftp_close($conn_id);
    $conn_id = FALSE;
  }
  return $conn_id;
}

function upitt_workflow_ftp_listing($current_path) {
  $current_path = '/' . str_replace("|", "/", $current_path);

  $conn_id = upitt_workflow_get_ftp_connection();
  if ($conn_id) {
    $command = 'CWD ' . $current_path;
    if (!$results = ftp_raw($conn_id, $command)) {
      echo "Could not change to directory $current_path<br>";
    }
    echo  "Current directory : " . ftp_pwd($conn_id) . "<hr>";

    $site = 'http://' . $_SERVER['HTTP_HOST'];

    $contents = ftp_nlist($conn_id, ".");

    $files = $folders = array();
    foreach ($contents as $ftp_file_or_folder) {
      if ($ftp_file_or_folder <> '.') {
        $full_path_file_or_folder = (($current_path) ? $current_path . '/' : '') . $ftp_file_or_folder;
        if (!ftp_chdir($conn_id, $full_path_file_or_folder)) {
          $files[$full_path_file_or_folder] = $ftp_file_or_folder;
        }
        else {
          $folders[$full_path_file_or_folder] = $ftp_file_or_folder;
          ftp_chdir($conn_id, $current_path);
        }
      }
    }
    // Output these as links to navigate
    $current_path_no_leftslash = str_replace("/", "|", ltrim($current_path, '/'));

    foreach ($folders as $folder) {
      $thisfolder_path = $current_path_no_leftslash . '|' . $folder;
      echo '<a class="ftp_folder" href="#" onclick="ftp_change_path(\'' . $thisfolder_path . '\', \'' . $site . '\');return false;">' . $folder . '</a><br>';
//      echo '<span class="ftp_folder" onclick="ftp_change_path(\'' . $thisfolder_path . '\', \'' . $site . '\')">' . $folder . '</span><br>';
    }
    foreach ($files as $file) {
      echo '<span class="ftp_file">' . $file . '</span><br>';
    }


    ftp_close($conn_id);
  }
  // exit;

}
