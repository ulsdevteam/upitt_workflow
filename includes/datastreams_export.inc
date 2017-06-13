<?php

function upitt_workflow_datastreams_io_export($batch_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('module', 'islandora_datastreams_io');

  $pids = array();
  if ($batch_id) {
    $batch = upitt_workflow_batch_load($batch_id);
    // TODO: this may be better to load the items from the database, but that
    // table is not populated with newly created batches that use a CSV upload.
    // The code would need to be written that will populate that record FIRST.
    $csv_arr = upitt_workflow_csv_file_as_array($batch['file']);
    $pid_values = upitt_workflow_csv_extract_identifiers($csv_arr);
    foreach ($pid_values as $p) {
      $pids[] = $p;
    }
  }
  $pids[] = 'pitt:testOCR2';
  $pids[] = 'pitt:20710320-hpichswp-0024';

  // Calling this will populate the PIDS for and redirect to the export form.
  islandora_datastreams_io_pids_to_export_form($pids, 1);
}
