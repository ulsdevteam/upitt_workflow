<?php


function upitt_workflow_test_ftp() {
  $ftp_user = variable_get('upitt_workflow_ftp_username');
  $ftp_pass = variable_get('upitt_workflow_ftp_password');
  $ftp_port = variable_get('upitt_workflow_ftp_port', 990);
  $ftp_server = "ftp.box.com";

  $ftp_conn = ftp_ssl_connect($ftp_server) or die("Could not connect to $ftp_server");
  $login = ftp_login($ftp_conn, $ftp_user, $ftp_pass);

  $local_file = "/tmp/spreadsheet.xslx";
/* May need to change to directory several times to get to the correct folder.

   Digital Collections Contributors
   + HeinzHistoryCenter
     + Lenox-2017.03.21
 */
  $server_file = "Bryce_Lenox_MSS_800_Image_Metadata.xlsx";
//   $server_file = "Digital Collections Contributors/HeinzHistoryCenter/Lenox-2017.03.21/Bryce_Lenox_MSS_800_Image_Metadata.xlsx";
echo "a<br>";
$list = ftp_nlist($ftp_conn, 'Digital Collections Contributors');
dpm($list);

$dir = 'Digital Collections Contributors';
  if (ftp_chdir($ftp_conn, $dir) == false) {
    die('could not change to ' . $dir);
  }
echo "a<br>";

$list = ftp_nlist($ftp_conn, 'HeinzHistoryCenter');
dpm($list);
$dir = 'HeinzHistoryCenter';
  if (ftp_chdir($ftp_conn, $dir) == false) {
    die('could not change to ' . $dir);
  }
echo "a<br>";
$list = ftp_nlist($ftp_conn, 'Lenox-2017.03.21');
dpm($list);

$dir = 'Lenox-2017.03.21';
  if (ftp_chdir($ftp_conn, $dir) == false) {
    die('could not change to ' . $dir);
  }
echo "a<br>";
return 'done';
// download server file
  if (ftp_get($ftp_conn, $local_file, $server_file, FTP_BINARY))
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