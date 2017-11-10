<?php

/**
 * @file
 * Drush hooks.
 */

/**
 * Implements hook_drush_command().
 */
function upitt_workflow_drush_command() {
  $items = array();

  $items['upitt_workflow_ingest_via_hopper'] = array(
    'aliases' => array('uwih'),
    'description' => 'This will ingest the next item from the hopper.  This call does not take any options.',
    'drupal dependencies' => array('islandora_batch','islandora_ocr'),
    'examples' => array(
      'drush -v --user=admin --uri=http://dev.gamera.library.pitt.edu upitt_workflow_ingest_via_hopper',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );

  $items['upitt_workflow_generate_ocr_datastreams'] = array(
    'aliases' => array('uwgod'),
    'description' => 'Create OCR datastreams for paged content object\'s pages.',
    'drupal dependencies' => array('islandora_batch','islandora_ocr'),
    'examples' => array(
      'drush -v --user=admin --uri=http://dev.gamera.library.pitt.edu upitt_workflow_generate_ocr_datastreams --paged_object_pid=pitt:00awc1789m',
    ),
    'options' => array(
      'paged_object_pid' => array(
        'description' => 'The PID value of the paged content object.',
        'required' => TRUE,
      ),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  return $items;
}

            // TODO - move all of the hopper logic to a separate drush job that
            // can check if there is space to ingest another.  That could be
            // called via cron job every 10 minutes.
/**
 * Drush function that will ingest the next hopper item if there is space.
 */
function drush_upitt_workflow_ingest_via_hopper() {
  module_load_include('inc', 'islandora_batch', 'includes/db');
  module_load_include('inc', 'upitt_workflow', 'includes/hopper');
  module_load_include('inc', 'upitt_workflow', 'includes/process_barcodes.form');

  $next_hopper_item = upitt_workflow_get_next_pending_hopper_queue_item();
  if (isset($next_hopper_item['PID']) && isset($next_hopper_item['files_size'])) {
    $partition_size_and_used = upitt_workflow_get_ingest_temp_size_and_used();
    $fedora_size_and_used = upitt_workflow_get_fedora_avail_bytes();
    // The avail calculation is slightly different -- using the size and
    // the available space, as long as 20% of the size is greater than
    // the available MINUS the new files_size.
    $threshold_used_fedora = 0.13 * $fedora_size_and_used['disk_size'];
    $threshold_used = 0.70 * $partition_size_and_used['partition_size'];
    if ((($partition_size_and_used['used'] + $next_hopper_item['files_size']) < $threshold_used) &&
        (($fedora_size_and_used['disk_bytes_avail'] - $next_hopper_item['files_size']) > $threshold_used_fedora) ) {
      @list($namespace, $next_barcode) = explode(":", $next_hopper_item['PID']);
      // This will purge, update workflow record, copy files, prepare
      // the ingest batch, and ingest the islandora_batch.
      $null_markup = upitt_workflow_delete_update_copy_ingest_for_barcodes($next_barcode);
      if (strstr($null_markup, 'There was an error') == '') {
        upitt_workflow_set_next_ingest_start_timestamp_for_hopper_by_pid($next_hopper_item['PID']);
      }
    }
    else {
      $space_errors = array();
      if (($partition_size_and_used['used'] + $next_hopper_item['files_size']) > $threshold_used) {
        $space_errors[] = dt('No space for temp ingest files for next object @PID which is @files_size ' .
          'bytes.  The temp ingest partition: @used' . ' bytes.',
          array(
            '@PID' => $next_hopper_item['PID'], 
            '@files_size' => number_format($next_hopper_item['files_size']),
            '@used' => number_format($partition_size_and_used['used']),
          ));
      }
      if (($fedora_size_and_used['disk_bytes_avail'] - $next_hopper_item['files_size']) < $threshold_used_fedora) {
        $space_errors[] = dt('No space on fedora object store for next object @PID which is ' . 
          '@files_size bytes.  Fedora available @avail bytes, but the threshold is at @threshold bytes.',
          array(
            '@PID' => $next_hopper_item['PID'],
            '@files_size' => number_format($next_hopper_item['files_size']),
            '@avail' => number_format($fedora_size_and_used['disk_bytes_avail']),
            '@threshold' => number_format($threshold_used_fedora),
          ));
      }
      if (count($space_errors)) {
        drush_log(implode("<br>", $space_errors), 'error');
      }
    }
  }
}

/**
 * Implements hook_islandora_batch_ingest().
 *
 * Ingests for the specified amount of time, if there is anything already
 * preprocessed.
 */
function drush_upitt_workflow_generate_ocr_datastreams() {
  module_load_include('inc', 'islandora', 'includes/derivatives');
  $parameters = array(
    'paged_object_pid' => drush_get_option('paged_object_pid', NULL),
  );

  $parameters = array_filter($parameters);

  $paged_content = islandora_object_load($parameters['paged_object_pid']);
  if (!$paged_content) {
    drush_print('Object ' . $parameters['paged_object_pid'] . ' does not exist');
    return;
  }
  islandora_job_submit_background('upitt_workflow_generate_ocr_datastreams', $paged_content->id, array());
}
