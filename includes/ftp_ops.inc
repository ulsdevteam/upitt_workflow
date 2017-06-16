<?php

function upitt_workflow_get_ftp_connection() {
  $ftp_user = variable_get('upitt_workflow_ftp_username');
  $ftp_pass = variable_get('upitt_workflow_ftp_password');
  $ftp_server = "ftp.box.com"; 

  // Should this use the variable_get('upitt_workflow_notification_email', 'digital-collections@pitt.edu')
  // account that is configured to recieve the notifications?  That is probably a separate account.

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
    // Put in the "Loading" animation, but keep it hidden
    echo '<div id="edit-box-files-loading-img" style="display:none"><h3>Loading results from FTP</h3><p>Please be patient...</p></div>';
    if (!$results = ftp_raw($conn_id, $command)) {
      echo "Could not change to directory $current_path<br>";
    }
    echo  '<div id="current_directory"> ' . ftp_pwd($conn_id) . '</div>';

    echo '<div id="box-files-output">';
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
      // Special handling for the ".." folder name -- else, just add the folder to
      // the previous value using the "|" delimiter.
      if ($folder == '..') {
        $current_path_parts = explode("|", $current_path_no_leftslash);
        $last_part = array_pop($current_path_parts);
        unset($current_path_parts[$last_part]);
        $thisfolder_path = implode("|", $current_path_parts );
      }
      else {
        $thisfolder_path = $current_path_no_leftslash . '|' . $folder;
      }
      echo '<div class="ftp_folder"><a href="#" onclick="ftp_change_path(\'' . $thisfolder_path . '\', \'' . $site . '\');return false;">' . $folder . '</a></div>';
    }
    foreach ($files as $file) {
      $pathinfo = pathinfo($file);
      $file_class = 'ftp_file';
      if (isset($pathinfo['extension'])) {
        if ($pathinfo['extension'] == 'zip') {
          $file_class='ftp_file_zip';
        }
      }
      echo '<div class="' . $file_class . '">' . $file . '</div>';
    }

    echo '</div>';

    ftp_close($conn_id);
  }
  // exit;
}

function upitt_workflow_ftp_get_zipfiles_and_csv($ftp_folder, $download_to, $batch_external_id) {
  $conn_id = upitt_workflow_get_ftp_connection();
  $downloaded = false;
  $num_zip_files_unzipped = 0;
  $files_unzipped = array();
  $csv_file = '';
  if ($conn_id) {
    // 1. Change to the $ftp_folder directory
    // 2. For each of the *.zip files in that directory, save in $download_to -
    //    and finally change to the newly created local directory.
    $make_folder_command = 'rm -rf ' . $download_to . ';mkdir ' . $download_to;
    exec($make_folder_command);

    $ftp_folder = '/' . str_replace(
            array("ftp:", "|"),
            array("", "/"),
            $ftp_folder);
    $command = 'CWD ' . $ftp_folder;
    // Put in the "Loading" animation, but keep it hidden
    if (!$results = ftp_raw($conn_id, $command)) {
      echo "Could not change to directory $ftp_folder<br>";
      return FALSE;
    }

    $folder_contents = ftp_nlist($conn_id, ".");
    $zip_files = array();
    foreach ($folder_contents as $ftp_file_or_folder) {
      if ($ftp_file_or_folder <> '.' && $ftp_file_or_folder <> '..') {
        $full_path_file_or_folder = (($ftp_folder) ? $ftp_folder . '/' : '') . $ftp_file_or_folder;
        $full_local_path_to_file = (($download_to) ? $download_to . '/' : '') . $ftp_file_or_folder;
        if (!@ftp_chdir($conn_id, $full_path_file_or_folder)) {
          $pathinfo = pathinfo($ftp_file_or_folder);
          if (isset($pathinfo['extension'])) {
            if ($pathinfo['extension'] == 'zip') {
              $zip_files[$full_local_path_to_file] = $ftp_file_or_folder;
            }
            elseif ($pathinfo['extension'] == 'csv') {
              $csv_file = $ftp_file_or_folder;
            }
          }
        }
      }
    }
    // Must change back to the working directory
    $command = 'CWD ' . $ftp_folder;
    if ($results = ftp_raw($conn_id, $command)) {
      foreach ($zip_files as $local_filename => $zip_file) {
        // 1. Download this to the local $download_to
        $local_name = $download_to . '/' . $zip_file;
        $downloaded |= ftp_get($conn_id, $local_name, $zip_file, FTP_BINARY);
      }

      if ($csv_file) {
        $local_name = $download_to . '/' . $csv_file;
        $downloaded_csv = ftp_get($conn_id, $local_name, $csv_file, FTP_BINARY);
        // update the batch record for this batch to set the file value to point to this CSV
        module_load_include('inc', 'upitt_workflow', 'includes/utilities');
        upitt_workflow_update_batch_file($batch_external_id, $local_name);
      }
      // Must also change to the local download_to directory.
      chdir($download_to);

      $return_var = $unzip_output = array();
      // 2. Unzip the files - and delete the ZIP files as we go.
      foreach ($zip_files as $local_filename => $zip_file) {
        $unzip_command = 'unzip ' . $local_filename;
        $output = array();
        exec($unzip_command, $output, $return_var);
        $unzip_output = array_merge($output, $unzip_output);
        if (!unlink($local_filename)) {
          dpm('There was a problem deleting the file ' . $zip_file);
        }
      }
    }

    foreach ($unzip_output as $output) {
      if (strstr($output, 'Archive:')) {
        $num_zip_files_unzipped++;
      } elseif (strstr($output, 'inflating:')) {
        $extracted_filename_only = str_replace("inflating: ", "", $output);
        $files_unzipped[] = $extracted_filename_only;
      }
    }

    ftp_close($conn_id);
  }
  return array('downloaded' => $downloaded,
      'num_zip_files_unzipped' => $num_zip_files_unzipped,
      'files_unzipped' => $files_unzipped,
      'csv_file' => $csv_file);
}
