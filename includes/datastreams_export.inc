<?php

function upitt_workflow_datastreams_io_export($batch_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  module_load_include('module', 'islandora_datastreams_io');

  if ($batch_id) {
    $pids = array();
    $items = upitt_workflow_batch_load_item_identifiers($batch_id);
    $pids = array_keys($items);
    // Calling this will populate the PIDS for and redirect to the export form.
    islandora_datastreams_io_pids_to_export_form($pids, 0);
  }
}
