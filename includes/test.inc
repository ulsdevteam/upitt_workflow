<?php


function upitt_workflow_test_ftp() {

  $ctx = stream_context_create(array('ssl' => array(
//    'verify_peer' => true,
    'verify_peer' => false,
    'cafile' => '/tmp/box_ssl_cert.pem',
    'CN_match' => 'ftp.box.com'
  )));

  $ftp_user = variable_get('upitt_workflow_ftp_username');
  $ftp_pass = variable_get('upitt_workflow_ftp_password');
  $ftp_port = variable_get('upitt_workflow_ftp_port', 990);
  $ftp_server = "ftp.box.com";

  $ftpPath = 'ftps://' . $ftp_user . ':' . $ftp_pass . '@' . $ftp_server;

  $dirHandle = opendir($ftpPath, $ctx);
  while (($file = readdir($dirHandle)) !== false) {
    echo "filename: $file\n";
  }
  closedir($dirHandle);
die();


//  $ftp_conn = ftp_connect($ftp_server, $ftp_port, 5) or die("Could not connect to $ftp_server");
  $ftp_conn = ftp_ssl_connect($ftp_server, 21, 5) or die("Could not connect to $ftp_server");
  $login = ftp_login($ftp_conn, $ftp_user, $ftp_pass);

echo ($login ? 'Login success!' : 'could not login!') . "<br>";

  $local_file = "/tmp/test.txt";
/* May need to change to directory several times to get to the correct folder.

   Digital Collections Contributors
   + HeinzHistoryCenter
     + Lenox-2017.03.21
 */
  $server_file = "ftp_test.txt"; // "Bryce_Lenox_MSS_800_Image_Metadata.xlsx";
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
}
