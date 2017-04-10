<?php

function upitt_workflow_datastreams_io_export($batch_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $pids = array();
  if ($batch_id) {
    $batch = upitt_workflow_batch_load($batch_id);
    $csv_arr = upitt_workflow_csv_file_as_array($batch['file']);
    $pid_values = upitt_workflow_csv_extract_identifiers($csv_arr);
    foreach ($pid_values as $p) {
      $pids[] = $p;
    }
  }
  $_SESSION['pids'] = json_encode($pids);

  drupal_goto('/islandora/datastreams_io/export');
}
