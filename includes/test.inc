<?php


function upitt_workflow_test_ftp() {
  $ftp_user = variable_get('upitt_workflow_ftp_username');
  $ftp_pass = variable_get('upitt_workflow_ftp_password');
  $ftp_port = ""; // variable_get('upitt_workflow_ftp_port', 990);
  $ftp_server = "ftp.box.com" . ($ftp_port ? ':' . $ftp_port : '');

  $conn_id = ftp_ssl_connect($ftp_server) or die("Couldn't connect to $ftp_server");
  if (!@ftp_login($conn_id, $ftp_user, $ftp_pass)) {
    die("not logged in to $ftp_server [" . $ftp_user . "] [" . $ftp_pass . "] <br>");
  }
  
  // specify port or enter passive mode
  ftp_pasv($conn_id, true);

  ftp_chdir($conn_id, "Digital Collections Contributors");
  $ftp_root_folder = ftp_pwd($conn_id) . '/';

  $contents = ftp_nlist($conn_id, ".");
  echo "<pre>" . print_r($contents, true);

//  $contents = ftp_rawlist($conn_id, "Digital Collections Contributors", true);
//  echo "<pre>" . print_r($contents, true);

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


  $command = 'LIST Digital Collections Contributors/HeinzHistoryCenter'; // yields "450 Non-existing file"
//  $command = "LIST README\ -\ Contributor\ Setup.boxnote";
  if ($results = ftp_raw($conn_id, $command)) {
    echo "<pre>" . print_r($results, true)."</pre>";
  }

die('<hr>done');
  // $local_file = "/tmp/test.txt";
/* May need to change to directory several times to get to the correct folder.

   Digital Collections Contributors
   + HeinzHistoryCenter
     + Lenox-2017.03.21
 */
/*  $server_file = "ftp_test.txt"; // "Bryce_Lenox_MSS_800_Image_Metadata.xlsx";
//   $server_file = "Digital Collections Contributors/HeinzHistoryCenter/Lenox-2017.03.21/Bryce_Lenox_MSS_800_Image_Metadata.xlsx";
echo "a<br>";
$list = ftp_nlist($ftp_conn, 'Digital Collections Contributors');
echo "<pre>".print_r($list, true) . "</pre>";

$dir = 'Digital Collections Contributors';
  if (ftp_chdir($ftp_conn, $dir) == false) {
    echo 'could not change to ' . $dir . "<br>";
  }
echo "a<br>";

$list = ftp_nlist($ftp_conn, 'HeinzHistoryCenter');
echo "<pre>".print_r($list, true) . "</pre>";

$dir = 'HeinzHistoryCenter';
  if (ftp_chdir($ftp_conn, $dir) == false) {
    echo 'could not change to ' . $dir . "<br>";
  }
echo "a<br>";
$list = ftp_nlist($ftp_conn, 'Lenox-2017.03.21');
echo "<pre>".print_r($list, true) . "</pre>";

$dir = 'Lenox-2017.03.21';
  if (ftp_chdir($ftp_conn, $dir) == false) {
    echo 'could not change to ' . $dir . "<br>";
  }
echo "a<br>";
return 'done';
// download server file
//  if (ftp_get($ftp_conn, $local_file, $server_file, FTP_BINARY))
//  if (ftp_get($ftp_conn, $local_file, $server_file, FTP_ASCII))
  if (ftp_nb_get($ftp_conn, $local_file, $server_file, FTP_ASCII))
    {
    echo "Successfully written to $local_file.";
    }
  else
    {
    echo "Error downloading $server_file.";
    }

  // close connection
  ftp_close($ftp_conn);
*/
  ftp_close($conn_id);
}
